# Course Builder Prompt

Use this file with the Copilot CLI agent to convert a source file or project into a build-along course.

**Reference Implementation:** `games/tetris/` — A complete example following these patterns.

---

## How to use

Attach this file + the source file(s) or directory you want to convert, then say:

> "Build a course from @path/to/source based on @ai-llm-knowledge-dump/prompt/course-builder.md and output it to @path/to/course/"

---

## Student profile

The target student is a JavaScript/web developer learning C and low-level game programming.

### JavaScript → C mental model

| JavaScript / Web        | C equivalent                              | Key difference                                    |
| ----------------------- | ----------------------------------------- | ------------------------------------------------- |
| `let obj = {}`          | `typedef struct { ... } T;`               | Fixed layout in memory; no GC                     |
| `array.push(x)`         | Fixed array + counter + bounds check      | No dynamic resize — pre-allocate the max you need |
| `addEventListener()`    | `while (XPending(...))` / poll each frame | No event system; you build it in the game loop    |
| `requestAnimationFrame` | Platform loop + `delta_time`              | You own the loop; no browser scheduler            |
| `event.key === 'a'`     | `case XK_a: UPDATE_BUTTON(...)`           | Platform-specific keysyms; map them yourself      |
| Module imports          | `#include` + forward declarations         | Compile-time text paste; order matters            |
| `console.log()`         | `printf()` / `fprintf(stderr, ...)`       | No inspector; use printf or a debugger            |
| `undefined` / `null`    | Uninitialized memory / `NULL`             | C doesn't zero-init — always `memset` or `= {0}`  |
| `===` strict equal      | `==` (always strict in C)                 | No type coercion                                  |
| Closures                | Passing state via pointers                | Functions don't capture — thread state explicitly |
| `async/await`           | Manual state machine across frames        | No coroutines; split work across frames yourself  |
| Garbage collector       | `free()` or stack lifetime                | You own memory; leaks crash eventually            |
| Prototype chain         | Flat structs with explicit fields         | No inheritance; compose with embedding            |

**Teaching principle:** anchor every new C concept to the nearest JS equivalent, then explain what C does differently and why. Every comment should explain _why_, not just _what_.

---

## What to build

### Step 1 — Analyze & Plan

1. Read the source file(s) thoroughly.
2. Read `@ai-llm-knowledge-dump/llm.txt` to understand the student's background.
3. Build a **full topic inventory** before deciding on lesson count:
   - List every source file in the project.
   - For each file, list every function, struct, macro, constant, and algorithm it contains.
   - Mark each item with its conceptual category (e.g., _platform init_, _input_, _audio_, _rendering_, _game logic_, _state machine_).
   - This inventory is the raw material. **Every item in it must appear in at least one lesson.** Nothing is omitted because it seems obvious.
4. Determine lesson count from the inventory, not from a preset number:
   - Group related items from the inventory into **lesson candidates** following concept dependencies (nothing can be used before it is taught).
   - Each candidate that introduces more than 3 new concepts must be **split**.
   - Each candidate that is too small to produce a visible result must be **merged** with an adjacent candidate.
   - The resulting list of candidates is the lesson plan. The count is whatever the material requires — 5 lessons and 40 lessons are both valid outcomes. Do not compress lessons to meet a target count; do not pad them to reach one.
5. Identify the natural **build progression**: what is the smallest runnable first step? What do you add next? What are the logical milestones?
6. Write a `PLAN.md` next to the source file(s) covering:
   - What the program does overall
   - The full topic inventory (source file → function/struct/concept list)
   - The proposed lesson sequence — for **each lesson**, include all of the following:
     - Lesson number and title
     - What gets built (one sentence stating the concrete addition)
     - What the student sees or hears at the end (observable outcome)
     - New concepts introduced (≤ 3 per lesson — if more are needed, split the lesson)
     - Which files are created or modified
   - A **concept dependency map**: a flat list of every C/architecture concept the course introduces, with an arrow showing which concepts must come first. Example:
     ```
     backbuffer → draw_rect → game_render
     platform_init → game loop → delta_time → RepeatInterval
     GameInput → double-buffered input → prepare_input_frame
     GAME_PHASE → change_phase → audio transitions
     ```
   - A **per-lesson skill inventory table** — one row per lesson, columns: Lesson | New concepts | Concepts re-used from prior lessons. This table is the primary tool for catching gaps: if a lesson re-uses a concept not yet in prior rows, the lesson plan has a gap. Every item from the topic inventory must appear in exactly one "New concepts" cell.
   - The planned file structure for the course output, including which new files appear in each lesson
   - A note for every place the course deviates from the standard layout (see Step 2) and why
7. **Do not start writing lessons until PLAN.md exists, the topic inventory is complete, and every item in the inventory maps to a lesson row in the skill inventory table.**

---

### Step 2 — Build the course files (Recommended file layout)

The layout is described in two stages. Start with Stage A; migrate to Stage B when the trigger conditions are met. Document any deviation in `PLAN.md`.

**Stage A — Starter layout** (single backend or small game)

```
game-name/
├── build-dev.sh              # Build script with backend selection
├── src/
│   ├── utils/
│   │   ├── backbuffer.h      # Backbuffer type and color macros
│   │   ├── draw-shapes.c     # Rectangle, line primitives
│   │   ├── draw-shapes.h
│   │   ├── draw-text.c       # Bitmap font rendering
│   │   ├── draw-text.h
│   │   ├── audio.h           # AudioOutputBuffer; SoundInstance; ToneGenerator
│   │   ├── math.h            # MIN, MAX, CLAMP macros
│   │   └── ...               # any other reusable helper
│   ├── game/
│   │   ├── main.c            # game_init(), game_update(), game_render()
│   │   ├── main.h            # Game types, state, public API
│   │   ├── audio.c           # game_get_audio_samples(), game_audio_update()
│   │   ├── audio.h           # GameAudioState, SOUND_ID enum
│   │   ├── base.h            # Shared game types (GameInput, GameState, etc.)
│   │   └── levels.c          # Level/map data (omit if no levels)
│   ├── platform.h            # Platform contract (visible to all layers)
│   ├── main_x11.c            # X11/OpenGL backend
│   └── main_raylib.c         # Raylib backend
└── build/                    # Build output (gitignored)
```

**Stage B — Evolved layout** (two backends with platform-specific internals)

When a backend grows beyond a single file — e.g., it needs its own audio driver, a global state struct, or OS-specific helpers — move it into its own subdirectory:

```
game-name/
├── build-dev.sh
├── src/
│   ├── utils/                # (same as Stage A)
│   ├── game/
│   │   ├── main.c            # game_init(), game_update(), game_render()
│   │   ├── main.h
│   │   ├── audio.c
│   │   ├── audio.h
│   │   └── base.h
│   ├── platforms/
│   │   ├── x11/
│   │   │   ├── main.c        # platform_init(), main loop, input, display
│   │   │   ├── audio.c       # ALSA init, write, shutdown
│   │   │   ├── audio.h       # X11-private audio API (not in platform.h)
│   │   │   ├── base.c        # Definition: X11State g_x11 = {0};
│   │   │   └── base.h        # X11State struct + extern declaration
│   │   └── raylib/
│   │       └── main.c        # platform_init(), main loop, audio stream
│   └── platform.h            # Shared platform contract
└── build/
```

**Trigger conditions for moving to Stage B:**

- A backend needs its own audio driver or hardware-parameter setup (e.g., ALSA)
- A backend needs a private global state struct that must not be visible to the game layer
- A backend file exceeds ~300 lines and has a clean subsystem boundary (e.g., audio vs. windowing)

**Why `utils/`?** Drawing primitives and math helpers are reusable across games. Keep them separate from game-specific logic.

**Why `game/` subdirectory?** Groups all game-logic files — state, audio, rendering — away from platform and utility code. As the game grows this boundary prevents accidental coupling.

**Platform-private state.** Each backend owns a single file-scope global struct that is never included by the game layer or `platform.h`. The game layer communicates with it only through functions declared in `platform.h`.

```c
/* platforms/x11/base.h  — included ONLY by x11/*.c files */
typedef struct {
  Display    *display;
  Window      window;
  GLXContext  gl_context;
  int         is_running;
  X11Audio    audio;          /* ALSA handle + config */
  /* ... */
} X11State;

extern X11State g_x11;       /* defined once in base.c */
```

```c
/* platforms/x11/base.c */
X11State g_x11 = {0};        /* zero-initialised; owns all x11-layer state */
```

The Raylib backend follows the same pattern with `RaylibState g_raylib`. Neither struct is ever visible to `game/` or `utils/`.

> **Why?** The alternative is passing a `void *platform_context` pointer through every function call, which adds noise with no benefit when there is exactly one instance. A file-scope global is idiomatic C for this pattern and clearly scoped by the compilation unit.

> ⚠ **Never re-export a platform-private header from `platform.h`.** If `platform.h` includes `platforms/x11/base.h`, the game layer gains sight of X11 types — a layer violation. Keep each backend's `base.h` strictly within its own subdirectory.

> ⚠ **Only one rule is non-negotiable about layout:** reusable helpers (drawing, math, audio types) belong in `utils/`; never at `src/` root or inside `game/`. Everything else — file names, subdirectory depth, splitting criteria — is a judgement call. Document your choices in `PLAN.md`.

**Reference implementations:** `games/tetris/` uses Stage A; `games/snake/` uses Stage B. Both are valid; choose based on your game's complexity.

**Game header evolution:** `main.h` (or `game.h` in Stage A) will grow substantially across lessons — from ~10 lines in lesson 1 to 300+ by the final lesson. Strategy:

