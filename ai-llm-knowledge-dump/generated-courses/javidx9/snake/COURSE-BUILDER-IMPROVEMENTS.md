# COURSE-BUILDER-IMPROVEMENTS.md
# Snake Course — Deviations, Upgrades, and Retrospective

This document has two parts:

1. **Part I — Engineering Deviations** — Every deliberate deviation from the
   reference source and why it makes the course better. Written for students who
   want to understand the engineering decisions behind the course structure.

2. **Part II — Process Retrospective** — Every issue that surfaced during the
   build and post-build phases, analyzed at a generic/meta level, with
   mitigations that apply to any future course. Not specific to Snake —
   applicable to any game course built with this framework.

---


## Summary Table — Part I: Engineering Deviations from Reference Source

| # | Area | Reference Source | Course Upgrade | Lesson |
|---|------|-----------------|----------------|--------|
| 1 | Input model | Single `GameInput`; 1-arg `prepare_input_frame` | Double-buffered `inputs[2]`; 2-arg `prepare_input_frame` copies `ended_down` | 03 |
| 2 | Input union | No union; cannot loop over buttons | `union { buttons[BUTTON_COUNT]; struct { turn_left, turn_right; }; }` | 03 |
| 3 | Game phase | `int game_over` boolean | `GAME_PHASE` enum + `switch` dispatch | 04 |
| 4 | Backbuffer | `{pixels, width, height}` — no pitch | `{pixels, width, height, pitch}` — teaches stride | 01 |
| 5 | Font system | `FONT_DIGITS[]`, `FONT_LETTERS[]`, `FONT_SPECIAL[]` + linear scan | `FONT_8X8[128][8]` indexed by ASCII char directly | 09 (Lesson 12) |
| 6 | Debug tools | No assertions | `DEBUG_TRAP()` / `ASSERT(expr)` under `#ifdef DEBUG` | 01 |
| 7 | Platform contract | 3 functions (no shutdown) | 4 functions: adds `platform_shutdown()` | 03 |
| 8 | Build system | Two separate scripts (`build_x11.sh`, `build_raylib.sh`) | Unified `build-dev.sh --backend=x11\|raylib` | 01 |
| 9 | Audio | No audio at all | Full procedural audio engine: square wave + fade + frequency slide + spatial pan; looping music sequencer; SOUND_GROW + SOUND_RESTART | 11 |
| 10 | Utils structure | All code in one file (`snake.c`) | Refactored to `utils/` (draw-shapes, draw-text, audio, math, backbuffer) | 12 |

---

## 1. Double-Buffered Input Model

### Reference source
```c
void prepare_input_frame(GameInput *input) {
    for (int i = 0; i < BUTTON_COUNT; i++) {
        input->buttons[i].half_transition_count = 0;
    }
}
```
Single `GameInput`.  Only resets `half_transition_count`.

### Course upgrade
```c
void prepare_input_frame(const GameInput *old_input, GameInput *current_input) {
    for (int i = 0; i < BUTTON_COUNT; i++) {
        current_input->buttons[i].ended_down = old_input->buttons[i].ended_down;
        current_input->buttons[i].half_transition_count = 0;
    }
    current_input->restart = 0;
    current_input->quit    = 0;
}
```
`GameInput inputs[2]` in `main()`.  `inputs[0]` = previous frame, `inputs[1]` = current.

### Why
X11 is **event-based**: you receive one `KeyPress` event when a key is pressed and one `KeyRelease` event when released.  If you only reset `half_transition_count` but not copy `ended_down`, then on the second frame the key appears "up" — because no new `KeyPress` event arrives while it's being held.

The two-arg version copies `ended_down` from the previous frame so `ended_down=1` persists across frames when a key is held.  This is correct for both X11 (event-based) and Raylib (poll-based).

### JS analogy
```js
// Manual input tracking — keeping held state across frames
const prevInput = { ...currentInput };   // copy ended_down
currentInput.halfTransitionCount = 0;     // reset per-frame events
// ... fill new events from DOM ...
```

---

## 2. GameInput Union for Button Array Access

### Reference source
```c
typedef struct {
    GameButtonState turn_left;
    GameButtonState turn_right;
} GameInput;
// Can't loop over buttons without manually indexing each field.
```

