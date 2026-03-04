# Course Builder Improvements

Observations from producing the Tetris build-along course against `course-builder.md`.

---

## 1. Patterns That Worked Well

### JS→C concept mapping table

> **From course-builder.md:** "anchor every new C concept to the nearest JS equivalent, then explain what C does differently and why."

Every lesson header naturally acquired a `## What you'll learn` section structured as JS-first explanations. The mapping table in the plan also made lesson scoping obvious — lesson boundaries naturally fell where a new row of the table applied. The inline code block comments (`/* JS: ... | C: ... */`) that emerged in `game.h` and `audio.c` are a direct product of this directive.

### Backbuffer pipeline architecture

The ASCII diagram in the spec (`game writes ARGB pixels → platform uploads to GPU`) was directly usable as course prose. Having it in the spec meant both backends (Raylib and X11) were scaffolded correctly from lesson 1 and stayed consistent through lesson 13 without revision. The "game logic is 100% platform-independent" invariant held throughout.

### Double-buffered input with `prepare_input_frame`

The spec's explicit warning about swapping contents vs. pointers, and the explanation of why `ended_down` must be preserved from the previous frame, translated directly into a **Common Input Bugs** table in lesson 03. The `UPDATE_BUTTON` macro and its `do { } while(0)` rationale were also straight from the spec and needed no adjustment.

### DAS/ARR with `passed_initial_delay`

The `RepeatInterval` pattern in the spec already included the correct `passed_initial_delay` flag. The one addition the course had to make — a callout explaining *why* the reference implementation's `initial_delay = 0.0f` mutation is wrong — became a `> **Handmade Hero principle:**` note in lesson 06.

### `> **Why?**` and `> **Handmade Hero principle:**` callouts

These two callout styles were used in nearly every lesson. They create a readable two-layer document: the code tells you *what* to type; the callouts tell you *why it exists*. The distinction (Why = general engineering reason; Handmade Hero = Casey-specific philosophy) held cleanly throughout all 13 lessons.

### Complete-file-per-lesson requirement

Having every lesson ship a complete, compilable snapshot of each file eliminated student confusion about "where does this go?" Each lesson's `## Build and run` section was a single short command. No lesson required the student to mentally merge two partial listings.

---

## 2. What Was Missing or Needed Adjustment

- **No warning about the reference implementation's `initial_delay` mutation.** `course-builder.md` defines the correct `RepeatInterval` struct with `passed_initial_delay`, but doesn't call out that the reference `games/tetris/` code mutates `repeat->initial_delay = 0.0f` as a shortcut. A future course author reading only the reference code will copy the bug.

- **No guidance on `game.h` evolution across lessons.** The spec says "complete file per lesson," but `game.h` grows from 10 lines in lesson 04 to 300+ lines by lesson 13. There is no guidance on whether to repeat the full header every lesson or only show diffs. The course adopted full-file snapshots at major milestones and incremental struct diffs in between.

- **Audio section is marked "optional" but required two full lessons.** The spec's audio coverage (roughly 60 lines) describes the `AudioOutputBuffer` and a basic `SoundDef` table. The actual Tetris audio is a phase accumulator, a step sequencer, volume ramping, stereo panning, and a two-loop architecture (game-thread sequencer + audio-thread sample fill). An "optional" label undersells it significantly.

- **No guidance on the two-audio-loop architecture.** `game_audio_update` (runs on game logic time, advances the sequencer) and `game_get_audio_samples` (runs on audio time, fills PCM) are separate concerns that share state via `ToneGenerator`. The spec doesn't name this pattern or explain when each should be called. Lesson 12 had to invent an explanation from scratch.

- **Build command evolution is undocumented.** Lesson 01's build command is `clang -I. main_raylib.c -lraylib -lm -o tetris`. Lesson 13's build command is a multi-file invocation with `-I./src`, `audio.c`, `game.c`, and backend flags. There is no guidance on how to present this progression or when to introduce `build-dev.sh` as a replacement for the manual command.

- **No guidance on removing debug `printf` from reference code.** `audio.c` in the reference keeps `#include <stdio.h>` with a comment "kept for potential debug printf." The course had to decide independently to retain it but annotate it clearly.

---

## 3. Concrete Suggested Edits

**Section:** `## Audio system (optional)`

**Current text:**
```
## Audio system (optional)

**Reference:** `games/tetris/src/utils/audio.h`, `games/tetris/src/audio.c`
```

**Suggested replacement:**
```
## Audio system

**Reference:** `games/tetris/src/utils/audio.h`, `games/tetris/src/audio.c`

**Note:** For games with non-trivial audio, this section typically expands into
two lessons: (1) sound effects (procedural synthesis, phase accumulator, stereo
pan); (2) background music (sequencer, MIDI-to-Hz, volume ramping). Treat the
patterns below as the minimum; expect to add 150–200 lines of explanation per lesson.
```

**Reason:** Marking audio "optional" causes it to be under-planned. Tetris needed two lessons; most games with audio will too.

---

**Section:** `### Auto-Repeat (DAS/ARR)`

**Current text (in `RepeatInterval` block):**
```
DAS = Delayed Auto Shift (initial delay before repeating starts)
ARR = Auto Repeat Rate (speed of repeating after DAS)
```