- Early lessons (1–3): show the complete header (it's short)
- Middle lessons: show only the changed struct with a before/after comment block
- Final lesson: show the complete file again as a definitive reference

Never ask the student to mentally merge two partial listings. If a header change is small, show the surrounding struct; if it touches many places, show the whole file.

**Do not start writing lessons until PLAN.md exists.**

---

## Build script

Use a build script with backend selection and run flags.

**Reference:** `games/tetris/build-dev.sh`

**Key features to include:**

- `--backend=x11|raylib` flag for selecting platform backend
- `-r` or `--run` flag to run the binary after a successful build
- Separate `SOURCES` variable for common game files
- Backend-specific source and library additions via `case` statement

**Usage pattern:**

```bash
./build-dev.sh                      # Build with default backend (raylib)
./build-dev.sh --backend=x11        # Build with X11
./build-dev.sh --backend=raylib -r  # Build and run
```

> ⚠ **`build-dev.sh` mandatory conventions (deviating from these causes confusing bugs):**
>
> | Convention       | Rule                                                                                                |
> | ---------------- | --------------------------------------------------------------------------------------------------- |
> | Output path      | Always `./build/game` — never per-backend names like `game_x11`                                     |
> | Build mode       | **Always debug**: `-O0 -g -DDEBUG -fsanitize=address,undefined` — no release mode in the dev script |
> | `-r` flag        | Means **run after build** — not "release mode"; never repurpose it                                  |
> | `mkdir -p build` | Must appear in the script **before** the compile command                                            |
> | Default backend  | **raylib** — matches the course-builder reference                                                   |
>
> When the dev script has a release mode it tempts the student to build without sanitizers. When `-r` means "release" the most natural flag has the wrong meaning. Keep it simple: one mode (debug), one flag meaning (run).

**Build command progression:** Lesson 1 shows a minimal manual `clang` command for one source file. Introduce `build-dev.sh` no later than the first lesson that adds a second `.c` file (adjust the lesson number to match your plan; the example below uses lesson 4–5 as a reference point). Document the flag breakdown table each time the compile command grows. Students who see the command expand without explanation assume something broke.

```
Lesson 01 (example): clang src/main_raylib.c -lraylib -lm -o build/game
Lesson 04 (example): clang src/game.c src/main_raylib.c -lraylib -lm -o build/game
Lesson 05 (example): ./build-dev.sh --backend=raylib   # replaces manual command
```

> These are illustration numbers only. In a course with a larger topic inventory the same milestones may fall at Lesson 6, 8, or 10. Use whichever lesson number your skill inventory table assigns to "second `.c` file added".

**Asset paths — bake at compile time:** For games that load files from disk (sprites, fonts, maps), bake the assets directory path into the binary using a `-D` flag rather than relying on the working directory:

```bash
# In build-dev.sh:
-DASSETS_DIR="\"$(pwd)/assets\""
```

```c
/* In game.c or main_*.c: */
#ifndef ASSETS_DIR
#define ASSETS_DIR "./assets"  /* fallback for manual builds */
#endif
LoadTexture(ASSETS_DIR "/frog.png");
```

> ⚠ **With a baked path the binary must be run from the directory where `build-dev.sh` was executed.** A production build would use `$XDG_DATA_DIR` or a config file. Document this constraint clearly in the first lesson that loads an asset.

---

## Source file rules

The source code must be:

- Written in **C** (not C++), even if the original is C++
- Split into three layers:
  - **Game logic** (`game/main.c`, `game/audio.c`, etc.) — all update, render, and draw calls; never calls OS-specific or windowing APIs
  - **Shared header** (`game/main.h` / `game/base.h`) — types, structs, enums, public function declarations
  - **Platform backends** (`platforms/x11/`, `platforms/raylib/`, or `main_x11.c` / `main_raylib.c`) — one compilation unit per platform; owns all OS-specific code
- Connected via a **`platform.h` contract**
- Uses a **backbuffer pipeline**: game writes ARGB pixels into a `uint32_t *` array; platform uploads that array to the GPU each frame
- Buildable with `clang` using the provided build scripts
- Heavily commented — every non-obvious line gets a comment explaining the _why_

### What the game layer may and may not call

The boundary is **portability**, not dogma. The rule is: if the call would need a different implementation on a different OS, it belongs in the platform layer or behind a contract function. If the C standard library provides it and it works identically on Linux, macOS, and Windows, the game layer can call it directly.

| Category                | Examples                                      | Where it belongs    | Reason                               |
| ----------------------- | --------------------------------------------- | ------------------- | ------------------------------------ |
| OS windowing / GPU      | `XCreateWindow`, `glTexImage2D`, `InitWindow` | Platform layer only | Different API per OS/backend         |
| Audio hardware          | `snd_pcm_writei`, `UpdateAudioStream`         | Platform layer only | Different API per OS/backend         |
| Input hardware          | `XNextEvent`, `IsKeyDown`                     | Platform layer only | Different API per OS/backend         |
| Timing / clocks         | `clock_gettime`, `GetTime`                    | Platform layer only | POSIX vs Win32 vs Raylib             |
| Portable file I/O       | `fopen`, `fclose`, `fread`, `fprintf`         | Game layer ✅       | C standard; identical on all targets |
| Portable strings / math | `memset`, `snprintf`, `sinf`, `rand`          | Game layer ✅       | C standard; identical on all targets |
| Portable time seed      | `time(NULL)`                                  | Game layer ✅       | C standard; identical on all targets |

> ⚠ **File paths are still a platform concern.** Even if the game layer calls `fopen`, the path it uses should be either relative (safe for local builds) or injected by the platform layer. Never hardcode an absolute path like `/home/user/...` in game code. For save files the pattern is: game code calls `fopen("build/save.dat", "r")` with a well-known relative path; `build-dev.sh` ensures the binary is always run from the project root so the path resolves correctly. Document this constraint in the first lesson that reads or writes a file.

> **Teaching note for JS developers:** This mirrors the browser's same-origin model — the game layer is "sandboxed" from OS details, but standard library functions (like `Math`, `JSON`, `console`) are global and always available. `fopen` is the C equivalent of `localStorage` — it's always there; you don't need the platform to wrap it.

### Debug printf policy

Remove or `#ifdef DEBUG`-guard any `printf`/`fprintf` that was present in the reference for development purposes. If you retain an `#include <stdio.h>` for potential student use, annotate it explicitly:

```c
#include <stdio.h>  /* Not used at runtime — available for student debugging */
```

Unexplained debug includes confuse students about what is "needed" vs. "leftover". When in doubt, guard with `#ifdef DEBUG` and add `-DDEBUG` to the debug build flags.

### Platform contract

**Reference:** `games/tetris/src/platform.h`, `games/snake/src/platform.h`

The minimal contract requires these functions:

```c
int    platform_init(PlatformGameProps *props);
double platform_get_time(void);
void   platform_get_input(GameInput *input, PlatformGameProps *props);
void   platform_shutdown(void);
```

For engine integration, wrap initialization state in `PlatformGameProps`:

- Window title, width, height
- Backbuffer struct
- `is_running` flag

**Look for in the references:**

- `platform_game_props_init()` — allocates backbuffer, sets dimensions
- `platform_game_props_free()` — frees backbuffer
- How each backend implements the contract differently while exposing the same function signatures

**Principle:** Keep the contract minimal. The game layer should never call platform-specific APIs directly. Expand the contract only when the game needs a service that genuinely varies by platform (timing source, audio callback trigger, etc.).

**Platform-private global state.** Each backend should declare a single file-scope global struct that lives entirely within the backend's compilation units. This struct holds all the OS handles, device handles, and configuration the platform layer needs to operate. It is never exposed through `platform.h` and is never `#include`d by any game-layer file.

In the Stage A layout (flat `src/`) this is a `static` global inside `main_x11.c` or `main_raylib.c`. In the Stage B layout (subdirectories) it gets its own `base.h` / `base.c` pair so multiple backend files can share it:

```c
/* platforms/x11/base.h — included ONLY within platforms/x11/ */
typedef struct {
  Display    *display;
  Window      window;
  GLXContext  gl_context;
  Atom        wm_delete_window;
  int         width, height;
  bool        vsync_enabled;
  GLuint      texture_id;
  int         is_running;
  X11Audio    audio;         /* owns ALSA handle + config */
} X11State;

extern X11State g_x11;      /* definition lives in base.c */
```

```c
/* platforms/x11/base.c */
#include "base.h"
X11State g_x11 = {0};       /* zero-init; C guarantees all fields start at 0/NULL/false */
```

The Raylib backend mirrors this pattern with its own `RaylibState g_raylib`. Neither struct is ever visible to `game/` or `utils/`.

**Why a named global rather than a pointer parameter?** With exactly one platform instance, passing `PlatformState *ctx` through every function adds noise without benefit. A file-scope global is idiomatic for this pattern in C and makes the ownership clear: the struct lives in the backend's translation unit, no one else can touch it.

> ⚠ **Always read `game/base.h` (or `game.h`) for the `GameInput` struct field names before writing a backend.** The field names (`turn_left`, `move_up`, etc.) are the canonical source of truth — they differ between courses. Copying a backend from a previous game without reading the current `GameInput` struct causes silent mismatches where the wrong action fires for a given key, or a key does nothing.

---

## Architectural patterns

### Backbuffer architecture (mandatory)

**Reference:** `games/tetris/src/utils/backbuffer.h`

```
┌──────────────────────────────────────────┐
│  main_x11.c / main_raylib.c              │
│  platform_init → loop:                   │
│    platform_get_input                    │
│    game_update(state, input, dt)         │
│    game_render(backbuffer, state)        │
│    upload backbuffer to GPU              │
└──────────────────────────────────────────┘
                  ↓ Backbuffer *
┌──────────────────────────────────────────┐
│  game.c                                  │
│  game_render writes ARGB pixels          │
│  draw_rect / draw_rect_blend             │
│  draw_text / draw_char (bitmap font)     │
└──────────────────────────────────────────┘
```

**Look for in the reference:**

- `Backbuffer` struct with `pixels`, `width`, `height`, `pitch`
- `GAME_RGB()` and `GAME_RGBA()` macros for color packing
- How `game_render()` writes pixels, platform uploads them

**Why?** Game logic is 100% platform-independent. Adding a new backend only requires a new `main_*.c`.

#### `pitch` field — why stride ≠ width × bpp

Some graphics APIs (DirectX surfaces, Win32 DIBs, Linux DRM) pad rows to a power-of-two byte alignment. If you use `width * 4` as the row stride everywhere and the platform later introduces padding, every row is off by the pad amount — a visually obvious diagonal shear.

Define `pitch = width * sizeof(uint32_t)` in your backbuffer and write pixel loops using it:

```c
/* Correct — works whether or not pitch == width*4 */
bb->pixels[py * (bb->pitch / 4) + px] = color;

/* Fragile — breaks if pitch ever differs from width*4 */
bb->pixels[py * bb->width + px] = color;
```

In our simple games `pitch = width * 4` always, but teaching the formula now means the student will never introduce a stride bug when they encounter a real API.

### Color system (mandatory)

**Reference:** `games/tetris/src/utils/backbuffer.h` for macros, `games/tetris/src/game.h` for constants

**Pattern:**

```c
/* 0xAARRGGBB — alpha in high byte, blue in low byte */
#define GAME_RGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)
```

Pre-define named color constants in `game.h`:

- `COLOR_BLACK`, `COLOR_WHITE`, `COLOR_RED`, etc.
- Game-specific colors (e.g., `COLOR_CYAN` for Tetris I-piece)

> ⚠ **Validate against BOTH backends before writing any rendering code.** The `GAME_RGBA` macro produces a `uint32_t` with a specific byte order (`0xAARRGGBB`). On a little-endian CPU this stores bytes in memory as `[BB, GG, RR, AA]`. X11 typically needs `GL_BGRA`; Raylib typically needs `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` (RGBA). If only one backend is tested first, the second will silently swap red and blue. **Test early with a pure-red `draw_rect` on both backends before building any more rendering code.** If channels are swapped, fix the `GAME_RGBA` macro and the alpha-blend channel extraction at the same time.

> ⚠ **Raylib pixel-format guard — R↔B channel swap.** `0xAARRGGBB` is stored little-endian as bytes `[B, G, R, A]`; Raylib's `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` reads bytes as `[R, G, B, A]`, so red and blue are silently swapped — the game runs but all colours appear wrong. Two fixes:
>
> **Option A — swap R and B inside `platform_display_backbuffer` before/after `UpdateTexture`:**
>
> ```c
> /* Swap R↔B: 0xAARRGGBB → Raylib RGBA bytes [R,G,B,A] */
> uint32_t *px = backbuffer->pixels;
> for (int i = 0; i < W * H; i++) {
>     uint32_t c = px[i];
>     px[i] = (c & 0xFF00FF00u) | ((c >> 16) & 0xFFu) | ((c & 0xFFu) << 16);
> }
> UpdateTexture(tex, px);
> /* Swap back so game state is unchanged next frame (operation is self-inverse) */
> for (int i = 0; i < W * H; i++) {
>     uint32_t c = px[i];
>     px[i] = (c & 0xFF00FF00u) | ((c >> 16) & 0xFFu) | ((c & 0xFFu) << 16);
> }
> ```
>
> **Option B — emit `0xAABBGGRR` from `GAME_RGB` directly and drop the swap loop:**
>
> ```c
> /* In backbuffer.h — emit Raylib-native byte order */
> #define GAME_RGB(r,g,b)    (0xFF000000u | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(r))
> #define GAME_RGBA(r,g,b,a) (((uint32_t)(a)<<24) | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(r))
> ```
>
> Pick one option in lesson 1. Document the byte-order choice in a comment next to the macro. Never mix both approaches.

### Delta-time game loop (mandatory)

**Reference:** `games/tetris/src/main_x11.c` (main loop), `games/tetris/src/main_raylib.c` (main loop)

**Pattern:**

```c
double prev_time = platform_get_time();
while (running) {
    double curr_time = platform_get_time();
    float delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;
    if (delta_time > 0.1f) delta_time = 0.1f;  /* cap for debugger pauses */

    prepare_input_frame(old_input, current_input);
    platform_get_input(current_input, &props);
    game_update(&state, current_input, delta_time);
    game_render(&backbuffer, &state);
    /* platform uploads backbuffer */

    /* swap input buffers */
    current_index = 1 - current_index;
}
```

**Game-specific considerations:**

- Tetris: Uses `delta_time` for drop timers, auto-repeat
- Physics games: May need fixed timestep with accumulator
- Turn-based: `delta_time` still useful for animations

### VSync with manual fallback

**Reference:** `games/tetris/src/main_x11.c` — `setup_vsync()` function

**Look for:**

- VSync extension detection (`GLX_EXT_swap_control`, `GLX_MESA_swap_control`)
- Fallback flag: `g_x11.vsync_enabled`
- Manual frame limiting when VSync unavailable:
  ```c
  if (!g_x11.vsync_enabled) {
      double frame_time = get_time() - current_time;
      if (frame_time < g_target_frame_time) {
          sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
      }
  }
  ```

**Raylib:** Handles this internally with `SetTargetFPS(60)`.

### Letterbox centering (mandatory from lesson 1)

For any game with a fixed logical canvas size, implement letterbox centering in the **very first** rendering lesson. Adding it retroactively requires touching three files across both backends and rewriting several lesson code blocks.

**Why:** The logical canvas (e.g., 1024×640) rarely matches the physical window after a resize. Letterboxing scales the canvas uniformly to fit — black bars appear top/bottom or left/right to fill the gap.

**X11 — two approaches:**

_Option A — lock window to canvas size with `XSizeHints` (simplest for single-canvas games):_

```c
/* After XCreateWindow() — prevents resize entirely; mouse is always 1:1 */
XSizeHints hints = {0};
hints.flags      = PMinSize | PMaxSize;
hints.min_width  = hints.max_width  = GAME_W;
hints.min_height = hints.max_height = GAME_H;
XSetWMNormalHints(display, window, &hints);
```

No `ConfigureNotify` handling, no scale math, no letterbox offset needed. Use this for games where resizing adds no value.

_Option B — resizable window with `ConfigureNotify` + `glViewport` (when resizing is required):_

```c
/* Handle ConfigureNotify events to update stored window size: */
g_win_w = new_width;
g_win_h = new_height;

/* In display_backbuffer — called each frame: */
float scale = (float)g_win_w / GAME_W;
if ((float)g_win_h / GAME_H < scale) scale = (float)g_win_h / GAME_H;
int vw = (int)(GAME_W * scale), vh = (int)(GAME_H * scale);
int vx = (g_win_w - vw) / 2,   vy = (g_win_h - vh) / 2;
glViewport(0, 0, g_win_w, g_win_h);
glClear(GL_COLOR_BUFFER_BIT);
glViewport(vx, vy, vw, vh);
/* draw fullscreen quad — letterbox bars are the cleared black area */
```

**Raylib — two approaches:**

_Option A — `DrawTexturePro` (when you need a `Rectangle dst` for other reasons):_

```c
/* At init: */
SetWindowState(FLAG_WINDOW_RESIZABLE);

/* Each frame: */
int sw = GetScreenWidth(), sh = GetScreenHeight();
float scale = (float)sw / GAME_W;
if ((float)sh / GAME_H < scale) scale = (float)sh / GAME_H;
int vw = (int)(GAME_W * scale), vh = (int)(GAME_H * scale);
Rectangle dst = { (sw - vw) / 2.0f, (sh - vh) / 2.0f, (float)vw, (float)vh };
ClearBackground(BLACK);
DrawTexturePro(tex, (Rectangle){0, 0, GAME_W, GAME_H}, dst, (Vector2){0,0}, 0, WHITE);
```

_Option B — `DrawTextureEx` (simpler for uniform scale):_

```c
float scale  = fminf((float)GetScreenWidth()  / (float)GAME_W,
                     (float)GetScreenHeight() / (float)GAME_H);
Vector2 offset = {
    ((float)GetScreenWidth()  - (float)GAME_W * scale) * 0.5f,
    ((float)GetScreenHeight() - (float)GAME_H * scale) * 0.5f
};
ClearBackground(BLACK);
DrawTextureEx(tex, offset, 0.0f, scale, WHITE);
```

Both Raylib options are correct; pick one per course and stay consistent.

> ⚠ **For mouse/cursor input, transform raw window coordinates to canvas coordinates using the letterbox scale and offset.** `GetMousePosition()` returns window pixels; after letterboxing the canvas is offset and scaled inside the window — raw mouse coordinates are wrong by the border amount.
>
> ```c
> /* Globals set every frame in platform_display_backbuffer: */
> static float g_scale;     /* min(win_w/GAME_W, win_h/GAME_H)     */
> static int   g_offset_x;  /* (win_w - GAME_W*scale) / 2          */
> static int   g_offset_y;  /* (win_h - GAME_H*scale) / 2          */
>
> /* platform_get_input — transform before writing to GameInput: */
> Vector2 pos = GetMousePosition();
> int cx = (int)((pos.x - (float)g_offset_x) / g_scale);
> int cy = (int)((pos.y - (float)g_offset_y) / g_scale);
> if (cx < 0) cx = 0;  if (cx >= GAME_W) cx = GAME_W - 1;
> if (cy < 0) cy = 0;  if (cy >= GAME_H) cy = GAME_H - 1;
> input->mouse_x = cx;
> input->mouse_y = cy;
> ```
>
> For X11, apply the same transform using the `vx`, `vy`, `scale` values computed in the `ConfigureNotify` / `display_backbuffer` path. Store them as globals alongside `g_win_w` / `g_win_h`.

---

## Input System (mandatory)

**Reference:** `games/tetris/src/game.h` (input types), `games/tetris/src/game.c` (`prepare_input_frame`, `handle_action_with_repeat`), `games/tetris/src/platform.h` (`platform_swap_input_buffers`)

### Double-Buffered Input

Why double-buffer? We need to know what happened **last frame** to detect transitions (pressed → held, held → released). A single buffer loses this history.

**Reference:** `games/tetris/src/main_raylib.c` (main loop), `games/tetris/src/main_x11.c` (main loop)

**Pattern (index-based swap):**

```c
/* In main(): */
GameInput inputs[2] = {0};
int current_index = 0;

while (running) {
    GameInput *current_input = &inputs[current_index];
    GameInput *old_input = &inputs[1 - current_index];

    prepare_input_frame(old_input, current_input);
    platform_get_input(current_input, &props);

    game_update(&state, current_input, delta_time);

    /* Swap by toggling index */
    current_index = 1 - current_index;
}
```

**Alternative (content swap):**

```c
/* In platform.h: */
static inline void platform_swap_input_buffers(GameInput *old_input,
                                               GameInput *current_input) {
  GameInput temp = *current_input;
  *current_input = *old_input;
  *old_input = temp;
}
```

**Warning:** Do NOT swap pointers — you must swap the actual contents:

```c
/* WRONG - swaps local pointer copies, not data! */
GameInput *temp = current_input;
*current_input = *old_input;  /* Copies data, but... */
*old_input = *temp;           /* temp is still old pointer! */

/* CORRECT - swaps contents */
GameInput temp = *current_input;  /* Copy CONTENTS to stack */
*current_input = *old_input;
*old_input = temp;
```

### Core Types

**Reference:** `games/tetris/src/game.h`

```c
typedef struct {
    int half_transition_count;  /* state changes this frame (0, 1, or 2) */
    int ended_down;             /* current state: 1=pressed, 0=released */
} GameButtonState;
```

**Why `half_transition_count`?**

- `0` = No change (button held or released entire frame)
- `1` = Changed once (normal press or release)
- `2` = Changed twice (pressed then released, or vice versa, within one frame)

At 30fps, each frame is 33ms. A quick tap might go down AND up within one frame.

**Button update macro:**

```c
#define UPDATE_BUTTON(button, is_down)                                         \
  do {                                                                         \
    if ((button).ended_down != (is_down)) {                                    \
      (button).half_transition_count++;                                        \
      (button).ended_down = (is_down);                                         \
    }                                                                          \
  } while (0)
```

**GameInput struct with union for iteration:**

```c
#define BUTTON_COUNT 4

typedef struct {
  union {
    GameButtonState buttons[BUTTON_COUNT];
    struct {
      GameButtonState move_left;
      GameButtonState move_right;
      GameButtonState move_down;
      GameButtonState rotate;
    };
  };
  int quit;
  int restart;
} GameInput;
```

**Why union?** Iterate with `buttons[i]` for bulk operations, access by name for readability.

### Preparing Each Frame

**Reference:** `games/tetris/src/game.c` — `prepare_input_frame()`

```c
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    /* CRITICAL: Preserve ended_down from last frame! */
    current_input->buttons[btn].ended_down = old_input->buttons[btn].ended_down;
    /* Reset transition count for new frame */
    current_input->buttons[btn].half_transition_count = 0;
  }
  /* Reset one-shot inputs */
  current_input->quit = 0;
  current_input->restart = 0;
}
```

**Why preserve `ended_down`?** For event-based input (X11), if no event fires this frame, the button state should remain unchanged. Copying from `old_input` ensures held keys stay held.

### Event-Based vs Polling Input

**Reference:** `games/tetris/src/main_x11.c` (event-based), `games/tetris/src/main_raylib.c` (polling)

| Platform | Model       | How it works                                                 |
| -------- | ----------- | ------------------------------------------------------------ |
| X11/ALSA | Event-based | `KeyPress` fires once on press, `KeyRelease` once on release |
| Raylib   | Polling     | `IsKeyDown()` returns true every frame while held            |

**X11 (events):**

> ⚠ **Use `XkbLookupKeySym`, not `XLookupKeysym`.** `XLookupKeysym(&event.xkey, 0)` returns the wrong keysym when CapsLock or NumLock is active — letters resolve to their shifted variant, Escape and other non-alpha keys may fail entirely. `XkbLookupKeySym` ignores modifier state and always returns the base keysym:
>
> ```c
> KeySym key;
> XkbLookupKeySym(display, event.xkey.keycode, 0, NULL, &key);
> ```

> ⚠ **Handle the WM_DELETE_WINDOW protocol.** Without this, clicking the window's close button sends a `ClientMessage` event that the default handler ignores — the window stays open and the process must be killed from the terminal. Register the protocol during init and check for it in the event loop:
>
> ```c
> /* After XCreateWindow(): */
> Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
> XSetWMProtocols(display, window, &wm_delete, 1);
>
> /* In the event loop: */
> case ClientMessage:
>     if ((Atom)event.xclient.data.l[0] == wm_delete)
>         props->is_running = 0;
>     break;
> ```

```c
while (XPending(display) > 0) {
    XNextEvent(display, &event);
    switch (event.type) {
    case KeyPress: {
        KeySym key;
        XkbLookupKeySym(display, event.xkey.keycode, 0, NULL, &key);
        switch (key) {
        case XK_Left:
        case XK_a:
            UPDATE_BUTTON(input->move_left, 1);
            break;
        /* ... other keys ... */
        }
        break;
    }
    case KeyRelease: {
        KeySym key;
        XkbLookupKeySym(display, event.xkey.keycode, 0, NULL, &key);
        switch (key) {
        case XK_Left:
        case XK_a:
            UPDATE_BUTTON(input->move_left, 0);
            break;
        /* ... other keys ... */
        }
        break;
    }
    case ClientMessage:
        if ((Atom)event.xclient.data.l[0] == wm_delete_window)
            props->is_running = 0;
        break;
    }
}
/* No event = no change. ended_down preserved from prepare_input_frame */
```

**Raylib (polling):**

```c
/* Called every frame regardless of events */
UPDATE_BUTTON(current_input->move_left, IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
UPDATE_BUTTON(current_input->move_right, IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
UPDATE_BUTTON(current_input->move_down, IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
```

**Why this matters:** With event-based input, `prepare_input_frame` MUST copy `ended_down` from the previous frame. With polling, you overwrite it every frame anyway—but the copy doesn't hurt and keeps the code consistent.

### Auto-Repeat (DAS/ARR)

**Reference:** `games/tetris/src/game.h` — `RepeatInterval`, `games/tetris/src/game.c` — `handle_action_with_repeat()`

DAS = Delayed Auto Shift (initial delay before repeating starts)
ARR = Auto Repeat Rate (speed of repeating after DAS)

```c
typedef struct {
  float timer;               /* Accumulates delta time */
  float initial_delay;       /* DAS: time before first repeat (e.g., 0.15s) */
  float interval;            /* ARR: time between repeats (e.g., 0.05s) */
  bool is_active;            /* Currently in repeat sequence? */
  bool passed_initial_delay; /* Have we triggered the first repeat? */
} RepeatInterval;
```

> ⚠ **Reference implementation warning:** The `games/tetris/` source mutates `repeat->initial_delay = 0.0f` to signal "entered ARR mode". This destroys the configuration value and prevents DAS from being reset correctly on key release. Use the `passed_initial_delay` bool flag instead — it records the phase transition without touching the config, so DAS behaviour is fully restored on release. Do not copy the mutation pattern.

**Key insight:** Repeat state lives in `GameState`, NOT in `GameInput`:

```c
/* In GameState: */
struct {
  RepeatInterval move_left;
  RepeatInterval move_right;
  RepeatInterval move_down;
} input_repeat;
```

**Why separate?** `GameInput` represents raw hardware state (what buttons are pressed). `RepeatInterval` represents game timing logic (when to trigger actions). Different concerns, different locations.

### Handle Action With Repeat

**Reference:** `games/tetris/src/game.c` — `handle_action_with_repeat()`

```c
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval *repeat,
                                      float delta_time,
                                      int *should_trigger) {
  *should_trigger = 0;

  /* Released - reset everything */
  if (!button->ended_down) {
    repeat->timer = 0.0f;
    repeat->is_active = false;
    repeat->passed_initial_delay = false;
    return;
  }

  /* Just pressed this frame */
  if (button->half_transition_count > 0) {
    repeat->timer = 0.0f;
    repeat->is_active = true;
    repeat->passed_initial_delay = (repeat->initial_delay <= 0.0f);

    /* Trigger immediately only if no initial delay */
    if (repeat->initial_delay <= 0.0f) {
      *should_trigger = 1;
    }
    return;
  }

  /* Held from previous frame */
  if (!repeat->is_active) {
    return;
  }

  repeat->timer += delta_time;

  if (!repeat->passed_initial_delay) {
    /* Waiting for initial delay (DAS) */
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger = 1;
      repeat->timer = 0.0f;
      repeat->passed_initial_delay = true;
    }
  } else {
    /* In repeat phase (ARR) */
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1;
      repeat->timer -= repeat->interval;  /* Keep remainder */
    }
  }
}
```

**Usage:**

```c
int should_move_left = 0;
handle_action_with_repeat(&input->move_left, &state->input_repeat.move_left,
                          delta_time, &should_move_left);
if (should_move_left && piece_can_move_left(state)) {
    state->piece.x--;
}
```

### Initialize Repeat Intervals

**Reference:** `games/tetris/src/game.c` — `game_init()`

```c
void game_init(GameState *state, GameInput *input) {
  /* ... other init ... */

  /* Movement: DAS=150ms, ARR=50ms */
  state->input_repeat.move_left = (RepeatInterval){
    .timer = 0.0f,
    .initial_delay = 0.15f,
    .interval = 0.05f,
    .is_active = false,
    .passed_initial_delay = false
  };

  state->input_repeat.move_right = (RepeatInterval){
    .timer = 0.0f,
    .initial_delay = 0.15f,
    .interval = 0.05f,
    .is_active = false,
    .passed_initial_delay = false
  };

  /* Soft drop: No delay, fast repeat */
  state->input_repeat.move_down = (RepeatInterval){
    .timer = 0.0f,
    .initial_delay = 0.0f,   /* Trigger immediately */
    .interval = 0.03f,       /* Fast repeat */
    .is_active = false,
    .passed_initial_delay = false
  };
}
```

### Timing Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│  KEY PRESSED                          KEY RELEASED                      │
│      │                                     │                            │
│      ▼                                     ▼                            │
│  ════╪═══════════════════════════════════════════════════════════════   │
│      │                                                                  │
│      │   initial_delay (DAS)   interval (ARR)                          │
│      │◄─────────────────────►│◄──────────►│◄──────────►│               │
│      │         150ms          │    50ms    │    50ms    │               │
│      │                        │            │            │               │
│      ▼                        ▼            ▼            ▼               │
│   [nothing]              [trigger]    [trigger]    [trigger]           │
│   (waiting)              (1st repeat) (2nd repeat) (3rd repeat)        │
│                                                                         │
│  For move_down (initial_delay=0):                                      │
│      │                                                                  │
│      │   interval   interval   interval                                │
│      │◄──────────►│◄──────────►│◄──────────►│                          │
│      │    30ms     │    30ms    │    30ms    │                          │
│      ▼             ▼            ▼            ▼                          │
│   [trigger]    [trigger]   [trigger]    [trigger]                      │
│   (immediate)                                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Common Input Bugs

| Symptom                                   | Cause                                               | Fix                                                                |
| ----------------------------------------- | --------------------------------------------------- | ------------------------------------------------------------------ |
| Held key acts like repeated presses       | `ended_down` not preserved in `prepare_input_frame` | Copy from `old_input` before resetting transitions                 |
| Key only works on first press (X11)       | Event-based input loses state between frames        | Ensure `prepare_input_frame` copies `ended_down`                   |
| Auto-repeat doesn't work                  | `passed_initial_delay` not tracked                  | Add flag to `RepeatInterval`, check in handler                     |
| Repeat triggers immediately despite delay | Early return when `half_transition_count > 0`       | Only trigger immediately if `initial_delay <= 0`                   |
| Input feels "sticky" after release        | Buffer swap not happening correctly                 | Verify swapping contents, not pointers                             |
| Down key doesn't repeat but left/right do | Different `initial_delay` values                    | Check initialization — `move_down` should have `initial_delay = 0` |

---

## Typed enums (mandatory)

**Reference:** `games/tetris/src/game.h` — all `typedef enum` declarations

**Pattern:**

```c
typedef enum { DIR_NONE, DIR_LEFT, DIR_RIGHT } MOVE_DIR;
void piece_move(GameState *state, MOVE_DIR dir);  /* self-documenting */
```

**Look for in the reference:**

- `TETROMINO_BY_IDX` — which piece type
- `TETRIS_FIELD_CELL` — cell contents (empty, wall, piece types)
- `TETROMINO_R_DIR` — rotation states
- `TETROMINO_ROTATE_X_VALUE` — rotation input direction

**Why?** Prevents passing wrong integer values. Makes function signatures readable.

---

## Debug trap macro (mandatory for debug builds)

**Why:** When an assertion fails, you want the debugger to break at the exact line. A null pointer dereference (`*(int*)0 = 0`) works but is undefined behavior. Platform-specific traps are cleaner.

**Pattern:**

```c
/* In a shared header like game.h or utils/debug.h */
#ifdef DEBUG
  #if defined(_MSC_VER)
    #define DEBUG_TRAP() __debugbreak()
  #elif defined(__GNUC__) || defined(__clang__)
    #define DEBUG_TRAP() __builtin_trap()
  #else
    #define DEBUG_TRAP() (*(volatile int *)0 = 0)
  #endif

  #define ASSERT(expr) do { if (!(expr)) { DEBUG_TRAP(); } } while(0)
  #define ASSERT_MSG(expr, msg) do { if (!(expr)) { fprintf(stderr, "ASSERT: %s\n", msg); DEBUG_TRAP(); } } while(0)
#else
  #define DEBUG_TRAP()
  #define ASSERT(expr)
  #define ASSERT_MSG(expr, msg)
#endif
```

**Usage:**

```c
ASSERT(player_x >= 0);
ASSERT_MSG(buffer != NULL, "Buffer was not allocated");
```

**Add `-DDEBUG` to debug builds in your build script.**

---

## Coordinate and unit systems

### When to use which system

| Game Type                                          | Coordinate System          | Unit System                                        | Default?    |
| -------------------------------------------------- | -------------------------- | -------------------------------------------------- | ----------- |
| Platformer, shooter                                | **Y-up** ← default         | **Meters** ← default; `PIXELS_PER_METER` to render | ✅ Use this |
| Physics / gravity game                             | Y-up (matches math)        | Meters (gravity = 9.8 m/s²)                        | ✅ Same     |
| RPG with scrolling                                 | Y-up + camera offset       | Meters + hierarchical for large worlds             | ✅ Same     |
| Infinite / procedural                              | Y-up + chunk + offset      | Chunk + sub-unit offset                            | ✅ Same     |
| Tetris, Breakout, tilemaps                         | Y-down (row 0 = top)       | Tiles / cells directly (no physics involved)       | Exception   |
| Falling sand, cellular automaton, pixel simulation | Y-down (pixel row 0 = top) | Raw pixels — grain `x`/`y` ARE pixel indices       | Exception   |

### Y-up vs Y-down

**Default: Y-up** for all games except pure grid/tile games. Y-up matches mathematical convention, standard physics intuition (gravity is −Y), and vector rotation math. The screen-coordinate flip from world Y-up to display Y-down happens **only in `game_render()`** — never in game logic.

**Use Y-down only when:** the game is grid-based (Tetris, Breakout, tile maps) and row 0 = top is the natural mental model, with no physics, angles, or rotation math involved.

**Pixel-grid exception — falling sand / cellular automata:** When the primary simulation domain is a 2D pixel grid where every particle's coordinates are direct indices into a flat `pixels[W * H]` array (grain IS a pixel), Y-down is correct throughout. Converting to Y-up would require `screen_y = H - grain.y` at every array access — pure noise with zero conceptual benefit. Gravity in these games is `+N px/s²` (positive = downward screen direction). Spatial queries like `IS_SOLID(x, y)` map directly to `pixels[y * W + x]`. Document this as an intentional exception in `PLAN.md` and in the first simulation lesson.

**Y-up pattern (default — platformers, shooters, anything with gravity or rotation):**

```c
/* Game logic: Y-up (0,0 = bottom-left, +Y = up) */
entity.vel_y -= 9.8f * dt;          /* gravity pulls down (negative Y) */
entity.pos_y += entity.vel_y * dt;  /* positive velocity = upward motion */

/* game_render() ONLY — Y-up to screen (Y-down) conversion */
int screen_y = (bb->height - 1) - (int)(entity.pos_y * PIXELS_PER_METER);
```

**Y-down pattern (grid games only):**

```c
/* Game logic: Y-down (0,0 = top-left, +Y = down) */
piece.y += 1;                        /* fall one row */
int screen_y = piece.y * CELL_SIZE;  /* 1:1 — no conversion needed */
```

**Rule:** State the convention in a comment at the top of `game.c` and at the top of `game_render()`. Never mix conventions within one game.

### Vector math: derive, don't guess

When computing velocity or direction vectors from an angle, always derive the result from the 2×2 rotation matrix — never guess the sign. Guessed signs produce subtle direction bugs that are hard to diagnose.

Convention example (Y-down, `θ = 0` = up, positive = clockwise):

```
Local "forward" vertex: (0, −1)
Rotation matrix R(θ) applied:
  dx = +sin(θ)
  dy = −cos(θ)
```

```c
/* Ship pointing at angle a (0 = up, clockwise positive in Y-down space) */
float dx =  sinf(a) * speed;  /* +sin from matrix — NOT −sin */
float dy = -cosf(a) * speed;  /* −cos from matrix — NOT +cos */
```

**Sanity test:** at `a = 0` the object must move in the known forward direction. If it moves backward, derive the fix from the matrix — not trial-and-error sign flipping. Never write `±sinf(a)` or `±cosf(a)` from memory.

State the coordinate convention and the "forward" direction in a comment at the top of whichever file contains the physics update.

### Unit system — world units (mandatory for non-grid games)

**Default: meters.** Use meters (or any named world unit) for all positions, velocities, dimensions, and physics constants. Convert to pixels only in `game_render()`. This makes physics intuitive, positions debuggable, and the game resolution-independent by changing a single constant.

```c
/* In game.h */
#define PIXELS_PER_METER 64.0f  /* visual scale: increase to zoom in, decrease to zoom out */

typedef struct {
    float x, y;          /* world position — meters from origin */
    float vel_x, vel_y;  /* velocity — meters/second */
    float width, height; /* dimensions — meters */
} Entity;

/* game.c — pure world units; no pixel numbers here */
entity.vel_y -= 9.8f * dt;           /* gravity: −9.8 m/s² */
entity.pos_y += entity.vel_y * dt;   /* Euler integration */

/* game_render() ONLY — convert to pixels at the last moment */
int px = (int)(entity.x      * PIXELS_PER_METER);
int py = (int)(entity.y      * PIXELS_PER_METER); /* + Y-flip for Y-up games */
int pw = (int)(entity.width  * PIXELS_PER_METER);
int ph = (int)(entity.height * PIXELS_PER_METER);
draw_rect(bb, px, py, pw, ph, color);
```

**Benefits:**

- Physics constants are real-world values: `vel = 5.0f` = 5 m/s — immediately readable in the debugger
- Adjust `PIXELS_PER_METER` to rescale the entire game without touching a line of game logic
- Easy to add a camera: subtract `camera.x * PIXELS_PER_METER` from the screen position

**Grid-based exception:** For pure grid games (Tetris, tile maps) the cell is the logical unit. Using `CELL_SIZE` directly in pixels is correct and simpler — no conversion layer needed.

**Reference:** `games/tetris/src/game.h` — `CELL_SIZE`, `FIELD_WIDTH`, `FIELD_HEIGHT` (grid exception)

---

## Drawing primitives

### Rectangle drawing

**Reference:** `games/tetris/src/utils/draw-shapes.c`

**Solid rectangle:**

```c
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
  /* Clip to backbuffer bounds */
  int x0 = (x < 0) ? 0 : x;
  int y0 = (y < 0) ? 0 : y;
  int x1 = (x + w > bb->width) ? bb->width : x + w;
  int y1 = (y + h > bb->height) ? bb->height : y + h;

  /* Use pitch/4, NOT width — pitch is bytes-per-row, which may differ from
   * width*4 if the platform adds row padding (common on OpenGL/DirectX).   */
  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);
    for (int px = x0; px < x1; px++) {
      row[px] = color;
    }
  }
}
```

**Blended rectangle (for overlays):**

```c
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
  int x0 = (x < 0) ? 0 : x;
  int y0 = (y < 0) ? 0 : y;
  int x1 = (x + w > bb->width) ? bb->width : x + w;
  int y1 = (y + h > bb->height) ? bb->height : y + h;

  uint32_t src_a = (color >> 24) & 0xFF;
  uint32_t src_r = (color >> 16) & 0xFF;
  uint32_t src_g = (color >> 8) & 0xFF;
  uint32_t src_b = color & 0xFF;
  uint32_t inv_a = 255 - src_a;

  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);  /* pitch/4, NOT width */
    for (int px = x0; px < x1; px++) {
      uint32_t dst = row[px];
      uint32_t dst_r = (dst >> 16) & 0xFF;
      uint32_t dst_g = (dst >> 8) & 0xFF;
      uint32_t dst_b = dst & 0xFF;

      uint32_t out_r = (src_r * src_a + dst_r * inv_a) / 255;
      uint32_t out_g = (src_g * src_a + dst_g * inv_a) / 255;
      uint32_t out_b = (src_b * src_a + dst_b * inv_a) / 255;

      row[px] = 0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
    }
  }
}
```

**Usage:**

```c
/* Solid red rectangle */
draw_rect(bb, 10, 10, 100, 50, COLOR_RED);

/* Semi-transparent black overlay for game over screen */
draw_rect_blend(bb, 0, 0, bb->width, bb->height, GAME_RGBA(0, 0, 0, 128));
```

### Bitmap font rendering

**Reference:** `games/tetris/src/utils/draw-text.c`, `games/tetris/src/utils/draw-text.h`

**Bit-endianness — document this before writing a single line of font code:**

```c
/* Bit 7 = leftmost pixel (BIT7-left convention) — use (0x80 >> col) to test */
/* Bit 0 = leftmost pixel (BIT0-left convention) — use (1 << col)    to test */
```

The bitmask in the renderer MUST match the font data's convention. **Test with the letter 'N' first** — its diagonal runs top-left → bottom-right. If it runs top-right → bottom-left, the mask direction is wrong.

**Pattern:** Embed a simple bitmap font as a static array. Each character is an 8x8 grid stored as 8 bytes (one byte per row, bits are pixels).

```c
/* 8x8 bitmap font - each char is 8 bytes */
static const uint8_t FONT_8X8[128][8] = {
  /* ASCII 32 (space) */
  [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  /* ASCII 65 'A' */
  ['A'] = {0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
  /* ... etc ... */
};

void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color, int scale) {
  if (c < 0 || c > 127) return;
  const uint8_t *glyph = FONT_8X8[(int)c];

  for (int row = 0; row < 8; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < 8; col++) {
      if (bits & (0x80 >> col)) {
        /* Draw scaled pixel */
        for (int sy = 0; sy < scale; sy++) {
          for (int sx = 0; sx < scale; sx++) {
            int px = x + col * scale + sx;
            int py = y + row * scale + sy;
            if (px >= 0 && px < bb->width && py >= 0 && py < bb->height) {
              bb->pixels[py * (bb->pitch / 4) + px] = color;  /* pitch/4, NOT width */
            }
          }
        }
      }
    }
  }
}

void draw_text(Backbuffer *bb, int x, int y, const char *text, uint32_t color, int scale) {
  int cursor_x = x;
  while (*text) {
    draw_char(bb, cursor_x, y, *text, color, scale);
    cursor_x += 8 * scale;  /* 8 pixels per char, scaled */
    text++;
  }
}
```

**Usage:**

```c
draw_text(bb, 10, 10, "SCORE", COLOR_WHITE, 2);  /* 2x scale */
draw_text(bb, 10, 30, "12345", COLOR_YELLOW, 2);
```

**Rendering integers as text — use `snprintf` (never `itoa`):**

```c
char score_buf[32];
snprintf(score_buf, sizeof(score_buf), "%d", state->score);
draw_text(bb, 10, 30, score_buf, COLOR_YELLOW, 2);
/* JS equivalent: String(state.score) */
```

`snprintf` is safe (null-terminates, never overflows), portable, and handles all int widths. `sprintf` works but has no bounds check — always prefer `snprintf`.

**Multi-size fonts:** Always implement `draw_text` with a `scale` integer parameter from the start. Each pixel becomes an `N×N` block. Retrofitting scale after the UI is "done" means touching every call site.

---

## Audio system

**Reference:** `games/tetris/src/utils/audio.h`, `games/tetris/src/audio.c`

> **Planning note:** For games with non-trivial audio, this section expands into **at minimum two lessons**, and often more: (1) `AudioOutputBuffer` + `SoundInstance` pool + `game_play_sound_at()` + the per-sample loop; (2) `MusicSequencer` + `ToneGenerator` + `game_audio_update()` + MIDI-to-Hz; (3) if the game has sustained/looping sounds: `game_sustain_sound_update()` + `WAVE_TRIANGLE` + `ENVELOPE_TRAPEZOID`; (4) if multiple music tracks: `MusicCrossfade` + `audio_ramp_music_volume()`. Each of these is a separate lesson candidate in the topic inventory — none may be merged into a single lesson if doing so would introduce more than 3 new concepts. Treat the patterns below as the minimum; expect to add 150–200 lines of explanation per audio lesson. Do not mark audio as "optional" in the lesson plan — it is a core feature of most games and is consistently underestimated.

### Audio abstraction tier

Pick one tier per course; document the choice in the audio lesson:

| Use case                                                                                       | Tier                                                                                        | API used                                                                    |
| ---------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| Continuously-generated waveforms — procedural SFX, synth tones, engine hum, chiptune sequencer | **Custom PCM mixer** (`AudioStream` loop + `game_get_audio_samples()`)                      | _Default_ for all courses                                                   |
| Discrete event sounds + background music loaded from file (no synthesis needed)                | **Raylib high-level** (`LoadSound` / `PlaySound` / `LoadMusicStream` / `UpdateMusicStream`) | Only when audio is a minor feature; acknowledge the trade-off in the lesson |

> ⚠ **Never mix tiers in one backend.** Custom-mixer courses must never call `PlaySound()` — route all audio through `game_get_audio_samples()`. High-level-API courses have no `AudioStream` or PCM loop. Mixing the two causes double-playback and audio-ownership confusion.

### Audio output buffer

```c
/*
 * WHO FILLS WHAT
 * ──────────────
 *   Platform sets at init:   samples_buffer, samples_per_second,
 *                             max_sample_count, is_initialized
 *   Platform sets per-frame: sample_count (≤ max_sample_count)
 *   Game writes into:         samples_buffer via game_get_audio_samples()
 *
 * MEMORY LAYOUT — Interleaved Stereo
 *   Index:  0      1      2      3   ...
 *   Value:  L[0]   R[0]   L[1]   R[1] ...
 *   Write:  samples_buffer[s*2+0] = left;  samples_buffer[s*2+1] = right;
 */
typedef struct {
  int16_t *samples_buffer; /* Interleaved stereo PCM; platform allocates.   */
                           /* Hard-coded I16_LE: works on Linux ALSA+Raylib. */
                           /* See EVOLUTION PATH below for F32 backends.     */
  int samples_per_second;  /* 44100 or 48000 — fixed at init                */
  int sample_count;        /* Stereo pairs to write THIS call                */
                           /* (set per-frame by platform, ≤ max_sample_count)*/
  int max_sample_count;    /* Buffer capacity in stereo pairs.               */
                           /* Allocated as: (samples_per_second/60)*8        */
                           /* game_get_audio_samples() MUST NOT exceed this. */
  bool is_initialized;     /* Set true by platform after audio device init.  */
                           /* Check EVERY FRAME — not just at startup.       */
                           /* Audio devices can disappear mid-session (USB,  */
                           /* sleep/wake, driver crash) on some platforms.   */
} AudioOutputBuffer;
```

> **EVOLUTION PATH — F32 / multi-backend:** The engine-level `GameAudioOutputBuffer` replaces `int16_t *samples_buffer` with `void *samples_buffer + AudioSampleFormat format` and adds an `audio_write_sample()` `_Generic` macro that dispatches on format at compile-time. This handles CoreAudio (F32) and WASAPI (F32) backends without touching game code. Keep that upgrade path in mind: do NOT sprinkle raw `int16_t` casts throughout the game layer — all i16 conversion happens once at the final write in `game_get_audio_samples()`.

### Sound effect system

```c
typedef enum {
  SOUND_NONE = 0,
  SOUND_MOVE,
  SOUND_ROTATE,
  SOUND_DROP,
  SOUND_LINE_CLEAR,
  SOUND_TETRIS,
  SOUND_LEVEL_UP,
  SOUND_GAME_OVER,
  SOUND_COUNT
} SOUND_ID;

typedef struct {
  float frequency;      /* Starting frequency (Hz) */
  float frequency_end;  /* Ending frequency (Hz), 0 = no slide */
  float duration_ms;    /* Duration in milliseconds */
  float volume;         /* 0.0 to 1.0 */
} SoundDef;

/* Define sounds as data */
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
  [SOUND_NONE]       = {0, 0, 0, 0.0f},
  [SOUND_MOVE]       = {200, 150, 50, 0.3f},   /* Quick blip down */
  [SOUND_ROTATE]     = {300, 400, 80, 0.3f},   /* Quick blip up */
  [SOUND_DROP]       = {150, 50, 100, 0.5f},   /* Thud down */
  [SOUND_LINE_CLEAR] = {400, 800, 200, 0.6f},  /* Rising sweep */
  [SOUND_TETRIS]     = {300, 1200, 400, 0.8f}, /* Long rising sweep */
  [SOUND_LEVEL_UP]   = {440, 880, 300, 0.5f},  /* Octave up */
  [SOUND_GAME_OVER]  = {400, 100, 500, 0.7f},  /* Sad descend */
};
```

> ⚠ **Default new sounds to maximum audible volume.** Set every `SoundDef.volume` to `0.7–1.0` when first writing it and set `master_volume = 1.0`. Tune down **after** confirming audibility. Starting quiet leads to "silent audio" bugs that are reported multiple times before the root cause is identified. For looping sounds, use `volume ≥ 0.50` — quieter values disappear under the music drone.

> ⚠ **Use `WAVE_TRIANGLE` for any continuously looping sound.** Square waves sound “retro/digital” — fine for short SFX (duration < ~0.3 s), but always perceived as harsh/buzzy when looped. Triangle formula:
>
> ```c
> float wave = 1.0f - fabsf(4.0f * inst->phase - 2.0f);
> ```
>
> Produces continuous linear ramps — no abrupt polarity flips. **Rule:** `WAVE_SQUARE` for short event sounds; `WAVE_TRIANGLE` for anything held or looping. Minimum looping tone frequency: **100 Hz** — below ~80 Hz most laptop/small speakers produce vibration, not audible tone.

> ⚠ **For sounds tied to a held key, use a sustain pattern — not a retrigger loop.** Re-triggering a short clip every 100–300 ms creates N separate start/stop events, perceived as amplitude throbbing. One continuous sustained sound with natural attack/decay is the correct model:
>
> ```c
> /* Call every frame while key is down: */
> game_sustain_sound(&state->audio, SOUND_THRUST);
> /* When key is released — stop calling — natural decay fades the sound out */
> ```
>
> `game_sustain_sound` should: (1) play the sound if not already playing → attack ramp ("spool up"); (2) pin `samples_remaining ≥ total/4` while called → keeps volume at full; (3) when calls stop, remaining samples drain through the decay zone → natural fade. Use `ENVELOPE_TRAPEZOID` (10% attack + 80% sustain + 10% decay) for all looping/held sounds.

**Reference:** `games/tetris/src/utils/audio.h` — `SoundInstance`, `ToneGenerator`, `MusicSequencer`, `MAX_SIMULTANEOUS_SOUNDS`

This is the **canonical** `SoundInstance` used by the Tetris game. Do not strip fields — they are all used:

```c
/* Size from your game's BUSIEST moment: count every looping sound as always-active.
   Add at least 4 slots of headroom. Default: 16 (each slot ≈32 bytes — negligible).
   Add ASSERT at insertion time — silent overflow masks audio bugs for too long.     */
#define MAX_SIMULTANEOUS_SOUNDS 16

typedef struct {
  SOUND_ID sound_id;
  float phase;           /* 0.0–1.0 oscillator phase; do NOT reset between frames */
  int samples_remaining; /* Counts down to zero — sound is done when 0            */
  float volume;          /* 0.0 to 1.0                                            */
  float frequency;       /* Current frequency in Hz (mutates during freq slide)   */
  float frequency_slide; /* Hz added per sample for pitch sweep (0 = no slide)    */
  int total_samples;     /* Total duration in samples; used for envelope ratio     */
  int fade_in_samples;   /* Ramp-up samples (~10 ms) to prevent click on attack   */
  float pan_position;    /* -1.0 = full left, 0.0 = center, 1.0 = full right     */
} SoundInstance;
```

`GameAudioState` owns the pool and volume controls:

```c
typedef struct {
  SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
  MusicSequencer music;
  int samples_per_second;
  float master_volume;  /* 0.0–1.0 global scale */
  float sfx_volume;
  float music_volume;
} GameAudioState;
```

**Reference:** `games/tetris/src/utils/audio.h` lines 84–144 for the full declaration.

### Signal shaping in game code — never the backend

> **Rule:** The platform's only audio job is moving bytes from `samples_buffer` to the hardware ring buffer. Gain, pitch, effects, volume curves, and any DSP modification all belong in **game code** — specifically inside `game_get_audio_samples()` and `game_audio_update()`. Never call `SetAudioStreamVolume()`, use ALSA software volume, or apply OS-level effects. That path creates hidden double-indirection (the backend applies gain on top of the game's gain), makes the two backends behave differently, and prevents porting to new backends. Concrete violations to avoid:
>
> | ❌ Never do this (backend layer)         | ✅ Do this instead (game layer)                          |
> | ---------------------------------------- | -------------------------------------------------------- |
> | `SetAudioStreamVolume(stream, 0.5f)`     | multiply samples by `audio->master_volume` in mixer loop |
> | ALSA SW plugin volume / dmix             | `mixed_left *= audio->master_volume` per sample          |
> | `SetAudioStreamPitch(stream, 1.5f)`      | slide `inst->frequency` in `game_get_audio_samples()`    |
> | Any Raylib `PlaySound()` / `LoadSound()` | `game_play_sound()` → PCM mixer → `samples_buffer`       |