### Course upgrade
```c
#define BUTTON_COUNT 2
typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];  /* for loop access */
        struct {
            GameButtonState turn_left;   /* named access */
            GameButtonState turn_right;
        };
    };
    int restart;
    int quit;
} GameInput;
```

### Why
The `prepare_input_frame` upgrade requires iterating over all buttons in a loop.  Without the union, you'd need a switch or manually repeat the copy for each field — fragile if you add more buttons.

The union guarantees that `buttons[0]` and `turn_left` are the same memory — no out-of-sync risk.

### JS analogy
```ts
interface GameInput {
    buttons: ButtonState[];  // array access
    get turn_left(): ButtonState  { return this.buttons[0]; }  // named access
    get turn_right(): ButtonState { return this.buttons[1]; }
}
```

---

## 3. GAME_PHASE Enum vs `int game_over`

### Reference source
```c
typedef struct {
    int game_over;  // 0 = playing, 1 = dead
    ...
} GameState;

void snake_update(GameState *s, ...) {
    if (s->game_over) { ... }
    else { ... }
}
```

### Course upgrade
```c
typedef enum {
    GAME_PHASE_PLAYING   = 0,
    GAME_PHASE_GAME_OVER = 1,
} GAME_PHASE;

typedef struct {
    GAME_PHASE phase;
    ...
} GameState;

void snake_update(GameState *s, ...) {
    switch (s->phase) {
        case GAME_PHASE_PLAYING:   ... break;
        case GAME_PHASE_GAME_OVER: ... break;
    }
}
```

### Why
1. **Scales to more phases** — adding `GAME_PHASE_PAUSED` or `GAME_PHASE_MENU` is one enum value + one switch case.  With `int game_over` you'd add `int is_paused`, `int in_menu` — flag soup.
2. **Compiler catches missing cases** — `-Wswitch` warns if you add an enum value but forget to handle it in a switch.  An `if (game_over)` chain never warns.
3. **Self-documenting** — `phase == GAME_PHASE_PLAYING` is unambiguous; `game_over == 0` requires the reader to remember what 0 means.

### JS analogy
```js
const GAME_PHASE = Object.freeze({ PLAYING: 0, GAME_OVER: 1 });
switch (state.phase) {
    case GAME_PHASE.PLAYING:   updatePlaying(); break;
    case GAME_PHASE.GAME_OVER: updateGameOver(); break;
}
```

---

## 4. `pitch` Field in `SnakeBackbuffer`

### Reference source
```c
typedef struct {
    uint32_t *pixels;
    int width, height;
} Backbuffer;

// Pixel access: pixels[py * width + px]
```

### Course upgrade
```c
typedef struct {
    uint32_t *pixels;
    int width, height;
    int pitch;  /* bytes per row = width * 4 */
} SnakeBackbuffer;

// Pixel access: pixels[py * (pitch / 4) + px]
```

### Why
`pitch` (bytes per row) introduces an important concept: a row's byte stride is not always `width * bytes_per_pixel`.  Some graphics APIs (DirectX surface, Win32 DIB, Linux DRM) may add padding to align rows to 16/32/64 bytes.

By making `pitch` explicit in `SnakeBackbuffer` from the start, students learn to write pixel loops that work correctly when `pitch > width * bytes_per_pixel`.

In our implementation `pitch = width * 4` always, but the formula `py * (pitch/4) + px` is correct in the general case.

---

## 5. ASCII-Indexed Bitmap Font

### Reference source
Three separate lookup tables with a linear scan for special characters:
```c
static const uint8_t FONT_DIGITS[10][7][5] = { ... };
static const uint8_t FONT_LETTERS[26][7][5] = { ... };
static const uint8_t FONT_SPECIAL[N_SPECIAL][7][5] = { ... };

int find_special_char(char c) { /* linear scan */ }
```

### Course upgrade
```c
static const uint8_t FONT_8X8[128][8] = {
    [0 ... 31]  = {0,0,0,0,0,0,0,0},  /* control chars — blank */
    ['A'] = {0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0x00},
    ['B'] = {0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0, 0x00},
    /* ... */
};

// Lookup: O(1) direct array index by ASCII value
uint8_t *glyph = FONT_8X8[(int)c];
```

### Why
1. **O(1) lookup** — `FONT_8X8[(int)c]` vs linear scan `find_special_char()`.
2. **Single table** — no need to choose between three tables based on character category.
3. **C99 designated initializers** — `['A'] = {...}` is idiomatic, self-documenting, and safe against reordering.
4. **Extensible** — add any ASCII character by writing `['@'] = {...}`.  No special handling needed.