**Suggested addition after the `RepeatInterval` struct definition:**
```
⚠ **Reference implementation warning:** The `games/tetris/` reference mutates
`repeat->initial_delay = 0.0f` to "enter ARR mode." This destroys the
configuration value. Use the `passed_initial_delay` bool flag instead — it
records the phase transition without touching the config, so DAS behaviour is
fully restored on key release.
```

**Reason:** Prevents a course author from copying the reference bug when the spec already shows the correct approach.

---

**Section:** `### Step 2 — Build the course files`

**Current text:**
```
**Do not start writing lessons until PLAN.md exists.**
```

**Suggested addition:**
```
**Game header evolution:** `game.h` will grow substantially across lessons.
In early lessons, show the complete header. In middle lessons, show only the
changed struct with a `/* BEFORE / AFTER */` comment block. In the final lesson,
show the complete file again as a definitive reference. Never ask the student to
mentally merge two partial listings.
```

**Reason:** The spec mandates complete files but doesn't address the specific problem of a rapidly-evolving shared header.

---

**Section:** `## Build script`

**Current text:**
```
**Usage pattern:**
./build-dev.sh                      # Build with default backend (raylib)
```

**Suggested addition:**
```
**Build command progression:** Lesson 1 shows a minimal manual `clang` command
for one source file. Introduce `build-dev.sh` no later than the first lesson
that adds a second `.c` file (typically lesson 4 or 5). Document the flag
breakdown table each time the compile command grows. Students who see the
command expand without explanation often assume something broke.
```

**Reason:** The command-line's growth is a teaching moment that the spec misses.

---

**Section:** `## Source file rules` (new sub-item)

**Current text:** *(no guidance on debug output in course code)*

**Suggested addition:**
```
**Debug printf policy:** Remove or `#ifdef DEBUG`-guard any `printf`/`fprintf`
that was present in the reference for development purposes. If you retain an
`#include <stdio.h>` for potential student use, annotate it explicitly:
```c
#include <stdio.h>  /* Not used at runtime — available for student debugging */
```
Unexplained debug includes confuse students about what is "needed" vs. "leftover."
```

**Reason:** `audio.c` in the reference retains `<stdio.h>` silently; the course had to decide independently what to do with it.

---

## 4. C/Game Concepts Not Covered in `course-builder.md`

| Concept | What it is | Suggested section |
|---|---|---|
| **Phase accumulator** | A float counter `[0, 1)` that advances by `freq / sample_rate` per sample, wrapping at 1. The simplest oscillator implementation. | `## Audio system` — add "Phase accumulator pattern" sub-section |
| **Square wave synthesis** | `(phase < 0.5f) ? 1.0f : -1.0f` — produces a square wave from any phase accumulator. PCM output is `(int16_t)(wave * volume * INT16_MAX)`. | Same audio section |
| **Volume ramping** | Advancing `current_volume` toward `target_volume` by a fixed amount per sample. Prevents audible clicks when volume changes discontinuously between buffer fills. | `## Audio system` — "Volume ramping" sub-section |
| **MIDI→Hz formula** | `440.0f * powf(2.0f, (note - 69) / 12.0f)`. Equal temperament: 12 semitones per octave, A4=69=440Hz. | `## Audio system` — "Music sequencer" sub-section |
| **Two-loop audio architecture** | `game_audio_update` (game thread, delta_time) advances the sequencer; `game_get_audio_samples` (audio thread, per-sample) fills PCM. They share state via `ToneGenerator`. | `## Audio system` — "Two-loop architecture" sub-section |
| **Stereo panning — linear law** | Multiply left channel by `1 - pan`, right by `pan` (pan in `[0,1]`). Simple and sufficient for gameplay spatial audio. | `## Audio system` |
| **Bit-packed bitmap font** | A 5×7 (or 8×8) glyph stored as one bit per pixel per row. `bitmap[row] & (0x10 >> col)` tests the `col`-th bit. No texture atlas required. | `## Drawing primitives` — expand bitmap font section |
| **`snprintf` for int→string** | `snprintf(buf, sizeof(buf), "%d", score)` — safe integer-to-string, null-terminates, never overflows. JS equivalent: `String(score)`. | `## Drawing primitives` — add note to bitmap font section |
| **Bottom-to-top row processing** | When collapsing filled rows in a 2D array, always scan from the bottom row upward. Scanning top-down shifts rows into already-processed positions, producing incorrect results. | Add a "2D array operations" note in coordinate systems section |
| **`(1 << n)` exponential scoring** | `(1 << lines_cleared) * BASE_SCORE` gives 2× reward per additional simultaneous line. JS equivalent: `Math.pow(2, n)`. | No obvious existing section — add to "Game logic patterns" or a new "Common game formulas" section |
| **`uint8_t` MIDI note array as sequencer** | Compact array of note numbers (0 = rest, 1-127 = MIDI note) driving a step sequencer. Each pattern is 16 bytes. | `## Audio system` — "Music sequencer" sub-section |
| **Ghost piece algorithm** | From current piece position, repeatedly apply the fall-by-one move until it would collide, then render at that final position with reduced alpha. | Add a "Polish patterns" section or expand existing lesson template |