### Playing sounds

**Reference:** `games/tetris/src/audio.c` — `game_play_sound_at()`, `game_play_sound()`

The primary API is `game_play_sound_at()` which accepts a pan position from the caller. `game_play_sound()` is a convenience wrapper for centered (non-spatial) sounds:

```c
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound, float pan) {
  if (sound == SOUND_NONE || sound >= SOUND_COUNT) return;

  /* Find a free slot (O(N), N is small — MAX_SIMULTANEOUS_SOUNDS = 16) */
  int slot = -1;
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    if (audio->active_sounds[i].samples_remaining <= 0) {
      slot = i;
      break;
    }
  }
  /* No free slot: steal oldest — better to drop a sound than to stall */
  if (slot < 0) {
    ASSERT(0 && "Sound pool full — increase MAX_SIMULTANEOUS_SOUNDS");
    slot = 0;  /* steal oldest as fallback in release builds */
  }

  const SoundDef *def   = &SOUND_DEFS[sound];
  SoundInstance  *inst  = &audio->active_sounds[slot];

  int duration_samples =
      (int)(def->duration_ms * (float)audio->samples_per_second / 1000.0f);

  inst->sound_id        = sound;
  inst->phase           = 0.0f;
  inst->frequency       = def->frequency;
  inst->volume          = def->volume;
  inst->total_samples   = duration_samples;
  inst->samples_remaining = duration_samples;
  inst->fade_in_samples = audio->samples_per_second / 100; /* ~10 ms */
  inst->pan_position    = pan;

  if (def->frequency_end > 0.0f && duration_samples > 0) {
    inst->frequency_slide =
        (def->frequency_end - def->frequency) / (float)duration_samples;
  } else {
    inst->frequency_slide = 0.0f;
  }
}

/* Centered (pan = 0.0) convenience wrapper */
void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
  game_play_sound_at(audio, sound, 0.0f);
}
```