**Bit convention upgrade:**
- Reference: 5-wide glyphs, bit 4 = leftmost pixel (5-bit rows).
- Course: 8-wide glyphs, bit 7 = leftmost pixel (8-bit rows, `BIT7-left`).
- Conversion: `new_byte = old_5bit_byte << 3`.

---

## 6. DEBUG_TRAP / ASSERT Macros

### Reference source
No assertions.

### Course upgrade
```c
#ifdef DEBUG
  #define DEBUG_TRAP()  __builtin_trap()
  #define ASSERT(expr)  do { if (!(expr)) { DEBUG_TRAP(); } } while(0)
#else
  #define DEBUG_TRAP()
  #define ASSERT(expr)
#endif
```
Enabled with `-DDEBUG` flag in `build-dev.sh --debug`.

### Why
Students learning C need a way to catch invalid states early.  A random crash 10 frames later is much harder to debug than an assertion halt at the exact bad line.

`__builtin_trap()` causes the debugger to stop at the exact line that failed and show the full call stack — much more informative than `abort()` or `exit()`.

In release builds (no `-DDEBUG`) the macro is empty — zero runtime overhead.

---

## 7. `platform_shutdown` in the Contract

### Reference source
```c
void platform_init(...);
double platform_get_time(void);
void platform_get_input(GameInput *input);
// No shutdown function.
```

### Course upgrade
Adds a 4th function:
```c
void platform_shutdown(void);
```

### Why
Explicit resource cleanup at the end of `main()` makes the program's resource lifecycle visible and testable.  Without it:
- ALSA streams may not be flushed before exit (audio cutoff).
- Valgrind reports "still reachable" memory that's actually fine but noisy.
- Students don't see where to add cleanup code for new resources they add.

---

## 8. Unified `build-dev.sh`

### Reference source
Two separate scripts:
```
build_x11.sh
build_raylib.sh
```

### Course upgrade
One unified script:
```bash
./build-dev.sh --backend=x11
./build-dev.sh --backend=raylib
./build-dev.sh --backend=x11 --debug     # -DDEBUG + AddressSanitizer
./build-dev.sh --backend=raylib -r        # run after build
```

### Why
1. **Single entry point** — students learn one command; no confusion about which script to use.
2. **Shared flags** — debug/release flags, output path, and warning levels are consistent across backends.
3. **`-r` flag** — build + run in one command speeds up the edit-compile-run loop.

---

## 9. Procedural Audio Engine

### Reference source
No audio.

### Course upgrade
Full procedural PCM audio engine in `audio.c`:
- Square wave oscillator with phase accumulator.
- Fade-in envelope (~10% of duration) to prevent click at attack.
- Fade-out envelope (~10ms) to prevent click at release.
- Linear frequency slide (`frequency_slide` per sample).
- Stereo spatial pan based on food X position.
- `MAX_SIMULTANEOUS_SOUNDS=4` slot pool.

Sound effects:
- `SOUND_FOOD_EATEN`: 800 → 1200 Hz rising blip, 80ms — positive reinforcement.
- `SOUND_GAME_OVER`: 400 → 100 Hz descending tone, 500ms — "bad thing happened".

### Why
Audio is one of the most requested topics for a game programming course.  Including it teaches:
- The phase accumulator pattern (fundamental digital synthesis).
- How audio callbacks and latency models work (ALSA vs Raylib AudioStream).
- Stereo panning mathematics.
- How to mix multiple voices without clipping (clamp_sample).

It's the only part of the course that requires math.h — a deliberate choice to keep the rest of the codebase dependency-free.

---

## 10. `utils/` Refactor

### Reference source
All rendering code inline in `snake.c`.

### Course upgrade
Drawing utilities extracted to `utils/`:
```
utils/math.h          — MIN, MAX, CLAMP, ABS
utils/backbuffer.h    — SnakeBackbuffer, GAME_RGB/RGBA, COLOR_*
utils/draw-shapes.h/c — draw_rect, draw_rect_blend
utils/draw-text.h/c   — draw_char, draw_text, FONT_8X8
utils/audio.h         — AudioOutputBuffer, SOUND_ID, SoundInstance, GameAudioState
```

Game-specific drawing (`draw_cell`) stays in `game.c` — it's not a general utility.

### Pedagogical structure
The refactor is presented as a **lesson 12 activity**, not the initial state.  Lessons 01–10 show the inline versions, then lesson 12 shows the extraction.  This teaches:
1. Start with inline code to understand it.
2. Extract to a module when the pattern is clear.
3. The module boundary: reusable generic utilities vs game-specific drawing.

### JS analogy
```js
// Before: inline in game.js
function drawRect(x, y, w, h, color) { ... }

// After: extracted to utils/drawing.js
import { drawRect } from './utils/drawing.js';
```

---

## Conformances (What the Course Preserves from the Reference)

| # | Preserved behaviour |
|---|---------------------|
| 1 | `UPDATE_BUTTON` macro — identical semantics |
| 2 | Ring buffer mechanics (head/tail/length) |
| 3 | `grow_pending` += 5 per food item |
| 4 | `move_interval` starts at 0.15f, floor 0.05f |
| 5 | `best_score` preserved across `snake_init()` calls |
| 6 | Food placement via retry loop (not a free-cell map) |
| 7 | 1px inset per cell (gap between cells) |
| 8 | Wall border cells drawn as filled cells (not lines) |
| 9 | Direction delta tables `DX[4]` / `DY[4]` |
| 10 | Platform + game independence: game.c never includes X11 or Raylib headers |

---

# Part II — Process Retrospective

> Every issue listed here happened during the build or post-build phases of
> this course.  Each is analysed generically — the root cause and mitigation
> apply to any course built with this framework, not just Snake.

---

## Issue Index

| # | Category | Short description | Phase detected |
|---|----------|-------------------|----------------|
| R1 | Pixel format | Byte-order mismatch between backends → wrong colours | Post-build play |
| R2 | Platform protocol | Missing OS close-window protocol → terminal hang | Post-build play |
| R3 | Platform input API | Unreliable keysym lookup → Q/Esc keys not working | Post-build play |
| R4 | Audio subsystem | ALSA default `start_threshold` silences all audio | Post-build play |
| R5 | Audio subsystem | Audio callback approach passed wrong frame count | Post-build play |
| R6 | State reset | `memset` zeroed audio config → permanent silence after restart | Post-build play |
| R7 | File integrity | Incomplete rewrite left dead code tail → compile error | Build |
| R8 | Feature completeness | No music, no grow SFX, no restart jingle shipped initially | Post-build play |

---

## R1 — Pixel Format: Byte-Order Must Be Agreed Upon Before Writing Any Backend

### What happened
The `GAME_RGBA` macro was defined to produce a `uint32_t` whose integer value
was `0xAARRGGBB`.  On a little-endian CPU this stores bytes in memory as
`[BB, GG, RR, AA]` — i.e. BGRA order.  The X11 backend used `GL_BGRA`,
which matched.  The Raylib backend used `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`
(RGBA), which did not — red and blue were swapped, making the green snake
appear blue and the red food appear blue-green.

The alpha-blend function in `draw-shapes.c` also extracted channels using
the wrong shifts once the macro was corrected (R was extracted from bits 16–23
instead of bits 0–7).

### Root cause
The shared pixel format was defined once but validated against only one
backend.  The second backend was written later without re-checking the byte
layout end-to-end through the full display pipeline.

### Generic mitigation

1. **Define the canonical pixel format in one place with an explicit byte-order
   diagram** in the shared header, before writing any backend:
   ```c
   /* GAME_RGBA — bytes in memory on little-endian: [R][G][B][A]
    * uint32 value: 0xAABBGGRR (bits 0-7 = R, bits 8-15 = G, ...) */
   ```
2. **Add a format smoke test** — a frame that draws a known colour (e.g.
   pure red `GAME_RGB(255,0,0)`) and verifies it appears red, not blue,
   on every backend before writing any lesson that discusses colour.
3. **Any function that reads individual colour channels** (e.g. alpha blend)
   must extract them using the same bit positions as the macro that creates
   them.  Document those positions in the macro comment and in the channel
   extraction comment.
4. **Rule of thumb:** if two backends use different format enums
   (`GL_RGBA` vs `GL_BGRA`, `R8G8B8A8` vs `B8G8R8A8`), one of them is
   using the wrong enum for the actual byte layout — investigate immediately.