**Spatial trigger example — pan from piece position:**

```c
/* In tetris_apply_input(), after a successful move: */
game_play_sound_at(&state->audio, SOUND_MOVE,
                   calculate_piece_pan(state->current_piece.x));
```

**Reference:** `games/tetris/src/audio.c` — `calculate_piece_pan()`, `game_play_sound_at()`

### Audio variety and dynamics

**Sound variety matters.** A game with only 2–3 sounds feels thin. Plan a full sound set up front:

| Category          | Examples                                      | Minimum count |
| ----------------- | --------------------------------------------- | ------------- |
| UI / menu         | confirm, cancel, cursor move                  | 2–3           |
| Player actions    | jump, shoot, dash, land, pickup               | 3–6           |
| Hits / feedback   | player hurt, enemy hit, death, block land     | 2–4           |
| Environment       | coins, doors, switches, ambient loop          | 2–5           |
| State transitions | game over, level up, victory jingle           | 2–4           |
| Music             | gameplay loop, menu theme, boss/intense theme | 1–3 tracks    |

**Name sounds by intent, not by frequency.** `SOUND_PLAYER_JUMP`, `SOUND_ENEMY_DEATH`, `SOUND_PICKUP_COIN` — the enum is documentation. Never `SOUND_HIGH_BLIP`, `SOUND_BEEP_2`.

**Audio dynamics — wire audio to game-state transitions via `change_phase()`:**

| Transition                          | Audio action                                    |
| ----------------------------------- | ----------------------------------------------- |
| → `PLAYING`                         | Start / resume gameplay music                   |
| → `PAUSED`                          | Duck music to ~30 %, mute new SFX               |
| → `GAME_OVER`                       | Stop music; play defeat sound                   |
| → `MENU`                            | Cross-fade to menu track                        |
| → `LEVEL_TRANSITION`                | Play jingle; stop gameplay music                |
| Intensity rising (boss, speed ramp) | Gradually increase `music_volume` or swap track |

**Sustained sounds (engine, thrust, ambient hum):**

For any sound tied to a held state, use a sustain model — not re-triggering a short clip:

```c
/* Call every frame while the source is active (key held, engine on, etc.) */
void game_sustain_sound_update(GameAudioState *audio, SOUND_ID id, int should_play) {  /* int, not bool — avoids stdbool.h dependency */
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    SoundInstance *inst = &audio->active_sounds[i];
    if (inst->sound_id != id || inst->samples_remaining <= 0) continue;
    if (should_play) {
      /* Pin samples_remaining above the decay threshold */
      int min_remaining = inst->total_samples / 4;
      if (inst->samples_remaining < min_remaining)
        inst->samples_remaining = min_remaining;
    }
    return;  /* found; handled */
  }
  /* Not already playing — start it */
  if (should_play) game_play_sound(audio, id);
}
```

Re-triggering a short clip every frame creates audible amplitude throbbing. One sustained sound with natural attack/decay is always preferred.

**Gradual music volume transitions (duck, fade out, ramp up):**

```c
/* Call in game_audio_update() every frame */
void audio_ramp_music_volume(GameAudioState *audio, float target, float rate, float dt) {
  float diff = target - audio->music_volume;
  float step = rate * dt;  /* rate = 1.0f → full range in 1 second */
  if (fabsf(diff) <= step) audio->music_volume = target;
  else audio->music_volume += (diff > 0.0f ? step : -step);
}
```

Use this on every phase transition: duck on pause, fade out on game over, ramp up at level start. Abrupt volume jumps always sound like a bug to the player.

### Music crossfading (for games with multiple tracks)

When the game switches between distinct music themes (menu ↔ gameplay ↔ boss), a hard cut always sounds jarring. Crossfade by simultaneously ramping one track down and the other up:

```c
typedef struct {
    int   current_track;       /* MUSIC_* enum index — track currently at full volume */
    int   next_track;          /* target track fading in                               */
    float progress;            /* 0.0 = current only → 1.0 = next only                */
    float duration;            /* total crossfade time in seconds                      */
    int   is_crossfading;      /* int flag — no stdbool.h dependency                  */
} MusicCrossfade;

void music_crossfade_to(GameAudioState *audio, int new_track, float duration) {
    if (audio->crossfade.current_track == new_track) return;
    audio->crossfade.next_track  = new_track;
    audio->crossfade.duration    = duration;
    audio->crossfade.progress    = 0.0f;
    audio->crossfade.is_crossfading = 1;
}

/* Call in game_audio_update() every frame */
void music_crossfade_update(GameAudioState *audio, float dt) {
    if (!audio->crossfade.is_crossfading) return;
    audio->crossfade.progress += dt / audio->crossfade.duration;
    if (audio->crossfade.progress >= 1.0f) {
        audio->crossfade.current_track   = audio->crossfade.next_track;
        audio->crossfade.progress        = 1.0f;
        audio->crossfade.is_crossfading  = 0;
    }
}
```

In the sample loop, mix both tracks weighted by `progress`:

```c
float vol_current = 1.0f - audio->crossfade.progress;
float vol_next    = audio->crossfade.progress;
/* Apply vol_current to current_track samples, vol_next to next_track samples */
```

Call `music_crossfade_to()` from `change_phase()` — never directly in `game_update()` logic.

### Generating samples

**Reference:** `games/tetris/src/audio.c` — `game_get_audio_samples()`

This is the **complete** sample loop as used by the Tetris game. It incorporates:

- Click prevention (fade-in / fade-out envelope)
- Stereo panning (per-channel volume)
- Frequency slide (pitch sweep)
- Music tone mixing

```c
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
  GameAudioState *audio = &state->audio;

  /* Safety cap: never write past the allocated buffer.                      */
  /* Platform already clamps before calling us; this is defence-in-depth.    */
  /* WHY max_sample_count? A backend misconfiguration or late audio-device   */
  /* change could pass a sample_count larger than the allocated buffer       */
  /* size → silent out-of-bounds write / memory corruption.                  */
  int sample_count = buffer->sample_count;
  if (buffer->max_sample_count > 0 && sample_count > buffer->max_sample_count)
    sample_count = buffer->max_sample_count;

  int16_t *out          = buffer->samples_buffer;
  float    inv_sr       = 1.0f / (float)buffer->samples_per_second;
  int      fade_out_samples = buffer->samples_per_second / 100; /* ~10 ms */

  /* Clear buffer (memset so any skipped slot leaves silence) */
  memset(out, 0, (size_t)sample_count * 2 * sizeof(int16_t));

  for (int s = 0; s < sample_count; s++) {
    float left  = 0.0f;
    float right = 0.0f;

    /* ── Sound effects ───────────────────────────────────────── */
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
      SoundInstance *inst = &audio->active_sounds[i];
      if (inst->samples_remaining <= 0) continue;

      /* Square wave oscillator — do NOT reset phase on slot reuse */
      float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;

      /* ── Click prevention: fade-in / fade-out ────────────── */
      int   elapsed  = inst->total_samples - inst->samples_remaining;
      float fade_in  = 1.0f;
      if (inst->fade_in_samples > 0 && elapsed < inst->fade_in_samples) {
        fade_in = (float)elapsed / (float)inst->fade_in_samples;
      }
      float fade_out = 1.0f;
      if (fade_out_samples > 0 && inst->samples_remaining < fade_out_samples) {
        fade_out = (float)inst->samples_remaining / (float)fade_out_samples;
      }
      float env = fade_in * fade_out * inst->volume * audio->sfx_volume;

      /* ── Stereo panning (linear law) ─────────────────────── */
      /* pan -1.0 = full left, 0.0 = center, 1.0 = full right   */
      float pan      = inst->pan_position;
      float vol_left  = (pan <= 0.0f) ? 1.0f : 1.0f - pan;
      float vol_right = (pan >= 0.0f) ? 1.0f : 1.0f + pan;

      left  += wave * env * vol_left;
      right += wave * env * vol_right;

      /* ── Advance oscillator state ─────────────────────────── */
      inst->phase += inst->frequency * inv_sr;
      if (inst->phase >= 1.0f) inst->phase -= 1.0f;  /* wrap, don't reset */
      inst->frequency += inst->frequency_slide;
      inst->samples_remaining--;
    }

    /* ── Music tone (ToneGenerator) ──────────────────────────── */
    /* Follow games/tetris/src/audio.c — game_get_audio_samples() */
    /* for the full tone mixing code (current_volume ramping).     */

    /* ── Master volume + clamp + write stereo pair ───────────── */
    float scale = audio->master_volume * 16000.0f;
    left  *= scale;
    right *= scale;

    *out++ = (left  > 32767.0f) ? 32767 : (left  < -32768.0f) ? -32768 : (int16_t)left;
    *out++ = (right > 32767.0f) ? 32767 : (right < -32768.0f) ? -32768 : (int16_t)right;
  }
}
```