---

## R2 — Platform Protocol: Register Every OS Lifecycle Hook Before Writing the Main Loop

### What happened
The X11 window was created without registering the `WM_DELETE_WINDOW` protocol.
When the user clicked the window's close button, the window manager destroyed
the window while the game loop was still executing `glXSwapBuffers` on it.
`glXSwapBuffers` blocked forever — the terminal required `CTRL+C` to recover.

### Root cause
`WM_DELETE_WINDOW` is an opt-in protocol in X11.  Without it, the WM's default
behaviour is to forcibly destroy the window.  This is not covered by minimal
"open a window" tutorials and is easy to miss on the first pass.

### Generic mitigation

**Maintain a platform integration checklist** — one per target OS/windowing
system — that must be completed before the main loop is written.  For X11:

```
[ ] Window creation — XCreateWindow with correct event mask
[ ] Visual / colormap — matched to GLX visual
[ ] WM_DELETE_WINDOW — XInternAtom + XSetWMProtocols (prevents hang on close)
[ ] Auto-repeat fix — XkbSetDetectableAutoRepeat (prevents phantom key events)
[ ] GL context — glXCreateContext + glXMakeCurrent
[ ] ALSA handle — snd_pcm_open + hw_params + sw_params (see R3, R4)
[ ] Cleanup — every resource opened in init must have a matching close in shutdown
```

The checklist is not game-specific — it belongs in the course-builder framework
and is reused for every course targeting that platform.

---

## R3 — Platform Input API: Use the Most Reliable Variant of Every OS API

### What happened
The initial implementation used `XLookupKeysym(&ev.xkey, 0)` to translate X11
key events into keysyms.  When `XkbSetDetectableAutoRepeat` was active,
`XLookupKeysym` returned inconsistent results — the Q and Escape keysyms were
not recognized, so quitting with the keyboard was broken.

The fix was to replace it with `XkbKeycodeToKeysym(display, keycode, 0, 0)`,
which always returns the base unshifted keysym for the physical key regardless
of keyboard state.

### Root cause
`XLookupKeysym` is the older X11 API; `XkbKeycodeToKeysym` is the
XKB-aware replacement.  When auto-repeat detection is enabled (which sets up
XKB internally), the older API can produce inconsistent results.

### Generic mitigation

1. **For every OS API used in a course, prefer the most modern / most reliable
   variant**, not the most commonly copy-pasted example.  Older tutorials often
   demonstrate deprecated or fragile APIs.
2. **Test every key mapping interactively** before writing lessons about input.
   A key that "should work" but doesn't fails silently — it requires human
   eyes on the running program to catch.
3. **Document which API was chosen and why** in the source comment.  Future
   maintainers need to know why `XkbKeycodeToKeysym` was chosen over
   `XLookupKeysym` — without that note they might "simplify" it back.

---

## R4 — Audio Subsystem: Validate Audio Playback Before Writing Audio Lessons

### What happened — ALSA
ALSA's software parameters include `start_threshold`, which controls when
the PCM device begins playback.  The default value (loaded by
`snd_pcm_sw_params_current`) equals `buffer_size` (e.g. 4096 samples).
A game writing one period per frame (~1024 samples) never fills the buffer to
that threshold — ALSA waits forever and produces no sound.

No error is returned.  `snd_pcm_open`, `snd_pcm_hw_params`, and
`snd_pcm_prepare` all succeed.  The silence looks like correct operation.

The fix: call `snd_pcm_sw_params` with `start_threshold = 1` so playback
begins after the first write.

### What happened — Raylib
The initial implementation used `SetAudioStreamCallback` (a background-thread
callback).  The callback received `bufferCount` — the number of *stereo frames*
— but the code divided it by 2 assuming it was individual samples.  This halved
every requested frame count, producing corrupted (half-length) buffers.

The fix: remove the callback entirely and use the main-thread polling model
(`IsAudioStreamProcessed` + `UpdateAudioStream`).

### Root cause
Both issues stem from the same pattern: the audio subsystem was set up
procedurally and appeared to succeed, but was not verified by actually hearing
output.  "No compile error and no runtime error" was treated as "audio works."

### Generic mitigation

1. **Audio must be heard before any audio lesson is written.**  Play the game,
   trigger every sound event, and confirm audible output on every backend.
2. **ALSA integration checklist:**
   ```
   [ ] snd_pcm_hw_params — format, rate, channels, buffer_size, period_size
   [ ] snd_pcm_sw_params_current — MUST load before overriding any sw param
   [ ] snd_pcm_sw_params_set_start_threshold — set to 1 (not default buffer_size)
   [ ] snd_pcm_sw_params — MUST apply after every sw_params change
   [ ] snd_pcm_prepare — after all params are applied
   [ ] fill_audio per frame — check avail, write period, recover on EPIPE
   ```
3. **Raylib audio: prefer the main-thread polling model.**  `IsAudioStreamProcessed`
   + `UpdateAudioStream` is simpler, debuggable, and avoids all threading
   concerns.  Callbacks add complexity without benefit for a course project.
4. **When using a new audio API**, always write a minimal test: generate a
   440 Hz sine wave for 1 second and verify it plays before building the full
   audio engine on top.

---

## R5 — State Reset: Every `memset` Reset Function Needs an Explicit Preservation Contract

### What happened
`snake_init` used `memset(s, 0, sizeof(*s))` to reset all game state at both
startup and player restart.  The audio subsystem stored its configuration
(`master_volume`, `sfx_volume`, `samples_per_second`) inside `GameState`.
`game_audio_init` was called before `snake_init` in `main()` and set
`master_volume = 0.8` and `sfx_volume = 1.0`.

`snake_init`'s `memset` then zeroed both fields.

In `game_get_audio_samples`, the output scale factor is
`master_volume * 16000.0f`.  With `master_volume = 0.0f`, every output sample
was 0 — permanent silence, on every frame, both at startup and after every
restart.

No error is produced.  The audio fill loop ran correctly; it simply produced
zeroes.

The fix: save `master_volume` and `sfx_volume` before the `memset` and restore
them after, alongside `samples_per_second`.

### Root cause
The reset function did not have a documented contract about what it preserved.
`best_score` was already being preserved (it was saved and restored).
`samples_per_second` was added in a later session.  `master_volume` and
`sfx_volume` were never added because the audio system was added after the
initial reset pattern was established, and no one audited the preservation list.

### Generic mitigation

1. **Every state-reset function must carry an explicit preservation comment:**
   ```c
   /* Fields preserved across this reset:
    *   best_score               — carries over from previous game
    *   audio.samples_per_second — set by platform, never changes
    *   audio.master_volume      — set by game_audio_init, constant
    *   audio.sfx_volume         — set by game_audio_init, constant
    * Everything else is zeroed. */
   ```
2. **Categorize fields at declaration time** — mark each field in the state
   struct with one of:
   - `/* persists across reset */`
   - `/* reset each game */`
   - `/* set by platform, never reset */`
   This makes a future audit trivial: grep for `/* set by platform` to find
   everything that needs preserving.
3. **Any subsystem added after the initial reset pattern** (audio, analytics,
   settings) must explicitly add its config fields to the preservation list.
   The checklist should be reviewed whenever a new subsystem is introduced.
4. **The configuration that a subsystem needs** should either be:
   - Stored *outside* the resettable state struct (passed as a parameter each
     call), or
   - Stored *inside* the struct but explicitly preserved by the reset, or
   - Re-initialized *inside* the reset function itself.
   There is no fourth option.  Leaving it to chance produces silent bugs.

---

## R6 — Audio Event Ordering: Events That Reference Mutable State Must Fire After State Is Stable

### What happened
After adding SOUND_RESTART (a jingle when the player restarts), the initial
attempt queued the sound *before* calling `snake_init`.  `snake_init` called
`memset(s, 0, sizeof(*s))` which wiped `active_sounds[]`.  The restart jingle
was silently dropped every time.

The fix: play SOUND_RESTART *after* `snake_init` so it lands in the freshly
initialised pool.

### Root cause
The ordering constraint ("events must come after the state they depend on is
stable") was not considered.  It is the same class of bug as R5 (reset
ordering), but manifests as a "missing sound" rather than "silence."

### Generic mitigation

1. **Document the ordering contract for every reset function:**
   ```
   Before snake_init: nothing game-logic-dependent
   snake_init():      memset + restore preserved fields + subsystem init
   After snake_init:  safe to queue events, play sounds, spawn particles
   ```
2. **As a general rule:** if a function calls `memset` on the containing struct,
   nothing that depends on that struct's contents should be called before it.