**Why float mixing — never int16 directly:** Two simultaneous sounds each at `INT16_MAX` (32767) added as `int16_t` overflow to –2 (two's complement wrap). Even away from extremes, every `int16_t` multiply drops the low bits, building up quantisation noise over N voices. Mixing in `float32` keeps full precision across all additions; _one_ clamped integer conversion at the final `*out++ = ...` is the only precision loss. Rule: accumulate in `float left / right`, scale by `master_volume * 16000.0f`, clamp and cast once.

**Click prevention** is handled by `fade_in` / `fade_out` computed from `inst->fade_in_samples` and `inst->total_samples`. Set these when initializing a slot (see `game_play_sound_at()` above).

**Stereo panning** uses linear-law: the attenuated channel is scaled by `1 - |pan|`. This matches `calculate_piece_pan()` in `audio.c`.

**DO NOT** reset `inst->phase` to 0 between frames or on buffer boundaries — that creates a click. `phase` is owned by `SoundInstance` and ticks continuously until the sound expires.

### Platform audio integration

**Reference:** `games/tetris/src/main_raylib.c` — `platform_audio_init()`, `platform_audio_update()` and `games/tetris/src/main_x11.c` — `platform_audio_init()`, `platform_audio_get_samples_to_write()`, `platform_audio_update()`

Always follow the actual Tetris files rather than this summary — they contain the full ALSA hardware parameter setup, VSync-safe buffer sizing, and pre-fill sequences.

**Raylib pattern:**

Every frame, after `IsAudioStreamProcessed()` returns true, fill the stream with `samples_per_frame * 2` samples. `buffer_size_samples` is set to `samples_per_frame * 2` during init (Casey's double-buffer approach):

```c
/* Raylib: init */
config->buffer_size_samples = config->samples_per_frame * 2;
SetAudioStreamBufferSizeDefault(config->buffer_size_samples);
g_raylib.stream = LoadAudioStream(config->samples_per_second, 16, 2);
PlayAudioStream(g_raylib.stream);

/* Pre-fill 2 silent buffers so Raylib has data immediately —              */
/* prevents an initial audio underrun click on the very first frame.       */
for (int i = 0; i < 2; i++) {
  memset(g_raylib.sample_buffer, 0,
         g_raylib.buffer_size_frames * 2 * sizeof(int16_t));
  UpdateAudioStream(g_raylib.stream, g_raylib.sample_buffer,
                    g_raylib.buffer_size_frames);
}

/* Raylib: per-frame update (inside game loop) */
/* Use WHILE — not if — to drain every processed slot in one frame.        */
/* An `if` only processes one chunk per frame; a large OS sleep can cause  */
/* two chunks to become ready simultaneously, leaving one unserviced.      */
while (IsAudioStreamProcessed(g_raylib.stream)) {
  AudioOutputBuffer ab = {
    .samples_buffer     = g_raylib.sample_buffer,
    .samples_per_second = config->samples_per_second,
    .sample_count       = g_raylib.buffer_size_frames,
    .max_sample_count   = config->max_sample_count,
  };
  game_get_audio_samples(game_state, &ab);
  UpdateAudioStream(g_raylib.stream, ab.samples_buffer, ab.sample_count);
}
```

> ⚠ **Never use Raylib's `PlaySound()` / `LoadSound()` / `LoadSoundFromWave()` in this architecture.** Those functions hand audio data to Raylib which plays it on its own schedule, bypassing `game_get_audio_samples()` entirely. Any call to `PlaySound()` in the codebase means the PCM mixer is being short-circuited — search for it and replace with `game_play_sound()`.

> ⚠ **Use a single `AUDIO_CHUNK_SIZE` constant for all Raylib audio sizing.** `SetAudioStreamBufferSizeDefault`, `UpdateAudioStream`, and `AudioOutputBuffer.sample_count` must all use the exact same constant. Independently deriving the size as `samples_per_frame * 2` in different places causes the ring-buffer write pointer to drift — audible stuttering every ~1 second:
>
> ```c
> #define AUDIO_CHUNK_SIZE 2048          /* single source of truth */
> /* Init: */  SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);
> /* Fill: */  UpdateAudioStream(stream, buf.samples, AUDIO_CHUNK_SIZE);
> /* Game: */  ab.sample_count = AUDIO_CHUNK_SIZE;
> ```

**X11/ALSA pattern:**

> ⚠ **Use `snd_pcm_hw_params` — never `snd_pcm_set_params`.** `snd_pcm_set_params(latency_hint_46ms)` is a convenience wrapper that creates an unpredictably-sized hardware buffer. At a 46 ms hint it may allocate only ~2028 samples; if your pre-fill is 2048 samples and you require `avail ≥ 2048` before each write, `avail` will almost always be below the threshold → permanent silence after startup. Use `snd_pcm_hw_params` (the explicit API) and follow `games/tetris/src/main_x11.c` — `platform_audio_init()` as the canonical reference.

Calculate the per-frame write amount using `snd_pcm_delay` (Casey's model — see §Latency model). Write only the samples needed to reach the latency target:

```c
/* X11: per-frame update */
int to_write = platform_audio_get_samples_to_write(config);
if (to_write > 0) {
  AudioOutputBuffer ab = {
    .samples            = g_x11.sample_buffer,
    .samples_per_second = g_x11.samples_per_second,
    .sample_count       = to_write,
  };
  game_get_audio_samples(game_state, &ab);
  snd_pcm_sframes_t written = snd_pcm_writei(g_x11.pcm_handle,
                                              ab.samples_buffer, to_write);
  if (written < 0) {
    snd_pcm_recover(g_x11.pcm_handle, (int)written, 0);
    /* Re-pre-fill silence after underrun recovery.                      */
    /* Without this the hardware buffer is empty at recovery boundary,   */
    /* causing an audible click on the very next frame.                  */
    int silence = config->latency_samples + config->safety_samples;
    memset(g_x11.sample_buffer, 0,
           (size_t)silence * 2 * sizeof(int16_t));
    snd_pcm_writei(g_x11.pcm_handle, g_x11.sample_buffer, silence);
  }
}
```

**Follow:** `games/tetris/src/main_x11.c` — `platform_audio_init()` for full ALSA hw_params setup and `hw_buffer_size` query. `games/tetris/src/main_raylib.c` — `platform_audio_init()` for pre-fill loop.

> ⚠ **`game_audio_init` resets audio state — music triggered by `game_init` will be silently erased.** `game_init` typically calls `change_phase()` which sets `seq.is_playing = 1` to start title music. But `platform_audio_init` calls `game_audio_init` immediately afterwards; `game_audio_init` begins with `memset(audio, 0, sizeof(*audio))` — erasing `is_playing`. The audio hardware starts with the sequencer stopped → no title music on launch.
>
> **Fix:** retrigger the initial phase's music explicitly after `platform_audio_init` returns:
>
> ```c
> game_init(&state, &bb);               /* sets initial phase; triggers music internally */
> platform_audio_init(&state, RATE);    /* game_audio_init runs here — memset wipes seq state */
> game_music_play_title(&state.audio);  /* retrigger initial phase music AFTER audio init */
> ```
>
> **Rule:** `game_init` owns game logic state. Audio sequencer state is owned by `GameAudioState` which is initialised to zero by `game_audio_init`. Never rely on `game_init` to start audio playback; always re-trigger initial audio after `platform_audio_init` completes.

### Latency model (Casey's approach)

**Reference:** `games/tetris/src/platform.h` — `PlatformAudioConfig`, `games/tetris/src/main_x11.c` — `platform_audio_init()`, `platform_audio_get_samples_to_write()`

The goal is to keep the audio hardware buffer filled just enough ahead of playback — too little causes underruns (silence/glitches), too much causes perceptible audio lag.

```c
/* In platform.h */
typedef struct {
  int samples_per_second;        /* 44100 or 48000                          */
  int buffer_size_samples;       /* Platform buffer size                    */
  int is_initialized;

  /* Casey's latency model */
  int samples_per_frame;         /* samples_per_second / hz                 */
  int latency_samples;           /* samples_per_frame * frames_of_latency   */
  int safety_samples;            /* samples_per_frame / 3  (small margin)   */
  int64_t running_sample_index;  /* Total samples ever written              */
  int frames_of_latency;         /* Target: ~2 frames of audio buffered     */
  int hz;                        /* Game update rate (e.g., 30 or 60)       */
} PlatformAudioConfig;
```

**Latency formula:**

```
latency_samples = (samples_per_second / hz) * frames_of_latency
```

Example at 48 000 Hz, 60 fps, 2 frames of latency:

```
latency = (48000 / 60) * 2 = 1600 samples ≈ 33 ms
```

**Initialization (both platforms):**

```c
config->samples_per_frame    = config->samples_per_second / config->hz;
config->latency_samples      = config->samples_per_frame * config->frames_of_latency;
config->safety_samples       = config->samples_per_frame / 3;
config->running_sample_index = 0;
```

**How to use it — X11/ALSA (query-based):**

```c
/* Every frame: how many samples should we write? */
static int platform_audio_get_samples_to_write(PlatformAudioConfig *config) {
  snd_pcm_sframes_t frames_avail = snd_pcm_avail_update(g_x11.pcm_handle);
  if (frames_avail < 0) return 0;

  /* Fill up to (latency + safety) ahead of the play cursor */
  int target        = config->latency_samples + config->safety_samples;
  int queued        = (int)(g_x11.hw_buffer_size - frames_avail);
  int to_write      = target - queued;
  return (to_write > 0) ? to_write : 0;
}
```

**Raylib note:** Raylib has no equivalent of `snd_pcm_avail_update`. Use `IsAudioStreamProcessed()` as the trigger and set `buffer_size_samples = samples_per_frame * 2` for a safe double-buffer. Follow `games/tetris/src/main_raylib.c` — `platform_audio_update()` for the full implementation.

> ⚠ **Raylib frame count must match registered buffer size.** The `sample_count` you pass to `game_get_audio_samples()` must equal the value used to register the stream with Raylib (i.e., `buffer_size_frames`). Passing a different count causes Raylib to misalign its internal ring buffer — you hear repeated or skipped audio every second. Store `buffer_size_frames` once at init and use it everywhere the audio buffer is filled.

> ⚠ **Raylib audio: gate BOTH generate and push behind `IsAudioStreamProcessed` — and use `SAMPLES_PER_FRAME = 2048`.** With 735 frames (≈1 render frame at 60 fps), any timing jitter causes consecutive pushes before the previous chunk is consumed. `UpdateAudioStream` silently drops the write → ring-buffer write-pointer drifts ahead of read-pointer → wavy/echoing audio with a subtle pitch shift. 2048 frames ≈46 ms of headroom eliminates this. Never generate-always / push-sometimes — generation and push are one atomic operation:
>
> ```c
> /* Correct — gate both; always push exactly SAMPLES_PER_FRAME */
> #define SAMPLES_PER_FRAME 2048  /* ≈46 ms — never use 735 */
> if (IsAudioStreamProcessed(stream)) {
>     game_get_audio_samples(&state, &buf);
>     UpdateAudioStream(stream, buf.samples, SAMPLES_PER_FRAME);
> }
> ```
>
> Call `SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME * 2)` **before** `LoadAudioStream` — without it the default ring buffer is very large, causing `IsAudioStreamProcessed` to return false for many consecutive frames. must be set to `1` to prevent silent startup.\*\* ALSA's default `start_threshold` equals the hardware ring-buffer capacity — ALSA waits until the entire buffer is full before beginning playback. At a typical 48 kHz / 2-frame latency configuration this causes ~0.5 s of silence at startup. Set it to `1` so playback begins immediately on the first write:
>
> ```c
> snd_pcm_sw_params_t *sw_params;
> snd_pcm_sw_params_alloca(&sw_params);
> snd_pcm_sw_params_current(pcm_handle, sw_params);   /* MUST call current() before overriding any field */
> snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, 1);
> snd_pcm_sw_params(pcm_handle, sw_params);
> ```
>
> **Never skip** `snd_pcm_sw_params_current()` — without it the other fields retain undefined values that silently override your settings.

> ⚠ **`snd_pcm_hw_params_alloca` and `snd_pcm_sw_params_alloca` expand to `alloca()`. With clang and `-Wall`, this produces "call to undeclared library function" unless `<alloca.h>` is included explicitly.** GCC is tolerant (glibc pulls it in transitively via `<alsa/asoundlib.h>`), but clang is stricter. Always add:
>
> ```c
> #include <alloca.h>       /* required for snd_pcm_*_alloca macros with clang */
> #include <alsa/asoundlib.h>
> ```
>
> Put `<alloca.h>` **before** `<alsa/asoundlib.h>` and add a comment explaining why — students will otherwise delete it thinking it is unused.

### Phase accumulator pattern

A phase accumulator is the simplest correct oscillator implementation. `phase` advances by `frequency / sample_rate` per sample and wraps at 1.0:

```c
/* Advance phase — do NOT reset this to 0 between frames or on slot reuse */
inst->phase += inst->frequency * inv_sr;
if (inst->phase >= 1.0f) inst->phase -= 1.0f;  /* wrap, never reset */
```

Why wrap instead of reset? A hard reset to 0 restarts the wave at a fixed point, creating a discontinuity (click) that is clearly audible.

**Square wave:** `(phase < 0.5f) ? 1.0f : -1.0f`

**Sine wave (richer but slower):** `sinf(phase * 2.0f * M_PI)`

For game audio, square waves work well for **short** event sounds (blips, hits, sweeps) — CPU-free and retro-sounding. For **sustained or looping** sounds use `WAVE_TRIANGLE` (see audio volume guidance above).

### Volume ramping (click prevention)

When volume changes discontinuously between buffer fills — e.g., a slot is reused mid-buffer — the sample value jumps, producing a click. The fix is to ramp `current_volume` toward `target_volume` by a fixed step per sample:

```c
typedef struct {
  float frequency;
  float current_volume;  /* actual volume this sample */
  float target_volume;   /* desired volume (set instantly) */
  int   is_playing;
} ToneGenerator;

/* Inside the per-sample loop: */
float ramp_speed = 0.001f;  /* per-sample step */
if      (tone->current_volume < tone->target_volume) tone->current_volume += ramp_speed;
else if (tone->current_volume > tone->target_volume) tone->current_volume -= ramp_speed;
```

**Reference:** `games/tetris/src/utils/audio.h` — `ToneGenerator`

### Two-loop audio architecture

The Tetris audio uses two separate loops that share state via `ToneGenerator`:

| Loop               | Function                   | Runs on                        | Does                                                                                    |
| ------------------ | -------------------------- | ------------------------------ | --------------------------------------------------------------------------------------- |
| **Sequencer loop** | `game_audio_update()`      | Game logic time (`delta_time`) | Advances step timer, picks next MIDI note, sets `tone.target_volume` / `tone.frequency` |
| **Sample loop**    | `game_get_audio_samples()` | Audio time (per PCM sample)    | Reads `ToneGenerator` state, produces actual int16 samples with ramped volume           |

```
game_update(delta_time)
    └─ game_audio_update()   ← advances sequencer, writes to ToneGenerator

platform audio callback
    └─ game_get_audio_samples()  ← reads ToneGenerator, fills PCM buffer
```

**Critical:** `game_audio_update` is called once per frame. `game_get_audio_samples` is called by the platform audio layer on its own schedule. They communicate only through `GameAudioState` — no direct calls between them.

**Reference:** `games/tetris/src/audio.c` — `game_audio_update()` and `game_get_audio_samples()`

### Music sequencer (optional)

**Reference:** `games/tetris/src/audio.c` — `MusicSequencer`, `game_audio_update()`

```c
#define MUSIC_PATTERN_LENGTH 16
#define MUSIC_NUM_PATTERNS 4

typedef struct {
  uint8_t patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
  int current_pattern;
  int current_step;
  float step_timer;
  float step_duration;  /* Seconds per step (tempo) */
  ToneGenerator tone;
  int is_playing;
} MusicSequencer;

void game_audio_update(GameAudioState *audio, float delta_time) {
  if (!audio->music.is_playing) return;

  MusicSequencer *seq = &audio->music;
  seq->step_timer += delta_time;

  if (seq->step_timer >= seq->step_duration) {
    seq->step_timer -= seq->step_duration;

    uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];
    if (note > 0) {
      seq->tone.frequency = midi_to_freq(note);
      seq->tone.is_playing = 1;
    } else {
      seq->tone.is_playing = 0;
    }

    seq->current_step++;
    if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
      seq->current_step = 0;
      seq->current_pattern = (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
    }
  }
}

/* MIDI note to frequency conversion */
static inline float midi_to_freq(int note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}
```

---

## Game state management

### State struct pattern

**Reference:** `games/tetris/src/game.h` — `GameState`

```c
typedef struct {
  /* Core game data */
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
  CurrentPiece current_piece;
  int score;
  int level;
  int pieces_count;
  bool is_game_over;

  /* Timing */
  struct {
    float timer;
    float interval;
  } drop_timer;

  /* Input repeat state (separate from raw input!) */
  struct {
    RepeatInterval move_left;
    RepeatInterval move_right;
    RepeatInterval move_down;
  } input_repeat;

  /* Line clear animation */
  struct {
    int indexes[4];
    int count;
    float flash_timer;
  } completed_lines;

  /* Audio state */
  GameAudioState audio;
} GameState;
```

**Principles:**

1. Group related fields with anonymous structs
2. Keep timing state separate from game logic state
3. Input repeat state lives here, not in `GameInput`
4. Audio state is part of game state (persists across frames)

> ⚠ **`memset` in `game_init` will wipe `GameAudioState` including `samples_per_second`.** If your restart flow calls `memset(state, 0, sizeof(*state))`, the audio subsystem goes permanently silent — `samples_per_second = 0` causes division-by-zero in the sample loop or fills every sound with zero-length. Fix: save audio config before memset, restore it after:
>
> ```c
> void game_init(GameState *state) {
>     /* Save fields that must survive reset */
>     int sps = state->audio.samples_per_second;
>     int score_best = state->score_best;
>
>     memset(state, 0, sizeof(*state));
>
>     /* Restore them */
>     state->audio.samples_per_second = sps;
>     state->audio.master_volume = 1.0f;
>     state->score_best = score_best;
> }
> ```
>
> Alternatively, store `samples_per_second` in a separate `PlatformAudioConfig` that is never passed through `game_init` — the platform layer owns it and re-injects it each frame.

### Initialization

**Reference:** `games/tetris/src/game.c` — `game_init()`

```c
void game_init(GameState *state, GameInput *input) {
  /* Zero everything first */
  memset(state, 0, sizeof(GameState));

  /* Set non-zero defaults */
  state->pieces_count = 1;
  state->drop_timer.interval = 0.8f;

  /* Initialize field with walls */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
      }
    }
  }

  /* Initialize input repeat intervals */
  state->input_repeat.move_left = (RepeatInterval){
    .initial_delay = 0.15f,
    .interval = 0.05f
  };
  state->input_repeat.move_right = (RepeatInterval){
    .initial_delay = 0.15f,
    .interval = 0.05f
  };
  state->input_repeat.move_down = (RepeatInterval){
    .initial_delay = 0.0f,
    .interval = 0.03f
  };

  /* Spawn first piece */
  srand((unsigned int)time(NULL));
  state->current_piece.index = rand() % TETROMINOS_COUNT;
  state->current_piece.next_index = rand() % TETROMINOS_COUNT;
  state->current_piece.x = FIELD_WIDTH / 2 - 2;
  state->current_piece.y = 0;
}
```

---

## State machine architecture

**Mandatory for any game with 2 or more mutually exclusive states.** Introduce `GAME_PHASE` as soon as the game has more than a single active screen — specifically, in the first lesson after `GameState` is introduced and before any second game screen or animation state is added. Retrofitting it later requires touching `game_update()`, `game_render()`, every animation timer, and every audio trigger — vastly cheaper to build it in from the start.

**Why:** Scattered boolean flags (`is_paused`, `is_game_over`, `is_animating`) cause combinatorial explosion — every branch must check multiple flags simultaneously, illegal combinations become possible, and audio/render coupling becomes incoherent. A single `GAME_PHASE` enum makes each frame’s control flow a single `switch`, establishes a single source of truth for “what is the game doing right now”, and makes audio transitions trivially correct.

### Pattern

```c
/* In game.h — start with what your game needs; add phases as features demand */
typedef enum {
  GAME_PHASE_MENU,              /* Title / main menu screen                          */
  GAME_PHASE_PLAYING,           /* Normal gameplay                                   */
  GAME_PHASE_PAUSED,            /* Pause overlay; game logic frozen                  */
  GAME_PHASE_ANIMATION,         /* Brief locked animation (line clear, hit flash...) */
  GAME_PHASE_LEVEL_TRANSITION,  /* "Level 2!" splash before next round               */
  GAME_PHASE_GAME_OVER,         /* Score display; waiting for restart input          */
  /* Extend as needed: GAME_PHASE_CUTSCENE, GAME_PHASE_LOADING, etc. */
} GAME_PHASE;

typedef struct {
  GAME_PHASE phase;
  /* ... other fields ... */
} GameState;
```

**Minimum for a complete game:** `MENU`, `PLAYING`, `PAUSED`, `GAME_OVER`. Add `ANIMATION`/`LEVEL_TRANSITION` the moment any locked animation is needed.

**Transition helper — centralizes per-phase entry logic AND audio:**

```c
static void change_phase(GameState *state, GAME_PHASE new_phase) {
  /* Per-phase entry: reset timers, trigger audio, set flags */
  switch (new_phase) {
  case GAME_PHASE_PLAYING:
    game_music_play(&state->audio);         /* resume or start gameplay music */
    break;
  case GAME_PHASE_PAUSED:
    game_music_duck(&state->audio, 0.3f);   /* lower music volume on pause    */
    break;
  case GAME_PHASE_GAME_OVER:
    game_music_stop(&state->audio);
    game_play_sound(&state->audio, SOUND_GAME_OVER);
    break;
  case GAME_PHASE_MENU:
    game_music_play_track(&state->audio, MUSIC_MENU);
    break;
  case GAME_PHASE_ANIMATION:
    state->animation_timer = 0.3f;          /* or game-specific duration */
    break;
  default:
    break;
  }
  state->phase = new_phase;
}
```

**`change_phase()` is the single place to trigger every audio response to a game-state change.** This keeps audio coupling out of `game_update()` and `apply_input()`.

**Dispatch in `game_update()`:**

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
  game_audio_update(&state->audio, delta_time);  /* always runs */

  switch (state->phase) {
  case GAME_PHASE_PLAYING:
    apply_input(state, input, delta_time);
    update_drop_timer(state, delta_time);
    break;

  case GAME_PHASE_LINE_CLEAR:
    state->completed_lines.flash_timer -= delta_time;
    if (state->completed_lines.flash_timer <= 0.0f) {
      collapse_completed_lines(state);
      change_phase(state, GAME_PHASE_PLAYING);
    }
    break;

  case GAME_PHASE_GAME_OVER:
    if (input->restart) {
      game_init(state);
      change_phase(state, GAME_PHASE_PLAYING);
      game_music_play(&state->audio);
    }
    break;

  case GAME_PHASE_PAUSED:
    if (input->restart) change_phase(state, GAME_PHASE_PLAYING);
    break;
  }
}
```

**In `game_render()`** — check phase for overlays:

```c
if (state->phase == GAME_PHASE_GAME_OVER) {
  draw_rect_blend(bb, 0, 0, bb->width, bb->height, GAME_RGBA(0, 0, 0, 160));
  draw_text(bb, bb->width / 2 - 40, bb->height / 2, "GAME OVER", COLOR_RED, 2);
}
```

**When to use what:**

| Situation                       | Use                               |
| ------------------------------- | --------------------------------- |
| 2–4 mutually exclusive states   | `GAME_PHASE` enum + `switch`      |
| Independently toggled options   | Boolean flag (`is_muted`)         |
| Nested sub-states               | Nested enum or separate sub-phase |
| Many orthogonal states          | Bitfield or multiple enums        |
| level/phase/mode state/behavior | Separate struct w/ flags/enums    |

> **All new courses must use `GAME_PHASE` from the lesson that introduces `GameState` onward.** Never use scattered booleans (`is_game_over`, `is_paused`) in generated course code — they are the anti-pattern this section teaches students to avoid. The Tetris reference uses `state->is_game_over` as a historical artefact; do not copy that pattern in new courses.

---

## Game update pattern

**Reference:** `games/tetris/src/game.c` — `game_update()`

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
  /* Update audio sequencer (always, even when paused) */
  game_audio_update(&state->audio, delta_time);

  /* Handle game over state */
  if (state->is_game_over) {
    if (input->restart) {
      game_init(state, input);
      game_music_play(&state->audio);
    }
    return;  /* Don't process game logic */
  }

  /* Handle animation states (line clear flash) */
  if (state->completed_lines.flash_timer > 0) {
    state->completed_lines.flash_timer -= delta_time;
    if (state->completed_lines.flash_timer <= 0) {
      /* Animation done - collapse lines */
      collapse_completed_lines(state);
    }
    return;  /* Freeze game logic during animation */
  }

  /* Process input */
  apply_input(state, input, delta_time);

  /* Update game timers */
  state->drop_timer.timer += delta_time;
  if (state->drop_timer.timer >= state->drop_timer.interval) {
    state->drop_timer.timer -= state->drop_timer.interval;
    try_drop_piece(state);
  }
}
```

**Key patterns:**

1. Audio updates even during pause/game over (music keeps playing)
2. Early returns for special states (game over, animations)
3. Input processing is separate from timer-based logic
4. Timer uses subtraction to keep remainder (precision)

---

## Rendering pattern

**Reference:** `games/tetris/src/game.c` — `game_render()`

```c
void game_render(Backbuffer *bb, GameState *state) {
  /* 1. Clear background */
  for (int i = 0; i < bb->width * bb->height; i++) {
    bb->pixels[i] = COLOR_BLACK;
  }

  /* 2. Draw static elements (field, walls) */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      unsigned char cell = state->field[y * FIELD_WIDTH + x];
      if (cell != TETRIS_FIELD_EMPTY) {
        draw_cell(bb, x, y, get_cell_color(cell));
      }
    }
  }

  /* 3. Draw dynamic elements (current piece) */
  draw_piece(bb, &state->current_piece);

  /* 4. Draw UI (score, level, next piece) */
  draw_ui(bb, state);

  /* 5. Draw overlays (game over screen) */
  if (state->is_game_over) {
    draw_game_over_overlay(bb);
  }
}
```

**Layer order:**

1. Background (clear or fill)
2. Static game elements
3. Dynamic game elements
4. UI elements
5. Overlays (semi-transparent)

> ⚠ **Render must read the same state snapshot that update wrote.** If any function computes a value from `state` in both `game_update` and `game_render` (e.g. a scroll offset, predicted position, or derived coordinate), both calls **must** read the same field of `state` — not a re-derived value from an already-incremented timer. The failure mode is a ghost collision: the update decides "no collision" using the pre-advance position, but render draws using the post-advance position, making the object appear to pass through a wall for one frame. **Rule:** `game_render` must be a pure read of `GameState` with no time advancement or mutation.

---

## Scalable game patterns

### When to use which pattern

| Pattern                             | Use when                                                    | Example in Tetris / beyond                    |
| ----------------------------------- | ----------------------------------------------------------- | --------------------------------------------- |
| Global fixed arrays                 | Max count is small and known at compile time                | Tetromino definitions, color table            |
| Fixed free-list pool                | Many short-lived objects of one type                        | Bullets, particles, enemies                   |
| `GAME_PHASE` state machine          | Any game with 2+ exclusive phases                           | Mandatory from the `GameState` lesson onward  |
| Spatial grid / tilemap              | Collision between many objects in bounded space             | Tetris field, platformer tiles                |
| Chunk + offset position             | World larger than pixel-coordinate range (integer overflow) | Platformer, open-world RPG                    |
| Double-buffered state               | Need last frame's value to detect transitions               | Input, interpolation                          |
| World units + `PIXELS_PER_METER`    | Non-grid games; physics/gravity; resolution independence    | Platformers, shooters, physics sandboxes      |
| Camera + `world_to_screen()`        | Level larger than one screen                                | Platformers, RPGs, any scrolling game         |
| AABB collision                      | 2D rectangle vs rectangle without rotation                  | Most platformers, Frogger, top-down shooters  |
| Swept AABB                          | Fast-moving objects that may tunnel through walls           | Bullets, rockets, fast players                |
| Fixed-timestep accumulator          | Physics that must be frame-rate-independent                 | Rigid body, precise collision, networked play |
| Entity type-enum + union            | Multiple entity behaviours sharing a pool                   | Mixed enemies, pickups, projectiles           |
| Struct-of-Arrays (SoA)              | N ≥ 1024 particles; inner loop touches only 1–2 fields      | Falling-sand grain simulation                 |
| Data file + designated initialisers | Level/map/config data that changes independently of logic   | `levels.c` with `g_levels[TOTAL_LEVELS]`      |
| Particle pool                       | Purely cosmetic effects (explosions, sparks, dust)          | Any game needing visual feedback/juice        |
| Debug visualization overlay         | Physics, AI, or hitbox bugs that are hard to see            | `#ifdef DEBUG` draw calls in `game_render()`  |
| Preview (example: Ghost piece)      | Show where something would land without committing          | Tetris ghost; any "aim preview"               |

### Fixed free-list pool

Pre-allocated pool for frequently created/destroyed objects — no `malloc` at runtime:

```c
#define MAX_BULLETS 64

typedef struct {
  float x, y;
  float vel_x, vel_y;
  int active;
} Bullet;

static Bullet bullets[MAX_BULLETS];  /* never malloc'd */

/* "Allocate" = find first inactive slot */
Bullet *spawn_bullet(float x, float y, float vx, float vy) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i] = (Bullet){x, y, vx, vy, 1};
      return &bullets[i];
    }
  }
  return NULL;  /* pool full — drop or steal oldest */
}

/* "Free" = mark inactive */
void update_bullets(float dt) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    bullets[i].x += bullets[i].vel_x * dt;
    bullets[i].y += bullets[i].vel_y * dt;
    if (out_of_bounds(&bullets[i])) bullets[i].active = 0;
  }
}
```

**Removing objects during iteration: `compact_pool`**

Never remove an object while iterating the pool — the element swapped into the current index is skipped or double-processed. Instead: **mark inactive during iteration, compact once after all loops.**

```c
/* Swap-with-tail: overwrites the hole with the last live element.
   O(n), keeps the live prefix dense — iteration stays fast.        */
static void compact_pool(Bullet *pool, int *count) {
  int i = 0;
  while (i < *count) {
    if (!pool[i].active) {
      pool[i] = pool[--(*count)];  /* fill hole with tail */
    } else {
      i++;
    }
  }
}

/* Usage: mark during loops, compact after ALL loops */
for (int i = 0; i < bullet_count; i++) {
  update_bullet(&bullets[i], dt);
  if (out_of_bounds(&bullets[i])) bullets[i].active = 0;
}
compact_pool(bullets, &bullet_count);
```

**Why not `malloc`?** Fixed pools are cache-friendly, never fragment, and can't fail mid-game.

### Struct-of-Arrays (SoA) layout

Array of Structs (AoS) is the default layout and is correct for most games. Use SoA when a simulation iterates N ≥ 1024 particles and the tight inner loop only accesses 1–2 out of many fields per iteration:

```c
/* AoS — standard; all fields for one grain are contiguous */
typedef struct { float x, y; uint8_t color; uint8_t active; } Grain;
Grain grains[MAX_GRAINS];   /* Loop: loads x, y, color, active every iteration */

/* SoA — cache-efficient when a loop only touches x/y and active */
typedef struct {
    float   x[MAX_GRAINS], y[MAX_GRAINS];
    uint8_t color[MAX_GRAINS];
    uint8_t active[MAX_GRAINS];
} GrainPool;
/* Loop: only x[], y[], active[] loaded — color[] stays in cache line */
```

**Trade-off:** SoA is harder to read. `pool.x[i]` vs `grain.x`; no single "grain" variable to inspect in the debugger. Default to AoS; switch to SoA only when profiling shows an AoS layout is a cache bottleneck. Document the choice in the lesson that introduces the pool.

### Data files and designated initialisers

For any game with levels, maps, or large config tables, separate the data into its own translation unit:

```c
/* levels.h */
#define TOTAL_LEVELS 30
typedef struct { int emitter_count; EmitterDef emitters[4]; int par_time; } LevelDef;
extern const LevelDef g_levels[TOTAL_LEVELS];

/* levels.c */
#include "levels.h"
const LevelDef g_levels[TOTAL_LEVELS] = {
    [0] = { .emitter_count = 1, .emitters = { EMITTER(320, 20, 240) }, .par_time = 30 },
    [1] = { .emitter_count = 2, .emitters = { EMITTER(100, 20, 200), EMITTER(540, 20, 280) } },
    /* Fields not listed here are zero-initialised by C99 designated-initialiser rules */
};
```

**Benefits:**

- Only non-default fields appear — eliminates hundreds of `.obstacle_count = 0, .filter_count = 0` lines
- Changing level data doesn't recompile `game.c` (separate translation unit)
- Helper macros (`EMITTER(x, y, angle)`, `CUP(x, y)`, `OBS(x, y, w, h)`) keep the file terse and readable

Declare the array `extern const` in the header and `const` (no `extern`) in the `.c` definition.

### Math utilities

**Reference:** `games/tetris/src/utils/math.h`

```c
#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define MAX(a, b)        ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#define ABS(a)           ((a) < 0 ? -(a) : (a))
```

Tetris's `math.h` only defines `MIN`/`MAX`. Add `CLAMP` and `ABS` when your game needs them; keep it minimal and avoid a dumping-ground header.

---

## Fixed timestep (for physics-heavy games)

**When to use:** any game with rigid-body physics, projectiles with precise collision, or gameplay that must be frame-rate-independent beyond the simple `delta_time` multiplication.

**Why variable `delta_time` breaks physics:** if the player pauses in the debugger for 2 seconds, the next frame's `dt` is 2.0 — an object falls 2 × 9.8 = 19.6 m in one step, tunnelling through floors. Capping `dt` at 0.1 s helps, but fixed timestep is the proper solution.

**Pattern — accumulator:**

```c
#define PHYSICS_DT (1.0f / 120.0f)  /* fixed physics tick: 120 Hz */

/* In main loop: */
float accumulator = 0.0f;

while (running) {
  float frame_dt = get_delta_time();
  if (frame_dt > 0.1f) frame_dt = 0.1f;  /* cap for debugger pauses */
  accumulator += frame_dt;

  /* Fixed-rate physics ticks */
  while (accumulator >= PHYSICS_DT) {
    game_update_physics(&state, PHYSICS_DT);  /* deterministic tick */
    accumulator -= PHYSICS_DT;
  }

  /* Render with interpolation alpha for smooth visuals */
  float alpha = accumulator / PHYSICS_DT;  /* 0.0..1.0 */
  game_render(&backbuffer, &state, alpha);  /* lerp between prev/curr pos */
}
```

**Interpolation in `game_render()`:**

```c
/* Store previous position alongside current: */
typedef struct {
  float x,  y;       /* current physics position */
  float px, py;      /* previous physics position */
} Entity;

/* In game_render() — smooth visual position: */
float draw_x = entity.px + (entity.x - entity.px) * alpha;
float draw_y = entity.py + (entity.y - entity.py) * alpha;
```

For simple games with no fast-moving objects a capped variable `delta_time` is sufficient. Move to fixed timestep when tunnelling or jitter is observed.

---

## Collision detection

**Start simple; add complexity only when the game requires it.**

### AABB vs AABB (axis-aligned bounding boxes)

The default for most 2D games. Fast, simple, no rotation.

```c
typedef struct { float x, y, w, h; } AABB;

int aabb_overlap(AABB a, AABB b) {
  return a.x < b.x + b.w && a.x + a.w > b.x &&
         a.y < b.y + b.h && a.y + a.h > b.y;
}

/* Resolve overlap: push A out of B */
void aabb_resolve(AABB *a, AABB b) {
  float dx_left  = (a->x + a->w) - b.x;       /* overlap from left  */
  float dx_right = (b.x + b.w)   - a->x;      /* overlap from right */
  float dy_up    = (a->y + a->h) - b.y;        /* overlap from below */
  float dy_down  = (b.y + b.h)   - a->y;       /* overlap from above */

  /* Push along the axis of minimum overlap */
  float min_x = dx_left < dx_right ? dx_left : dx_right;
  float min_y = dy_up   < dy_down  ? dy_up   : dy_down;

  if (min_x < min_y)
    a->x += (dx_left < dx_right) ? -dx_left : dx_right;
  else
    a->y += (dy_up   < dy_down)  ? -dy_up   : dy_down;
}
```

### Circle vs Circle

```c
int circles_overlap(float ax, float ay, float ar,
                    float bx, float by, float br) {
  float dx = ax - bx, dy = ay - by;
  float dist2 = dx*dx + dy*dy;
  float sum_r = ar + br;
  return dist2 < sum_r * sum_r;  /* avoid sqrtf */
}
```

### Swept AABB (fast movers — bullets, fast projectiles)

For objects that may tunnel through thin walls in one frame:

```c
/* Returns time of impact [0,1] along the movement vector, or 1.0 if no hit */
float swept_aabb(AABB mover, float vx, float vy, AABB obstacle,
                 float *normal_x, float *normal_y) {
  float dx_entry = (vx > 0) ? obstacle.x - (mover.x + mover.w)
                             : (obstacle.x + obstacle.w) - mover.x;
  float dy_entry = (vy > 0) ? obstacle.y - (mover.y + mover.h)
                             : (obstacle.y + obstacle.h) - mover.y;
  float dx_exit  = (vx > 0) ? (obstacle.x + obstacle.w) - mover.x
                             : obstacle.x - (mover.x + mover.w);
  float dy_exit  = (vy > 0) ? (obstacle.y + obstacle.h) - mover.y
                             : obstacle.y - (mover.y + mover.h);

  float tx_entry = (vx != 0) ? dx_entry / vx : -1e30f;
  float ty_entry = (vy != 0) ? dy_entry / vy : -1e30f;
  float tx_exit  = (vx != 0) ? dx_exit  / vx :  1e30f;
  float ty_exit  = (vy != 0) ? dy_exit  / vy :  1e30f;

  float t_entry = tx_entry > ty_entry ? tx_entry : ty_entry;
  float t_exit  = tx_exit  < ty_exit  ? tx_exit  : ty_exit;

  if (t_entry > t_exit || t_entry >= 1.0f || t_exit <= 0.0f) return 1.0f;

  *normal_x = (tx_entry > ty_entry) ? (dx_entry > 0 ? -1.0f : 1.0f) : 0.0f;
  *normal_y = (ty_entry > tx_entry) ? (dy_entry > 0 ? -1.0f : 1.0f) : 0.0f;
  return t_entry < 0.0f ? 0.0f : t_entry;
}
```

**How to apply the result:**

```c
float nx, ny;
float t = swept_aabb(player_aabb, player.vx * dt, player.vy * dt, wall_aabb, &nx, &ny);

if (t < 1.0f) {
    /* Collision at fraction t — advance only that far */
    player.x += player.vx * dt * t;
    player.y += player.vy * dt * t;
    /* Slide along surface: remove the component in the collision normal direction */
    float remaining = 1.0f - t;
    float dot = (player.vx * remaining * nx + player.vy * remaining * ny);
    player.x += (player.vx * remaining - dot * nx) * dt;
    player.y += (player.vy * remaining - dot * ny) * dt;
    /* Zero velocity in normal direction so object doesn't push into wall next frame */
    if (nx != 0.0f) player.vx = 0.0f;
    if (ny != 0.0f) player.vy = 0.0f;
} else {
    /* No collision — full movement */
    player.x += player.vx * dt;
    player.y += player.vy * dt;
}
```

> `t = 0` means the objects are already overlapping; `t = 1` means no hit this frame. The contact normal `(nx, ny)` is always either `(±1, 0)` or `(0, ±1)` — use it to zero the matching velocity component and slide the remainder along the surface.

**Common bugs table:**

| Symptom                              | Cause                              | Fix                                             |
| ------------------------------------ | ---------------------------------- | ----------------------------------------------- |
| Object passes through walls at speed | No swept collision                 | Use swept AABB or substep movement              |
| Object sticks to walls diagonally    | Resolving both axes simultaneously | Resolve one axis at a time (X first, then Y)    |
| Player clips corners                 | Corner resolution order wrong      | Resolve largest overlap axis first              |
| Jitter when standing on a platform   | Resolve pushes into floor again    | Zero the velocity component after floor contact |

---

## Camera system

A camera converts world coordinates to screen coordinates. Without it, the game is locked to a single screen. Add it in the first lesson that has a world larger than the viewport.

```c
typedef struct {
  float x, y;           /* world position of the camera centre (meters) */
  float width, height;  /* viewport size in world units (meters)         */
  float lerp_speed;     /* 0..1 per second — how fast it tracks the target */
} Camera;

/* Move camera smoothly toward a target (e.g., the player) */
void camera_follow(Camera *cam, float target_x, float target_y, float dt) {
  float t = cam->lerp_speed * dt;
  if (t > 1.0f) t = 1.0f;
  cam->x += (target_x - cam->x) * t;
  cam->y += (target_y - cam->y) * t;
}

/* Convert a world position to a screen pixel (Y-up world, Y-down screen) */
void world_to_screen(Camera *cam, Backbuffer *bb,
                     float wx, float wy, int *sx, int *sy) {
  /* Offset relative to camera centre */
  float rel_x = wx - cam->x;
  float rel_y = wy - cam->y;
  /* Convert to pixels, centred on screen */
  *sx = (bb->width  / 2) + (int)(rel_x * PIXELS_PER_METER);
  *sy = (bb->height / 2) - (int)(rel_y * PIXELS_PER_METER); /* Y-flip */
}
```

**Clamping to world bounds** (prevents showing outside the level):

```c
float half_w = cam->width  / 2.0f;
float half_h = cam->height / 2.0f;
if (cam->x - half_w < 0)           cam->x = half_w;
if (cam->x + half_w > WORLD_WIDTH)  cam->x = WORLD_WIDTH  - half_w;
if (cam->y - half_h < 0)           cam->y = half_h;
if (cam->y + half_h > WORLD_HEIGHT) cam->y = WORLD_HEIGHT - half_h;
```

Replace every `entity.x * PIXELS_PER_METER` in `game_render()` with `world_to_screen(&camera, bb, entity.x, entity.y, &sx, &sy)` the moment you add a camera.

---

## Entity type patterns

When a game has multiple entity types that share some logic but differ in behaviour, use a tagged union or type-enum approach — **not** function pointers or vtables.

### Type-enum + switch (recommended for simple games)

```c
typedef enum {
  ENTITY_PLAYER,
  ENTITY_ENEMY,
  ENTITY_BULLET,
  ENTITY_PICKUP,
} EntityType;

typedef struct {
  EntityType type;
  float x, y, vel_x, vel_y;
  float width, height;
  int   active;
  int   health;

  union {
    struct { float shoot_cooldown; int facing; }         player;
    struct { float patrol_timer; float patrol_range; }   enemy;
    struct { float damage; int owner_type; }             bullet;
    struct { int   pickup_kind; float bob_timer; }       pickup;
  };
} Entity;

#define MAX_ENTITIES 256
static Entity entities[MAX_ENTITIES];
static int    entity_count = 0;

void entity_update(Entity *e, GameState *state, float dt) {
  switch (e->type) {
  case ENTITY_PLAYER: update_player(e, state, dt); break;
  case ENTITY_ENEMY:  update_enemy(e, state, dt);  break;
  case ENTITY_BULLET: update_bullet(e, state, dt); break;
  case ENTITY_PICKUP: update_pickup(e, state, dt); break;
  }
}
```

**When to split into separate pools:** if one type's update function is significantly heavier than others, keeping it in the same array can cause cache misses. Split into `enemies[MAX_ENEMIES]`, `bullets[MAX_BULLETS]`, etc. when profiling shows a bottleneck — not before.

---

## Particle system

Particles add visual feedback (explosions, sparks, dust, trails) without gameplay impact. Keep them in a separate fixed pool that is completely independent of game logic.

```c
#define MAX_PARTICLES 256

typedef struct {
  float x, y;
  float vel_x, vel_y;
  float lifetime;       /* seconds remaining */
  float max_lifetime;   /* total duration — used for alpha fade ratio */
  uint32_t color;       /* base color */
  float size;           /* radius or half-width in pixels */
  int   active;
} Particle;

static Particle particles[MAX_PARTICLES];

void spawn_particle(float x, float y, float vx, float vy,
                    float lifetime, uint32_t color, float size) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) {
      particles[i] = (Particle){x, y, vx, vy, lifetime, lifetime, color, size, 1};
      return;
    }
  }
  /* Pool full: silently drop — particles are cosmetic, never gameplay-critical */
}

void update_particles(float dt) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) continue;
    particles[i].x += particles[i].vel_x * dt;
    particles[i].y += particles[i].vel_y * dt;
    particles[i].vel_y -= 2.0f * dt;  /* optional: light gravity */
    particles[i].lifetime -= dt;
    if (particles[i].lifetime <= 0.0f) particles[i].active = 0;
  }
}

void render_particles(Backbuffer *bb, float alpha /*unused here*/) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) continue;
    /* Fade alpha based on remaining lifetime */
    float t   = particles[i].lifetime / particles[i].max_lifetime;
    uint32_t a = (uint32_t)(t * 255.0f);
    uint32_t c = (particles[i].color & 0x00FFFFFF) | (a << 24);
    int size  = (int)particles[i].size;
    draw_rect_blend(bb, (int)particles[i].x - size/2,
                        (int)particles[i].y - size/2, size, size, c);
  }
}
```

**Spawn helpers for common effects:**

```c
void spawn_explosion(float x, float y, uint32_t color, int count) {
  for (int i = 0; i < count; i++) {
    float angle = ((float)rand() / RAND_MAX) * 6.2832f;
    float speed = 30.0f + ((float)rand() / RAND_MAX) * 80.0f;
    spawn_particle(x, y, cosf(angle)*speed, sinf(angle)*speed,
                   0.4f + ((float)rand()/RAND_MAX)*0.4f, color, 3.0f);
  }
}
```

---

## Debug visualization

Inline debug rendering makes physics, collision, and AI bugs instantly visible without a separate debugger tool.

```c
#ifdef DEBUG
/* Draw all entity AABBs as coloured outlines */
void debug_draw_hitboxes(Backbuffer *bb, Entity *entities, int count) {
  for (int i = 0; i < count; i++) {
    if (!entities[i].active) continue;
    int x = (int)(entities[i].x * PIXELS_PER_METER);
    int y = (int)(entities[i].y * PIXELS_PER_METER);
    int w = (int)(entities[i].width  * PIXELS_PER_METER);
    int h = (int)(entities[i].height * PIXELS_PER_METER);
    uint32_t col = (entities[i].type == ENTITY_PLAYER) ? COLOR_GREEN : COLOR_RED;
    /* Draw four 1-pixel-thick edges */
    draw_rect(bb, x,       y,       w, 1, col);
    draw_rect(bb, x,       y+h-1,   w, 1, col);
    draw_rect(bb, x,       y,       1, h, col);
    draw_rect(bb, x+w-1,   y,       1, h, col);
  }
}

/* Draw velocity vectors */
void debug_draw_velocities(Backbuffer *bb, Entity *entities, int count) {
  for (int i = 0; i < count; i++) {
    if (!entities[i].active) continue;
    int cx = (int)(entities[i].x * PIXELS_PER_METER);
    int cy = (int)(entities[i].y * PIXELS_PER_METER);
    int vx = cx + (int)(entities[i].vel_x * 4.0f);  /* scale for visibility */
    int vy = cy - (int)(entities[i].vel_y * 4.0f);  /* Y-flip */
    /* Draw a simple line using draw_rect for each pixel (or a proper line fn) */
    draw_rect(bb, cx, cy, vx - cx, 1, COLOR_YELLOW);
  }
}
#endif
```

Add a `debug_key` to `GameInput` and call these inside `#ifdef DEBUG` in `game_render()`. Remove the conditional entirely in release — zero cost.

These principles apply to every piece of generated code. They come directly from Casey Muratori's approach in Handmade Hero — prioritize explicit control over convenience abstractions.

### Code as data transformation

Every frame is a pure transformation: `input + old_state → new_state + output_buffer`. There is no hidden state, no callbacks firing at unknown times, no objects notifying other objects. The entire frame is visible in `game_update()` and `game_render()`.

```
Platform layer (I/O, timing, OS events)
        ↓ raw input, delta_time
   game_update   →   GameState (mutated in place)
        ↓
   game_render   →   Backbuffer (written pixel by pixel)
        ↓
Platform layer (upload backbuffer, submit audio)
```

**Rule:** The game layer never calls platform APIs. The platform layer calls game functions and passes results down.

### Memory layout over object hierarchy

Prefer flat arrays to linked lists. Prefer linear access over pointer chasing. When you need polymorphism, use an enum + switch, not vtables.

```c
/* AVOID: linked list — pointer chase every node */
typedef struct Entity { struct Entity *next; ... } Entity;

/* PREFER: flat array — full cache line utilization */
#define MAX_ENTITIES 1024
typedef struct { float x[MAX_ENTITIES]; float y[MAX_ENTITIES]; int active[MAX_ENTITIES]; } Entities;

/* Or the simpler AoS version when fields are always accessed together */
typedef struct { float x, y; int active; } Entity;
static Entity entities[MAX_ENTITIES];
```

Use SoA (struct-of-arrays) when you iterate over one field of many objects (e.g., updating all X positions). Use AoS (array-of-structs) when you process all fields of one object together (e.g., rendering a single entity). For simple games, AoS is usually sufficient.

### Explicit ownership and lifetime

Every allocation has a clear owner and a clear free point. Prefer stack allocation and fixed arrays. Use `malloc` only at startup; never inside the game loop.

```c
/* Good: fixed lifetime, allocated once at startup */
static GameState g_state;

/* Good: stack allocation for temporaries */
char score_text[32];
snprintf(score_text, sizeof(score_text), "SCORE %d", state->score);

/* Avoid: malloc inside the game loop */
for (int i = 0; i < n; i++) {
  char *s = malloc(...);  /* DON'T — heap pressure every frame */
}
```

### Introspectable state

At any frame you should be able to print all state that matters. Keep state in plain structs. Avoid hiding important values inside function-local statics or OS objects.

```c
/* Debug print: add to game_update() under a keypress */
#ifdef DEBUG
if (input->debug_dump) {
  printf("phase=%d score=%d level=%d drop_t=%.3f\n",
         state->phase, state->score, state->level,
         state->drop_timer.timer);
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    printf("  sound[%d] id=%d rem=%d\n", i,
           state->audio.active_sounds[i].sound_id,
           state->audio.active_sounds[i].samples_remaining);
  }
}
#endif
```

### One allocation, one scope

Avoid deeply nested allocations. Ideally the only `malloc` is in `platform_game_props_init()` for the backbuffer and audio sample buffer. Everything else is a fixed global or stack variable.

### When to deviate

- Procedural/infinite worlds: use chunk + offset, accept the complexity
- Networked games: snapshots and rollback require copies of state
- Very large entity counts: profile first; the simple flat array is usually fast enough

**Handmade Hero reference:** Episodes 1–40 cover the platform layer, backbuffer pipeline, and input system. Episodes 41–60 cover the game loop timing model and the audio latency approach used here. Watch the "Compilation" episodes for build philosophy.

---

## Lesson writing quality standards (mandatory)

These rules govern how every lesson is written. They prevent the most common failure modes: lessons that skip context, truncate code, assume memory of earlier material, or leave gaps the student cannot cross without reading source code.

**These are not suggestions.** Every generated lesson must pass the per-lesson checklist at the bottom of this section before it is written into the course output.

---

### Rule 1 — Complete code, never truncated

**Never** use placeholder comments like `/* ...existing code... */`, `// rest stays the same`, `/* ...unchanged... */`, or bare ellipsis (`...`). These are invisible landmines — the beginner cannot distinguish "unchanged from last lesson" from "you must write this yourself."

**Two acceptable choices:**

**Choice A — Show the entire file.** Use this for any file ≤ 120 lines, or any time more than one third of the file changed.

```c
/* Complete src/game.h after Lesson 3 */
#ifndef GAME_H
#define GAME_H
/* ... every line shown ... */
#endif /* GAME_H */
```

**Choice B — Show a clearly-bounded delta block.** When the file is large and the change is surgical, show ≥ 5 lines of unchanged context **before** and ≥ 5 lines **after** the changed block. Mark every added or changed line with an inline comment:

```c
/* game.h — only GameState changes in this lesson; everything else is unchanged */
typedef struct {
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
  CurrentPiece  current_piece;
  int score;
  int level;                    /* NEW: track current difficulty level */
  int lines_cleared_total;      /* NEW: total lines cleared (persists across levels) */
  /* ...all other fields are identical to Lesson 4... */
} GameState;
```

When Choice B is used, add a callout box **at the top of the lesson**:

> **File display policy:** Only the changed portions of `game.h` and `game.c` are shown. All other files and all unshown lines within those files are identical to Lesson N-1.

---

### Rule 2 — No forward references without anchors

If a lesson uses a type, macro, or function introduced in an earlier lesson, it must either:

- Include a one-line back-reference: _"Recall `GAME_PHASE` from Lesson 3 — we now add a `PAUSED` case."_
- Or re-show the original definition with `/* From Lesson N — unchanged */` comment.

**Never assume the student remembers** anything introduced more than two lessons ago. Re-anchor it in prose or code.

---

### Rule 3 — "Where we are" opener (every lesson after Lesson 1)

Every lesson must begin with a clearly-labelled **project state block** that shows:

1. The complete current file tree (every file that exists at that point)
2. One sentence describing what the program does when built and run
3. The exact build command

````markdown
> **Project state entering this lesson**
>
> ```
> src/
> ├── main_raylib.c
> ├── main_x11.c
> ├── platform.h
> ├── game.h
> ├── game.c
> └── utils/
>     ├── backbuffer.h
>     └── draw-shapes.c
> build-dev.sh
> ```
>
> **What it does:** A dark-gray window opens. A white rectangle moves with arrow keys. No game logic yet.
>
> **Build:** `./build-dev.sh --backend=raylib -r`
````

This block eliminates the most common student question: _"Which files should I have at this point?"_

---

### Rule 4 — Concept limit per lesson

Each lesson introduces at most **2–3 new C language or architecture concepts**. If a lesson requires more, split it.

- **Bad:** "Lesson 5: the game loop, delta-time, double-buffered input, and the platform contract"
- **Good:** "Lesson 5: the game loop and delta-time" + "Lesson 6: double-buffered input and the platform contract"

There is no target lesson count and no upper limit on lesson count. A source with many subsystems will produce many lessons. Every subsystem, every function, every non-obvious struct field that is part of the source gets its own coverage — there is no "minor detail" that is silently skipped. Splitting is always preferred over overloading.

List the new concepts explicitly in the "What you'll learn" section. For each concept, provide:

- A JS analogy (where one exists)
- A code example with inline `/* why */` comments
- At least one observable outcome the student can see or hear

---

### Rule 5 — Self-contained compilation

**Every lesson must compile and run to a visible result.** Never leave the student with code that compiles silently but shows nothing, crashes, or requires "fix this next lesson."

If a lesson necessarily introduces a stub, make its behavior explicit:

- _"The window will be all-black — that is correct. We will add drawing in the next lesson."_
- Or add a visible placeholder: a single `draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_DARK_GRAY)` so _something_ appears.

---

### Rule 6 — Both backends, always, before moving on

Every lesson section that contains platform-specific code must include **both the X11 and Raylib versions before moving to the next section**. Never write one backend and defer the other to "later in the lesson" or "the next lesson."

Acceptable section layout:

```markdown
#### Step 2: Reading keyboard input

##### Raylib

[complete Raylib code block]

**What's happening (Raylib):**
[bullet explanations]

##### X11

[complete X11 code block]

**What's happening (X11):**
[bullet explanations]

> **Only following one backend?** You can skip the other section, but read the "Key difference" note immediately below.

**Key difference:** Raylib polls every frame; X11 receives discrete events. Both must preserve `ended_down` across frames — the mechanism differs but the contract is identical.
```

The only exception: sections explicitly labelled **"Identical on both backends — Raylib code applies as-is."**

---

### Rule 7 — Expected output after every step

Every "Step N" subsection must end with an **Expected output** statement:

```markdown
**Expected:** The window background changes from black to dark gray. No other visual change.
```

```markdown
**Expected:** Pressing the left arrow key moves the rectangle left at a constant speed. Releasing it stops movement immediately.
```

If a step makes no visible change, say so explicitly:

```markdown
**Expected:** Visually identical to before. The benefit is structural — `game.c` now contains zero platform calls.
```

This eliminates the most common panic from students: _"Nothing changed — did I do it wrong?"_

---

### Rule 8 — Exercises that extend, not replace

Every lesson must include exactly three exercises:

1. **Beginner** — a one-line or minimal change using a concept from this lesson. Example: _"Change the background color to navy blue (`GAME_RGB(0, 0, 100)`)."_
2. **Intermediate** — extends the feature built in the lesson. Example: _"Add a second rectangle that moves with WASD keys, independent of the first."_
3. **Challenge** — combines concepts from this lesson and the previous one. Example: _"Make the rectangle bounce off all four walls and change color on each bounce."_

Each exercise must state:

- What the student observes when it works correctly
- Which specific lines or functions to modify

---

### Rule 9 — Prior-lesson vocabulary recap

At the start of every lesson after the first, include a one-paragraph recap using the exact terminology established in previous lessons. This reinforces vocabulary and prevents drift:

```markdown
In the previous lesson we built a `Backbuffer` — a `uint32_t *` pixel array the game writes each frame — and implemented `draw_rect()`. We also introduced `platform.h` as the contract between `main_raylib.c`/`main_x11.c` and `game.c`. This lesson adds `GameInput` and connects keyboard events to it.
```

---

### Rule 10 — No magic numbers without explanation

Every numeric literal in lesson code must be explained inline or in the paragraph immediately surrounding it. No exceptions.

```c
/* BAD */
state->drop_timer.interval = 0.8f;

/* GOOD */
state->drop_timer.interval = 0.8f;  /* pieces fall once every 0.8 seconds at level 1 */
```

Well-known constants still require explanation:

```c
int samples_per_second = 44100;  /* CD-quality audio: 44,100 PCM samples played per second */
float gravity = -9.8f;           /* Earth-surface gravitational acceleration in m/s² */
```

---

### Rule 11 — "What's next" closer

Every lesson must end with one or two sentences connecting the last concept of this lesson to the opening concept of the next. This prevents each lesson from feeling like a disconnected unit.

```markdown
## What's next

We now have a moving rectangle — but its speed is tied to the frame rate. In Lesson 4 we introduce `delta_time` so the game runs at the same speed on a 30 fps laptop and a 240 fps gaming monitor.
```

---

### Recommended lesson-writing order

When generating a lesson, follow this sequence to avoid going back and rewriting:

1. Write the **"Where we are"** callout from the previous lesson's final state.
2. Write **"What we're building"** — one sentence describing the end state.
3. List **"What you'll learn"** — ≤ 3 concepts.
4. Write each **Step** in order:
   - Raylib version complete
   - X11 version follows immediately
   - "Expected output" stated
5. Write the **"Build and run"** section with exact command and exact described output.
6. Write **three exercises** (beginner / intermediate / challenge).
7. Write **"What's next"** — tie the last concept of this lesson to the first concept of the next.
8. Check the **per-lesson checklist** below.

---

### Per-lesson checklist (reviewer's gate)

Before writing a lesson into the course output, verify every item:

- [ ] "Where we are" callout shows the current file tree and one-sentence program description
- [ ] "What you'll learn" lists ≤ 3 new concepts — if the material requires more, the lesson has already been split in PLAN.md before writing begins
- [ ] All new concepts have a JS-equivalent analogy where one exists
- [ ] No code block uses `...`, `// existing code`, or equivalent — or a clear diff-policy declaration is at the top of the lesson
- [ ] Every new concept has a code example with inline `/* why */` comments
- [ ] Both X11 and Raylib versions present for every platform-specific section
- [ ] Every step has an "Expected output" line
- [ ] The lesson compiles and runs to a visible (or audible) result
- [ ] No forward reference to a concept not yet introduced — or an explicit back-reference callout
- [ ] All numeric literals are annotated with meaning or units
- [ ] Exercises reference actual code from this lesson
- [ ] "What's next" previews the next lesson's opening concept
- [ ] Prior-lesson vocabulary recap paragraph present
- [ ] No step is skippable without breaking a later step — progression is linear and explicit

---

## Lesson structure

Each lesson should follow this structure:

### Lesson file template

````markdown
# Lesson XX: [Title]

> **Project state entering this lesson**
>
> ```
> src/
> ├── main_raylib.c
> ├── main_x11.c
> ├── platform.h
> ├── game.h
> ├── game.c
> └── utils/
>     └── backbuffer.h
> build-dev.sh
> ```
>
> **What it does:** [One sentence — what runs when you build and run right now]
>
> **Build:** `./build-dev.sh --backend=raylib -r`

> **File display policy for this lesson:** [Either "All files shown in full" or "Only changed
>
> > sections shown; unshown lines in `game.h` and `game.c` are identical to Lesson XX-1."]

---

[One paragraph recap of the previous lesson's key concepts and terminology, anchoring
the vocabulary the student has already seen:]

_In the previous lesson we built a `Backbuffer` — a `uint32_t _`pixel array the game writes each
frame — and implemented`draw_rect()`. We also introduced `platform.h`as the contract between`main_raylib.c`/`main_x11.c`and`game.c`. This lesson adds `GameInput` and connects keyboard
events to it.\*

---

## What we're building

[One paragraph describing the concrete addition and the observable outcome the student will have at
the end of this lesson. Be specific: name the files changed, the new functions introduced, and what
the player sees or hears.]

## What you'll learn

- [Concept 1 — include JS equivalent if one exists, e.g. "`GameButtonState` — like a JS
  `{ isDown, justPressed }` object, but fixed-size on the stack"]
- [Concept 2]
- [Concept 3] ← maximum 3; split the lesson if more are needed

## Prerequisites

- Completed Lesson XX-1 (you should have [describe the current visible state])
- [Any specific prior concept to recall]

---

## Step 1: [First major step]

[Explanation of what we're doing and why — 2–4 sentences. Reference the JS equivalent where
relevant. State what problem this step solves.]

#### Raylib

```c
/* src/main_raylib.c — [describe which part of the file this is] */
/* Complete file shown / Only the changed region shown (see display policy above) */

/* ... code with inline /* why */ comments on every non-obvious line ... */
```

**What's happening (Raylib):**

- [Bullet: key mechanism or concept]
- [Bullet: why this is done this way and not another way]
- [Bullet: common mistake to avoid]

#### X11

```c
/* src/main_x11.c — [describe which part of the file this is] */

/* ... code with inline /* why */ comments ... */
```

**What's happening (X11):**

- [Key difference from Raylib version]
- [Anything that surprised you that the student should know]

**Key difference:** [One sentence summarizing the fundamental distinction between how the two
backends handle this step — e.g., "Raylib polls once per frame; X11 receives a `KeyPress` event
only when the key goes down. Both update the same `GameButtonState` field."]

**Expected:** [Exactly what the student sees or hears at the end of this step — or explicitly
"No visible change yet — that happens in Step 2."]

---

## Step 2: [Second major step]

[Same structure as Step 1: explanation → Raylib code → X11 code → Key difference → Expected]

---

## [Additional steps as needed — each follows the same pattern]

---

## Build and run

```bash
./build-dev.sh --backend=raylib -r
```

**Expected:** [Precise description — what color is the background, what does the player control
look like, what happens when the user presses the key. Be specific enough that the student can
verify they got it right without asking anyone.]

```bash
# Optional: test X11 backend too
./build-dev.sh --backend=x11 -r
```

**Expected (X11):** [State whether the result is identical or note any visible difference.]

---

## Exercises

1. **Beginner:** [One-line or minimal change using a concept from this lesson. State exactly which
   constant or variable to change and what the student will see.
   Example: *Change the background color to navy blue. Hint: use `GAME_RGB(0, 0, 100)` in
   `game_render()`.*]

2. **Intermediate:** [Extends the feature built in this lesson. Names the function or struct to
   modify. States the observable outcome.
   Example: *Add a second rectangle controlled by WASD keys, independent of the first. Add a
   second `pos_x`/`pos_y` pair to `GameState` and a second move block in `game_update()`.*]

3. **Challenge:** [Combines concepts from this lesson and the previous one. May require thinking,
   not just copying.
   Example: *Make the rectangle bounce off all four walls and change to a random color on each
   bounce. Use `GAME_RGB(rand()%256, rand()%256, rand()%256)` and negate the velocity.*]

---

## What's next

[One or two sentences. The last concept introduced in this lesson leads directly into the first
concept of the next. Example: *"We now have a moving rectangle, but its speed is tied to the frame
rate — run the game on a faster machine and it moves faster. In Lesson 4 we introduce `delta_time`
so the game runs at the same speed everywhere."*]
````

### Lesson progression guidelines

#### How to size the lesson plan

The lesson count is driven entirely by the source material, not by a target number. Use this process:

1. **Enumerate every topic.** From the full topic inventory in `PLAN.md`, list every concept, function, struct, algorithm, and source-file section that a student needs to understand.

2. **Cluster by dependency.** Group topics that must be learned together (e.g., `Backbuffer` + `GAME_RGB` + `draw_rect` form one natural cluster). A cluster that has no runnable endpoint must be merged with the next cluster. A cluster that introduces more than 3 new concepts must be split.

3. **Assign one lesson per cluster.** The resulting lesson count is whatever the material requires. A small utility game may need 8–12 lessons; a game with a full audio system, particle system, and camera may need 20–30. Both are correct outcomes. Never collapse two unrelated clusters to reduce count; never add filler to increase it.

4. **Verify coverage.** Every item in the topic inventory must map to exactly one lesson in the skill inventory table. Any unmapped item is a gap that must be closed by adding a lesson or expanding an existing one.

#### Canonical opening sequence (adapt as needed)

The first four lessons form a universal foundation for any game. Everything after Lesson 4 is determined by the game's own topic inventory:

1. **Lesson 1**: Platform init + colored window (proof that the toolchain works; backbuffer concept)
2. **Lesson 2**: First visible game object drawn (draw primitive; color macros; game/platform separation)
3. **Lesson 3**: Object responds to keyboard (input types; `GameButtonState`; `prepare_input_frame`)
4. **Lesson 4**: `GameState` struct + game loop wired up (`game_update` / `game_render` split; `GAME_PHASE` introduced)
5. **Lessons 5–N**: One cluster per lesson, in dependency order, until every topic in the inventory is covered

> ⚠ **Do not stop at Lesson 5.** "Game-specific features" is not a single lesson — it is every remaining cluster in the topic inventory, each given its own lesson. A source file with 300 lines of game logic will typically produce 4–8 lessons on its own.

#### Splitting an oversized cluster

If a cluster would introduce more than 3 new concepts, split at the first natural seam:

- **Audio:** split into (1) PCM mixer + `SoundInstance` + `game_play_sound` and (2) `MusicSequencer` + `ToneGenerator` + `game_audio_update`
- **Input:** split into (1) `GameButtonState` + `UPDATE_BUTTON` + `prepare_input_frame` and (2) `RepeatInterval` + `handle_action_with_repeat` + DAS/ARR timing
- **State machine:** split into (1) `GAME_PHASE` enum + `switch` dispatch and (2) `change_phase` + audio transitions + per-phase entry logic
- **Collision:** split into (1) AABB overlap + resolve and (2) swept AABB for fast movers

Document every split in `PLAN.md` with a one-line reason.

#### Merging an undersize cluster

If a cluster produces no visible or audible result on its own (e.g., adding a `#define` constant or a helper macro), merge it into the adjacent lesson that first uses it. Never write a standalone lesson that compiles but shows nothing.

**Each lesson must:**

- Introduce at most 2–3 new concepts — split the lesson if more are needed
- End with something the student can see or hear (or explicitly state why the lesson sets up a visible result in the next step)
- Include exercises at three levels (beginner / intermediate / challenge)
- Explicitly connect to the previous lesson and preview the next
- Cover both X11 and Raylib backends or explicitly note "identical"

### Platform Coverage Checklist (per lesson)

Any course with two backends must cover **both** in every lesson — or explicitly note "Identical on both backends". Deferring X11 coverage to later consistently requires expensive rewrites to multiple lessons.

For any lesson that touches these areas, check both backends are covered:

- [ ] Window creation / teardown
- [ ] Frame timing (`clock_gettime` vs `GetTime`)
- [ ] Input reading (event loop vs `IsKeyDown`)
- [ ] Pixel / texture rendering
- [ ] Audio push (ALSA write vs Raylib stream)
- [ ] Shutdown / cleanup

**Rule:** Draft the X11 section immediately after the Raylib section, before moving to the next lesson. Never leave it for the end.

---

## Quick reference table

| Need                                      | Pattern                                                                                              |
| ----------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| Platform-independent rendering            | Backbuffer with `uint32_t *pixels`                                                                   |
| Color constants                           | `GAME_RGB(r,g,b)` macro + named constants                                                            |
| Frame timing                              | `delta_time` from platform, cap at 0.1f                                                              |
| Button state                              | `GameButtonState` with `ended_down` + `half_transition_count`                                        |
| Previous frame's input                    | Double-buffered `GameInput` with swap                                                                |
| Auto-repeat with delay                    | `RepeatInterval` with `initial_delay` + `passed_initial_delay`                                       |
| Event-based input (X11)                   | Preserve `ended_down` in `prepare_input_frame`                                                       |
| Polling input (Raylib)                    | Call `UPDATE_BUTTON` every frame with current state                                                  |
| Type-safe constants                       | `typedef enum { ... } NAME;`                                                                         |
| Debug assertions                          | `ASSERT()` macro with `DEBUG_TRAP()`                                                                 |
| Sound effects                             | `SoundDef` table + `game_play_sound_at(audio, id, pan)`                                              |
| Centered sound (no pan)                   | `game_play_sound(audio, id)` — wrapper with `pan = 0.0`                                              |
| Spatial panning                           | `calculate_piece_pan(x)` → `pan_position` on `SoundInstance`                                         |
| Click-free audio                          | `fade_in_samples = sps/100`; loop ramps both ends                                                    |
| Background music                          | `MusicSequencer` with pattern data + `ToneGenerator`                                                 |
| Audio latency budget                      | `PlatformAudioConfig`: `latency_samples = (sps/hz)*frames`                                           |
| Multiple exclusive game states            | `GAME_PHASE` enum + `switch` dispatch in `game_update()`                                             |
| Many short-lived objects                  | Fixed free-list pool (no `malloc`)                                                                   |
| Semi-transparent overlay                  | `draw_rect_blend()` with alpha                                                                       |
| Text rendering                            | 8x8 bitmap font + `draw_text()`                                                                      |
| Resolution independence                   | World units + `PIXELS_PER_METER` conversion                                                          |
| VSync with fallback                       | Detect extension, manual sleep if unavailable                                                        |
| Integer to display string                 | `snprintf(buf, sizeof(buf), "%d", val)` + `draw_text()`                                              |
| X11 keysym (modifier-safe)                | `XkbLookupKeySym(display, keycode, 0, NULL, &key)` — ignores CapsLock                                |
| X11 window close button                   | `XInternAtom` `WM_DELETE_WINDOW` + handle `ClientMessage` in event loop                              |
| ALSA silent startup (~0.5 s)              | `snd_pcm_sw_params_set_start_threshold(..., 1)` — always; call `current()` first                     |
| Audio silent after restart                | Save `samples_per_second` before `memset(state)`; restore after                                      |
| Phase accumulator oscillator              | `phase += freq * inv_sr; if (phase >= 1.0f) phase -= 1.0f;`                                          |
| Square wave from phase                    | `(phase < 0.5f) ? 1.0f : -1.0f`                                                                      |
| Volume ramping (no click)                 | `ToneGenerator.current_volume` → `target_volume` per sample                                          |
| Two-loop audio (seq + PCM)                | `game_audio_update` (game time) + `game_get_audio_samples` (audio time)                              |
| MIDI note → Hz                            | `440.0f * powf(2.0f, (note - 69) / 12.0f)`                                                           |
| preview (example: Ghost piece algorithm)  | Re-apply fall until collision, render at final pos w/ alpha                                          |
| Exponential line-clear scoring            | `(1 << lines_cleared) * BASE_SCORE` — doubles per extra line                                         |
| Sustained/held-key sound (engine, thrust) | `game_sustain_sound` — pin `samples_remaining ≥ total/4` while key held                              |
| Looping background sound (no buzz)        | `WAVE_TRIANGLE` + `ENVELOPE_TRAPEZOID`; `volume ≥ 0.50`                                              |
| Sound pool overflow (silent drops)        | `ASSERT(slot >= 0)` at insertion; `MAX_SIMULTANEOUS_SOUNDS ≥ 16`                                     |
| Window resize without art breaking        | Letterbox (`glViewport` / `DrawTexturePro`) from lesson 1                                            |
| Directional vector from rotation angle    | Derive from 2×2 rotation matrix; document convention; test at `a=0`                                  |
| Safe pool mutation during iteration       | `compact_pool` (swap-with-tail) — mark during loops, compact after                                   |
| Y-up to screen conversion                 | `screen_y = (bb->height - 1) - (int)(world_y * PIXELS_PER_METER)`                                    |
| World units → pixels                      | `px = (int)(x * PIXELS_PER_METER)`; only in `game_render()`                                          |
| Camera world-to-screen                    | `sx = bb->width/2 + (wx - cam.x)*PPM`; `sy = bb->height/2 - (wy - cam.y)*PPM`                        |
| Camera follow (smooth lerp)               | `cam.x += (target_x - cam.x) * lerp_speed * dt`                                                      |
| AABB overlap test                         | `a.x < b.x+b.w && a.x+a.w > b.x && a.y < b.y+b.h && a.y+a.h > b.y`                                   |
| AABB resolve (minimum overlap axis)       | Push along the axis with the smallest overlap distance                                               |
| Fast mover tunnelling                     | Swept AABB collision; returns `t` in `[0,1]` and contact normal                                      |
| Fixed timestep accumulator                | `accumulator += dt; while (acc >= PHYS_DT) { tick(PHYS_DT); acc -= PHYS_DT; }`                       |
| Spawn particles                           | Fixed pool `particles[MAX]`; find inactive slot; set lifetime and velocity                           |
| Particle alpha fade                       | `alpha = (uint32_t)(lifetime / max_lifetime * 255.0f); color = (base & 0x00FFFFFF) \| (alpha << 24)` |
| Multiple entity types                     | `EntityType` enum + `union` for type-specific data; `switch` dispatch in update                      |
| Sustained sound (engine, thrust)          | `game_sustain_sound_update()` — pin `samples_remaining` while active                                 |
| Music volume fade/duck                    | `audio_ramp_music_volume(audio, target, rate, dt)` per frame                                         |
| Game-state audio transition               | Trigger in `change_phase()` — single place for all audio/state coupling                              |

---

## Common bugs and fixes

| Symptom                                          | Cause                                                                    | Fix                                                                                   |
| ------------------------------------------------ | ------------------------------------------------------------------------ | ------------------------------------------------------------------------------------- |
| Held key acts like repeated presses              | `ended_down` not preserved in `prepare_input_frame`                      | Copy from `old_input` before resetting transitions                                    |
| Key only works on first press (X11)              | Event-based input loses state between frames                             | Ensure `prepare_input_frame` copies `ended_down`                                      |
| Auto-repeat doesn't work                         | `passed_initial_delay` not tracked                                       | Add flag to `RepeatInterval`, check in handler                                        |
| Repeat triggers immediately despite delay        | Early return when `half_transition_count > 0`                            | Only trigger immediately if `initial_delay <= 0`                                      |
| Input feels "sticky" after release               | Buffer swap not happening correctly                                      | Verify swapping contents, not pointers                                                |
| Colors look wrong                                | RGBA vs ARGB byte order mismatch                                         | Check platform expects `0xAARRGGBB`; test with pure-red rect on BOTH backends         |
| Red and blue channels swapped (one backend only) | Second backend uses different pixel format than first                    | Validate `GAME_RGBA` macro end-to-end on each backend before building rendering       |
| Window close button does nothing (X11)           | `WM_DELETE_WINDOW` protocol not registered                               | `XInternAtom` + `XSetWMProtocols`; handle `ClientMessage` in event loop               |
| Keys wrong with CapsLock/NumLock on (X11)        | `XLookupKeysym` respects modifier state                                  | Use `XkbLookupKeySym(display, keycode, 0, NULL, &key)` instead                        |
| Game runs too fast without VSync                 | No frame limiting                                                        | Add manual sleep when VSync unavailable                                               |
| ALSA audio silent for first ~100 ms              | Default `start_threshold = 1` starts playback too early                  | Set `start_threshold = hw_buffer_size`; pre-fill buffer with silence before start     |
| Raylib audio repeats or skips every ~1 s         | `sample_count` passed to fill callback doesn't match stream registration | Store `buffer_size_frames` at init; use it for every `game_get_audio_samples` call    |
| Audio permanently silent after restart           | `memset(state, 0)` in `game_init` zeroes `samples_per_second`            | Save audio config before memset; restore `samples_per_second` + `master_volume` after |
| Audio clicks/pops at sound start                 | `phase` reset to 0 on slot reuse                                         | Never reset `phase`; it ticks continuously per `SoundInstance`                        |
| Audio clicks/pops at sound end                   | Abrupt cutoff — no fade-out                                              | Set `fade_in_samples`; loop uses `samples_remaining` fade-out                         |
| Audio clicks/pops on buffer boundary             | Wave sample discontinuity between fills                                  | `phase` persists across fills — never reset it                                        |
| Audio plays mono instead of stereo               | Both channels written with same mono sample                              | Use separate `left`/`right` accumulators; apply `pan_position`                        |
| Sound pos not panning                            | `game_play_sound()` used instead of `game_play_sound_at()`               | Call `game_play_sound_at()` with spatial pan value                                    |
| Game freezes during line clear                   | Animation timer not decrementing                                         | Ensure `delta_time` subtraction in animation state                                    |
| Piece spawns inside wall                         | Initial position not accounting for piece size                           | Center based on piece width, not field center                                         |
| Looping SFX sounds buzzy/harsh                   | `WAVE_SQUARE` used for sustained background sound                        | Switch to `WAVE_TRIANGLE`; reserve square waves for short SFX (duration < 0.3 s)      |
| Looping SFX throbs (loud→quiet→loud)             | Short clip re-triggered with `ENVELOPE_FADEOUT` every 100–300 ms         | Use `game_sustain_sound` + `ENVELOPE_TRAPEZOID` for held-key sounds                   |
| Looping sound inaudible despite volume set       | Frequency below ~80 Hz; laptop speakers vibrate, not ring                | Minimum looping tone frequency: 100 Hz                                                |
| Object moves in wrong direction after rotation   | Sign of `sinf`/`cosf` was guessed, not derived from rotation matrix      | Derive `dx = +sinθ, dy = −cosθ` from matrix; sanity test at angle = 0                 |
| Wrong element updated after pool removal         | Element removed during iteration; tail-swap element at index skipped     | Mark inactive during loops; call `compact_pool` once after all loops                  |
| Object tunnels through wall at high speed        | No swept collision; AABB only checks end position                        | Use swept AABB; or subdivide movement into smaller steps                              |
| Physics jittery or explodes at low frame rate    | `delta_time` too large; Euler integration diverges                       | Cap `dt` at 0.1 s; switch to fixed-timestep accumulator for physics                   |
| World position in pixels, breaks on resize       | Physics/logic using pixel values directly                                | Move to world units (meters); convert only in `game_render()` via `PIXELS_PER_METER`  |
| Gravity acts differently on fast vs slow machine | `delta_time` accumulated over multiple physics steps                     | Use fixed-timestep accumulator so physics ticks are deterministic                     |
| Music abruptly stops / starts on phase change    | `game_music_play()`/`stop()` called directly in `game_update()` logic    | Move all audio triggers into `change_phase()` — one central place                     |
| Game has only 2–3 sounds (feels empty)           | Audio planned as afterthought; `SOUND_COUNT` too small                   | Plan sound set from PLAN.md: actions, hits, UI, transitions, music (≥6–8 minimum)     |

---

## Checklist before submitting course

### Lesson writing quality (applies to every lesson)

- [ ] Every lesson has a "Where we are" callout showing the file tree and one-sentence program description
- [ ] No lesson code block uses `...`, `// existing code`, or similar truncation without a diff-policy declaration at the top of that lesson
- [ ] Every step in every lesson has an "Expected output" line describing the visible or audible result
- [ ] No forward reference to a concept introduced later — or an explicit back-reference callout is present
- [ ] Every lesson after the first has a prior-lesson vocabulary recap paragraph
- [ ] Every numeric literal in every lesson is annotated with its meaning or units
- [ ] Every lesson introduces ≤ 3 new concepts — any lesson that needed more was split during PLAN.md planning, increasing the total lesson count
- [ ] Every lesson has exactly 3 exercises (beginner / intermediate / challenge) referencing actual lesson code
- [ ] Every lesson ends with a "What's next" sentence connecting to the next lesson's opening concept
- [ ] Every lesson is self-contained: it compiles and runs to a visible result
- [ ] Both X11 and Raylib versions are present for every platform-specific section in every lesson

### PLAN.md and structure

- [ ] PLAN.md exists and matches the final lesson structure (lesson count, titles, outcomes)
- [ ] PLAN.md includes the **full topic inventory**: every source file listed, with every function/struct/concept mapped to a lesson
- [ ] Every item in the topic inventory maps to exactly one "New concepts" cell in the skill inventory table — no item is omitted
- [ ] Lesson count was derived from the topic inventory (split/merge applied), not forced to a preset number
- [ ] PLAN.md includes the concept dependency map (what must be introduced before what)
- [ ] PLAN.md includes the per-lesson skill inventory table (new concepts + reused concepts per row)
- [ ] Skill inventory table was reviewed for gaps before any lesson was written
- [ ] PLAN.md lists which files are created or modified in each lesson
- [ ] Each lesson builds and runs independently
- [ ] All code compiles with `-Wall -Wextra` without warnings
- [ ] Both X11 and Raylib backends work
- [ ] Comments explain WHY, not just WHAT
- [ ] Exercises are included and tested
- [ ] Build script output is `./build/game` (not per-backend names)
- [ ] Build script is always-debug (`-O0 -g -DDEBUG -fsanitize=address,undefined`); no release mode
- [ ] `-r` flag means "run after build"; `mkdir -p build` is present before the compile command
- [ ] Default backend in `build-dev.sh` is raylib
- [ ] Input system uses double-buffering with proper swap
- [ ] `prepare_input_frame` preserves `ended_down`
- [ ] Auto-repeat uses `RepeatInterval` with all fields
- [ ] **Y-up convention used for all non-grid games; Y-down only for pure tile/grid games**
- [ ] **Game logic uses world units (meters); pixel conversion only in `game_render()`**
- [ ] **`GAME_PHASE` enum introduced in the lesson that adds `GameState`, for any game with 2+ mutually exclusive states; no scattered boolean flags**
- [ ] **`change_phase()` wires all audio transitions (music start/stop/duck)**
- [ ] Audio (if included) works on both platforms
- [ ] `src/utils/audio.h` created with `GameAudioState`, `SoundInstance`, `ToneGenerator`, `MusicSequencer`, `AudioOutputBuffer`
- [ ] `src/audio.c` created with `game_get_audio_samples()` sample loop and `game_audio_update()`
- [ ] `GameAudioState audio` embedded directly in `GameState` (NOT a global or separate allocation)
- [ ] `platform.h` contract has `platform_audio_init`, `platform_audio_update`, `platform_audio_shutdown` — NOT `platform_play_sound`
- [ ] `change_phase()` has a `switch (new_phase)` block at the end that wires all audio transitions
- [ ] `game_audio_update()` is called once per game frame from `game_update()` (not from the platform layer)
- [ ] Raylib backend: `SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME * 2)` called **before** `LoadAudioStream`
- [ ] Raylib backend: 2 silent buffer pre-fills after `PlayAudioStream` (prevents initial underrun click)
- [ ] Raylib backend: per-frame update uses `while (IsAudioStreamProcessed(...))` — not `if`
- [ ] Raylib backend: **no `PlaySound()` / `LoadSound()` / `LoadSoundFromWave()` calls anywhere** — all audio goes through `game_get_audio_samples()`
- [ ] Audio sounds use `fade_in_samples` to prevent clicks/pops
- [ ] **Sound set covers: player actions, hits/feedback, UI, state transitions, and music (at minimum 6–8 sounds total)**
- [ ] **Sustained sounds use `game_sustain_sound_update()` pattern, not repeated re-trigger**
- [ ] **Music volume transitions use gradual ramp, not abrupt set**
- [ ] Stereo panning used where game position has spatial meaning
- [ ] Naming follows conventions table (PascalCase structs, UPPER_SNAKE enums, snake_case functions)
- [ ] Letterbox centering implemented on both backends from lesson 1
- [ ] Every lesson section has code examples for both X11 and Raylib (or explicit “identical on both backends” note)
- [ ] All looping/sustained sounds use `WAVE_TRIANGLE` + `volume ≥ 0.50`
- [ ] `MAX_SIMULTANEOUS_SOUNDS ≥ 16`; sized from busiest-moment inventory + 4 headroom
- [ ] All directional vectors derived from rotation matrix, not guessed; sanity-tested at `a=0`
- [ ] `compact_pool` (or equivalent) used for all pools that remove elements during update
- [ ] `memset(state, 0)` in `game_init` saves/restores persistent fields (audio config, hi-score)
- [ ] **Fixed timestep accumulator used for any game with physics or gravity** (platformers, shooters); variable `dt` only for animations
- [ ] **Camera system added before any scrolling world content; `world_to_screen()` replaces raw `PIXELS_PER_METER` in `game_render()`**
- [ ] **Collision detection operates in world units (meters), not pixels; only the final draw converts to screen**
- [ ] **Particle effects use a fixed pool with `lifetime`/`max_lifetime` alpha fade; no heap allocation**
- [ ] **`#ifdef DEBUG` hitbox and velocity overlay available; toggled at compile time, not at runtime**
- [ ] OpenGL pixel format validated with a pure-red test pixel on both backends
- [ ] `platform_shutdown()` in platform contract; called on normal exit and window close
- [ ] **File structure expanded appropriately for game scope; PLAN.md updated if structure diverged**

---

---

## Testing checklist

### Input testing

- [ ] Tap key once → action triggers once
- [ ] Hold key → action triggers after delay, then repeats
- [ ] Release key → action stops immediately
- [ ] Press two keys → both actions work independently
- [ ] Quick tap (< 1 frame) → action still triggers
- [ ] Works identically on X11 and Raylib

### Audio testing

- [ ] Sound effects play when triggered
- [ ] Multiple sounds play simultaneously without cutoff (up to `MAX_SIMULTANEOUS_SOUNDS`)
- [ ] No click/pop at sound **start** — `fade_in_samples` ramps volume up
- [ ] No click/pop at sound **end** — `samples_remaining` ramps volume down
- [ ] No click/pop at **buffer boundary** — `phase` is never reset between fills
- [ ] Stereo panning: sounds from left side of play field audibly left
- [ ] Music loops correctly through all patterns
- [ ] Master volume, sfx volume, music volume controls all work
- [ ] No audio when platform audio fails to initialize (`is_initialized = 0` path is safe)

### Rendering testing

- [ ] Colors appear correct (not swapped channels)
- [ ] No flickering or tearing
- [ ] UI elements are readable
- [ ] Overlays blend correctly
- [ ] Window resize doesn't break rendering

### Game logic testing

- [ ] Game initializes to correct state
- [ ] Restart resets everything properly
- [ ] Score/level increment correctly
- [ ] Game over triggers at right time
- [ ] Animations play at correct speed
- [ ] Delta time works (game speed independent of frame rate)

---

## Style guide

### Code formatting

```c
/* Function declarations: return type on same line */
void game_update(GameState *state, GameInput *input, float delta_time);

/* Braces: opening brace on same line */
if (condition) {
    /* code */
} else {
    /* code */
}

/* Switch: cases indented, break aligned with case */
switch (value) {
case CASE_A:
    do_something();
    break;
case CASE_B:
    do_other();
    break;
default:
    break;
}

/* Pointer declarations: star with type */
int *pointer;
GameState *state;

/* Constants: UPPER_SNAKE_CASE */
#define MAX_PLAYERS 4
#define CELL_SIZE 30

/* Types: PascalCase */
typedef struct { ... } GameState;
typedef enum { ... } MOVE_DIR;

/* Functions: snake_case */
void game_update(...);
void draw_rect(...);

/* Local variables: snake_case */
int current_index = 0;
float delta_time = 0.016f;
```

### Naming conventions

| Kind               | Convention      | Example                                     |
| ------------------ | --------------- | ------------------------------------------- |
| Structs            | `PascalCase`    | `GameState`, `RepeatInterval`, `Backbuffer` |
| Enum type names    | `UPPER_SNAKE`   | `SOUND_ID`, `TETROMINO_R_DIR`, `GAME_PHASE` |
| Enum values        | `UPPER_SNAKE`   | `SOUND_MOVE`, `GAME_PHASE_PLAYING`          |
| Functions          | `snake_case`    | `game_update`, `draw_rect`, `game_init`     |
| Local variables    | `snake_case`    | `delta_time`, `current_index`               |
| Macros / constants | `UPPER_SNAKE`   | `CELL_SIZE`, `MAX_BULLETS`, `BUTTON_COUNT`  |
| Global state       | `g_` prefix     | `g_x11`, `g_raylib`                         |
| Boolean fields     | `is_` prefix    | `is_running`, `is_game_over`, `is_active`   |
| Count fields       | `_count` suffix | `sample_count`, `pieces_count`              |
| One-shot flags     | plain `int`     | `quit`, `restart`                           |

**Consistency matters most.** Generated course code should look identical to `games/tetris/` — students copy patterns, not just concepts.

### Comment style

```c
/* Single-line comments use this style */

/*
 * Multi-line comments use this style.
 * Each line starts with a star.
 */

/* ═══════════════════════════════════════════════════════════════════════════
 * Section headers use box drawing characters
 * ═══════════════════════════════════════════════════════════════════════════
 */

void function(void) {
    /* Explain WHY, not WHAT */
    x += 1;  /* WRONG: increment x */
    x += 1;  /* RIGHT: move to next cell for collision check */

    /* Group related operations with blank lines and comments */

    /* Calculate new position */
    float new_x = pos_x + vel_x * dt;
    float new_y = pos_y + vel_y * dt;

    /* Check bounds */
    if (new_x < 0) new_x = 0;
    if (new_y < 0) new_y = 0;

    /* Apply position */
    pos_x = new_x;
    pos_y = new_y;
}
```

### File organization

```c
/* 1. Include guards (headers only) */
#ifndef GAME_H
#define GAME_H

/* 2. System includes */
#include <stdint.h>
#include <stdbool.h>

/* 3. Local includes */
#include "utils/backbuffer.h"

/* 4. Constants and macros */
#define FIELD_WIDTH 12
#define FIELD_HEIGHT 18

/* 5. Type definitions (enums first, then structs) */
typedef enum { ... } CellType;
typedef struct { ... } GameState;

/* 6. Function declarations */
void game_init(GameState *state);
void game_update(GameState *state, GameInput *input, float dt);

/* 7. End include guard */
#endif /* GAME_H */
```

**`static const` arrays in headers — use `extern const` for multi-TU projects:**

Declaring a `static const` array (e.g. a color palette or lookup table) in a header that is included by multiple `.c` files silently compiles a separate copy into every translation unit. The linker sees no duplicate symbol (`static` gives internal linkage) so no error is raised, but memory is wasted and the illusion that it is "shared" data can surprise students.

```c
/* AVOID in shared headers (each TU gets its own copy): */
static const uint32_t CONSOLE_PALETTE[16] = { ... };

/* PREFER: declare in the header, define once in game.c: */
/* game.h */  extern const uint32_t CONSOLE_PALETTE[16];
/* game.c */  const uint32_t CONSOLE_PALETTE[16] = { ... };
```

For early lessons (1–3) where only one `.c` file exists, `static const` is fine and simpler. Introduce `extern const` in the lesson that first splits game logic into two or more `.c` files, paired with a brief note on translation units and linkage.

---

## Final notes

### Philosophy

1. **Simplicity over cleverness** — Write code a beginner can follow
2. **Explicit over implicit** — Name things clearly, avoid magic numbers
3. **Platform independence** — Game logic never calls platform APIs
4. **Data-oriented** — Flat arrays, linear access, no pointer chasing; see `## Engineering principles`
5. **Comments explain WHY** — Code shows what, comments show reasoning
6. **One frame = one function call chain** — No hidden callbacks, no "notify" patterns; all logic is visible in `game_update()` → `game_render()`
7. **No dynamic allocation in the loop** — `malloc` only at startup; everything else is stack or fixed global

### When to deviate

These patterns work for simple 2D games. Deviate when:

- Performance requires it (profile first!)
- Game mechanics demand different architecture
- Platform limitations force changes

Document deviations clearly and explain the reasoning.

### Resources

- **Handmade Hero (Casey Muratori)** — Episodes 1–40: platform layer, backbuffer, input. Episodes 41–60: game loop timing, Casey's audio latency model. The single best reference for this architecture.
- Raylib examples — Simple game patterns (good for quick prototypes)
- Lazy Foo SDL tutorials — Cross-platform basics
- Game Programming Patterns (Robert Nystrom) — Architecture patterns; read after you understand the data-oriented approach