3. **Fire-and-forget events** (play sound, spawn particle, log event) should
   always be the *last* thing in a block, after all state transitions are complete.

---

## R7 — File Edit Integrity: Verify Output Files After Large Rewrites

### What happened
A large rewrite of `main_raylib.c` left the old file's tail (hundreds of lines
including a second `main()` function and a block comment with Unicode box-drawing
characters) appended after the new content.  The result compiled on the first
attempt because the second `main` was inside a block comment — but a subsequent
edit exposed the Unicode characters, which `clang` rejected as "unexpected
character."

### Root cause
The rewrite was applied as an edit (new content inserted, old content not
fully removed).  The verification step (check that the file ends at the expected
location) was skipped.

### Generic mitigation

1. **After any full-file rewrite, verify the file ends at the expected boundary:**
   - Check the last 10 lines with `tail -10 file.c`.
   - Check the line count with `wc -l file.c` and compare to expectation.
2. **Build both backends immediately after any platform file change.**
   A change to `main_raylib.c` must be followed by `build-dev.sh --backend=raylib`,
   not just the backend that was already working.
3. **Unicode characters in comments are a latent hazard.**  All box-drawing
   characters (`─`, `═`, `│`) in block comments will cause `clang` to reject
   the file if they end up outside a valid comment block.  Prefer ASCII box
   characters (`-`, `=`, `|`) in source files, or at least verify the file
   is valid C after inserting any content containing non-ASCII bytes.

---

## R8 — Feature Completeness: Verify All Interactive Features Before Writing Lessons

### What happened
The course shipped with no background music, no grow SFX, and no restart jingle.
These were added only after the user played the game and reported missing audio.
Lesson 11 (audio) was written before the full audio feature set was finalized —
it described `SOUND_FOOD_EATEN` and `SOUND_GAME_OVER` but made no mention of
music or per-step growth feedback.

### Root cause
"Audio works" was assessed by checking that sound effects played.  The question
"what audio experiences should this game have?" was not asked before writing the
audio lesson — the lesson was written to match what was implemented, not what
was desirable.

### Generic mitigation

1. **Before writing any lesson, play the game as a player, not as a developer.**
   Ask: "Is this enjoyable?  Is there audio feedback for every meaningful game
   event?  Does anything feel missing?"  Fix the experience first; write the
   lesson second.
2. **Define a feature completeness checklist per game before implementation:**
   ```
   Audio:
     [ ] Background music (looping during play)
     [ ] Sound on every player-caused event (eat, grow, turn, restart)
     [ ] Sound on every game-triggered event (game over, speed up)
     [ ] Music stops/starts correctly with game phase transitions
   ```
3. **Lessons should describe the finished system, not the minimum viable system.**
   A lesson about audio that doesn't mention music when the game has music will
   confuse students who can hear it but can't find the code for it.
4. **The audio design table** (sound name → trigger → description) should be
   written in the lesson *before* the sounds are implemented.  This surfaces
   missing sounds at design time, not after the lesson is done.

---

## Summary: Course Quality Gates

These gates should be verified in order before any lesson is written for a
new course.  Each gate corresponds to one or more retrospective items above.

| Gate | Verification | Retrospective items |
|------|-------------|---------------------|
| **G1. Pixel format agreement** | Draw a known colour (pure red) and verify it appears correctly on ALL backends simultaneously | R1 |
| **G2. Platform lifecycle complete** | Window opens, resizes, and closes cleanly without terminal hang or error on ALL platforms | R2 |
| **G3. Input complete** | Every mapped key is tested manually on ALL platforms; quit/restart work | R3 |
| **G4. Audio audible** | Every sound event is triggered and heard on ALL backends; no silent channels | R4, R5 |
| **G5. State reset audit** | Every field in the resettable state struct is categorized (persists / resets / platform); reset function comment lists every preserved field | R5, R6 |
| **G6. Build clean** | Both backends build with zero warnings on the compiler flags used for lessons | R7 |
| **G7. Feature completeness** | A design table listing every audio/visual event exists and every entry is implemented | R8 |
| **G8. Interactive play session** | At least one full play session (start → play → die → restart → play → quit) completed on each backend before any lesson is written | R2, R3, R4, R8 |

> **Rule:** A course is not ready for lesson writing until all eight gates are green.

