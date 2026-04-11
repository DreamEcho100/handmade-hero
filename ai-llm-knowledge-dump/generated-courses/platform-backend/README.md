# Platform Backend Course

Build a reusable two-backend game runtime from scratch, then use that runtime to learn how memory, rendering, audio, scene flow, and proof-driven debugging fit together in one live codebase.

This directory now has one active learner surface:

- `course/` — the live course, source tree, build script, and lesson sequence

Anything outside that path is archive or support material and is not part of the active reading order.

---

## What This Course Builds

By the end of the course, you have a working platform/backend runtime with:

- a shared game facade
- two platform shells: Raylib and X11/GLX
- a CPU backbuffer rendered into a window every frame
- explicit input snapshots and frame timing
- a procedural and loaded-audio pipeline
- explicit memory lifetimes (`perm`, `level`, `scratch`)
- a four-scene runtime that turns allocator and runtime concepts into visible proof surfaces

This is not a toy theory course.

The lesson sequence stays tied to the live source in `course/src/`, and the later modules use scene labs so you can verify allocator, rebuild, pool, slab, and diagnostic behavior in the running program.

---

## Who This Is For

This course is written for a learner who may be comfortable with basic programming but is new to platform/backend development.

You should already know:

- variables, loops, functions, structs, and pointers in C
- basic compilation vocabulary
- the difference between stack and heap at a high level

You do not need prior experience with:

- X11
- OpenGL/GLX boot code
- Raylib internals
- low-level audio backends
- custom allocators
- game-engine architecture

The course is designed to build those concepts in order.

---

## How To Read The Course

The active lessons live under:

- `course/lessons/`

The modules are ordered and cumulative. Read them in sequence.

### Module Sequence

1. `01-course-foundations` — lessons `01-03`
   - learn the live runtime spine, build script, and platform contract
2. `02-windowing-and-boot` — lessons `04-06`
   - open real windows and understand backend boot flow
3. `03-pixel-pipeline-and-drawing` — lessons `07-11`
   - build the backbuffer, drawing rules, clipping, alpha, and text
4. `04-input-timing-and-presentation` — lessons `12-17`
   - understand input snapshots, timing, pacing, scaling, and presentation
5. `05-audio-foundations-and-runtime` — lessons `18-23`
   - build the audio contract, synthesis, loaded audio, and backend flow
6. `06-coordinate-systems-and-camera` — lessons `24-29`
   - move from raw pixels to world units, transforms, and camera reasoning
7. `07-memory-lifetimes-and-allocators` — lessons `30-35`
   - learn explicit ownership and custom allocator behavior
8. `08-game-facade-and-scene-machine` — lessons `36-40`
   - understand the live runtime boundary and scene coordination
9. `09-runtime-instrumentation-and-proof-panels` — lessons `41-45`
   - learn how prompts, traces, and HUD proof surfaces are produced
10. `10-arena-lifetimes-lab` — lessons `46-51`
    - verify arena lifetime rules in a live scene lab
11. `11-level-reload-lab` — lessons `52-57`
    - verify rebuild, reset, and scan behavior in runtime
12. `12-pool-reuse-lab` — lessons `58-63`
    - verify free-list reuse and churn in runtime
13. `13-slab-audio-lab` — lessons `64-69`
    - verify slab growth, transport state, and audio pressure together
14. `14-runtime-integration-capstone` — lessons `70-75`
    - integrate bench, failure handling, render layering, scene isolation, and final extension guidance

### Reading Rule

Each lesson should do four jobs:

1. tell you what result you are aiming for
2. explain why the design exists
3. walk you through the implementation or runtime behavior step by step
4. give you a way to verify the claim in code or in the running build

If you want the fastest useful path, read one module at a time and run the build between modules instead of trying to finish the whole course as pure reading.

---

## Build And Run

From this directory:

```bash
cd course
./build-dev.sh --backend=raylib -r
```

For the X11 backend:

```bash
cd course
./build-dev.sh --backend=x11 -r
```

Focused runtime checks:

```bash
cd course

# Run a timed profiler session
./build-dev.sh --backend=raylib --bench=5 -r

# Lock a specific scene for study
./build-dev.sh --backend=raylib --force-scene=0 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=1 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=2 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=3 --lock-scene -r
```

Recommended Linux packages:

```bash
# Raylib path
sudo apt install libraylib-dev

# X11/GLX + ALSA path
sudo apt install libx11-dev libgl-dev libxkbcommon-dev libasound2-dev
```

---

## Source Tree Overview

The live code surface is:

```text
platform-backend/
  course/
    build-dev.sh
    src/
      platform.h
      game/
      platforms/
      utils/
    lessons/
```

The architectural spine is:

```text
build-dev.sh
    |
    v
platform backend main (Raylib or X11)
    |
    v
src/platform.h
    |
    v
src/game/main.h
    |
    v
src/game/main.c + scene/runtime support
```

The course keeps returning to that path because it is the active runtime front door.

---

## Runtime Conventions You Will See Repeatedly

### Two backend shells, one runtime

Raylib and X11 are two platform edges around the same game/runtime contract.

### Three memory lifetimes

- `perm` — long-lived/session state
- `level` — rebuildable scene state
- `scratch` — per-frame temporary work

### Proof-driven labs

The later modules stop being “read the code and trust it.”

They turn runtime behavior into visible evidence:

- prompts
- trace steps
- HUD metrics
- scene-local proof panels
- warning lines
- forced-scene builds

That is how allocator and runtime architecture become teachable instead of abstract.

---

## Recommended Workflow

1. Read one lesson.
2. Open the target files immediately.
3. Run the relevant build.
4. Do the lesson exercises.
5. Verify the runtime proof checkpoint before moving on.

For the lab modules, keep one terminal open in `course/` and rerun focused scene builds often.

---

## Start Here

Begin with:

- [course/lessons/01-course-foundations/README.md](course/lessons/01-course-foundations/README.md)

That opening module explains the active runtime map, the build script, and the shared platform contract before the course drops into backend boot code.
├── utils/
│ ├── math.h ← CLAMP, MIN, MAX, ABS
│ ├── backbuffer.h ← Backbuffer struct + GAME_RGB/RGBA + color constants
│ ├── draw-shapes.h/.c ← draw_rect, draw_rect_blend
│ ├── draw-text.h/.c ← FONT_8X8[128][8], draw_char, draw_text
│ └── audio.h ← AudioOutputBuffer, SoundDef, SoundInstance,
│ │ GameAudioState, ToneGenerator, MusicSequencer,
│ │ PlatformAudioConfig, audio_write_sample
├── game/
│ ├── base.h ← GameInput union template, UPDATE_BUTTON,
│ │ │ prepare_input_frame, platform_swap_input_buffers,
│ │ │ DEBUG_TRAP/ASSERT
│ ├── main.h/.c ← game facade + 4-scene state machine
│ ├── demo.c ← historical visual demo snapshot
│ ├── demo_explicit.c ← older explicit render demo kept as reference
│ └── audio_demo.c ← PCM mixer, SoundDef table, MusicSequencer,
│ │ scene audio profiles
├── platforms/
│ ├── x11/
│ │ ├── base.h ← X11State struct + extern g_x11
│ │ ├── base.c ← X11State g_x11 = {0}
│ │ ├── main.c ← X11 + GLX + VSync + letterbox + input + FrameTiming
│ │ └── audio.c ← ALSA init/write/latency/shutdown
│ └── raylib/
│ └── main.c ← Raylib + audio stream + letterbox + input
└── platform.h ← shared contract: 4 functions + PlatformGameProps +
STRINGIFY/TOSTRING/TITLE

````

---

## Legacy Flat Lesson Reference

This table preserves the original `01-21b` reference snapshot. The complete flat bridge path continues beyond it through the module directories listed above, ending at lesson `75`.

| #    | Title                                  | What you build                                                              | What you see/hear                                                               |
| ---- | -------------------------------------- | --------------------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| 01   | Toolchain + Colored Window (Raylib)    | `build-dev.sh`, Raylib window                                               | Solid-color window at 60 fps                                                    |
| 02   | X11/GLX Window                         | X11 backend matching Lesson 1                                               | Same window via Linux X server                                                  |
| 03   | Backbuffer + Pixel Format              | Pixel buffer + GPU upload + `demo_render()`                                 | First CPU-rendered frame on screen                                              |
| 04   | Drawing Primitives + Game Units        | `draw_rect`, `draw_rect_blend`, unit-space drawing                          | Colored rectangles and alpha blending                                           |
| 05   | Platform Contract                      | `platform.h`, shared platform/game boundary                                 | Same visual, cleaner architecture                                               |
| 06   | Bitmap Font Rendering                  | `FONT_8X8`, `draw_char`, `draw_text`                                        | Text rendered into the backbuffer                                               |
| 07   | Double-Buffered Input                  | `GameInput`, `UPDATE_BUTTON`, input frame prep                              | Correct held-vs-pressed behavior                                                |
| 08   | Frame Timing + Letterbox               | Fixed timestep, FPS stats, aspect-preserving letterbox                      | Stable frame pacing and centered canvas                                         |
| 09   | Window Scaling Modes                   | Fixed / dynamic / aspect-preserving resize behavior                         | Window can resize without losing control                                        |
| 10   | Platform Template Extraction           | Historical extraction milestone and platform/game boundary mapping          | Understand how the bare template boundary relates to the current branch         |
| 11   | Arena Allocator                        | `Arena`, `ArenaBlock`, `TempMemory`, guard-page-backed storage              | No visual change; memory now uses arena lifetime management                     |
| 11.1 | Arena Lifetimes + Ownership            | Lifetime vocabulary, ownership rules, stats/debug tooling                   | No visual change; much clearer memory model                                     |
| 11.2 | Level / Scene Arena                    | Level-lifetime arena integrated into platform state                         | No visual change; scene data now has a dedicated lifetime                       |
| 11.3 | Pool / Free-List Allocator             | Fixed-size reusable object slots with individual free                       | No visual change; supports high-churn same-size objects                         |
| 11.4 | Slab Allocator                         | Growable multi-page allocator for same-size objects                         | No visual change; pool-like objects can now grow in chunks                      |
| 11.5 | Arena Debugging + Inspection           | Runtime inspection workflow for temp scopes, stats, and traces              | Arena lab becomes a real debugging surface                                      |
| 12   | Rendering at Scale                     | Camera pan/zoom, multiple render contexts, culling helpers                  | Camera movement and scalable world rendering                                    |
| 12.1 | State Machines + Scene Flow            | Scene enum, transition rules, runtime/compile-time overrides                | You can move between scenes, track exercise progress, and lock a debug override |
| 12.2 | Multi-Level Game Facade                | `game_app_*` facade, scene-local state, scene-specific audio                | Both backends run the same 4-scene runtime with guided lab checklists           |
| 13   | Audio Foundations                      | Sample formats, buffer helpers, ownership model                             | First structured audio pipeline                                                 |
| 14   | Waveform Synthesis                     | Oscillators, phase accumulation, procedural tones                           | Synthesized waveforms                                                           |
| 15   | Sound Effects Voice Pool               | `ToneGenerator` pool, voice stealing, envelopes                             | Layered sound effects                                                           |
| 15a  | Voice Stealing Walkthrough             | Step-by-step oldest-voice replacement reasoning                             | Easier to inspect why new sounds still cut through                              |
| 16   | Raylib Audio Ring Buffer               | SPSC ring buffer and robust Raylib audio feed                               | Smooth Raylib audio stream                                                      |
| 16a  | Ring Buffer State Walkthrough          | Read/write cursor mental model and underrun interpretation                  | Audio buffering becomes easier to reason about                                  |
| 16.1 | Underrun Simulation + Diagnosis        | Exact failure-state walkthrough for missing buffered audio                  | Audio glitches become explainable in buffer terms                               |
| 17   | ALSA Audio Cursors                     | ALSA latency model, cursors, underrun recovery                              | Smooth X11/ALSA audio                                                           |
| 17a  | ALSA Underrun Recovery Walkthrough     | Step-by-step retry and recovery flow after write failures                   | ALSA recovery becomes easier to debug                                           |
| 18   | WAV File Loading                       | RIFF/WAV parsing and loaded sound ownership                                 | Loadable audio assets                                                           |
| 19   | Audio Mixer                            | Mixer architecture, fades, pitch control, and current-course mapping        | Multiple sounds mixed together                                                  |
| 19a  | Mixer Debugging Walkthrough            | Voice-pool, fade, and chain-state debugging                                 | Mixer bugs become easier to localize                                            |
| 19b  | Audio Warning Severity Walkthrough     | Warning colors, cause-vs-severity reading, and pressure recovery            | Yellow and red warnings become concrete runtime states with visible recovery    |
| 20   | Audio Hooks + Focus                    | Focus pause/resume, lifecycle reasoning, and current-backend mapping        | Correct behavior on focus changes                                               |
| 20a  | Focus + Pause Lifecycle Walkthrough    | Focus loss, reload-silence reference reasoning, and resume-state debugging  | Audio lifecycle bugs become easier to classify                                  |
| 21   | Music Sequencer                        | Pattern-based sequencer layering synth + samples, with current type mapping | Background music playback                                                       |
| 21a  | Sample-Accurate Sequencing Walkthrough | Exact note-boundary timing inside the sample loop                           | Sequencer timing becomes explainable, not just observable                       |
| 21b  | Sequencer + Mixer Layering Walkthrough | Shared headroom, layering order, and interaction debugging                  | Sequencer and SFX interactions become easier to diagnose                        |

### Walkthrough Track

Use these follow-up lessons as one debugging-oriented reading path rather than isolated extras:

- `11.5` Arena Debugging + Inspection
- `15a` Voice Stealing Walkthrough
- `16a` Ring Buffer State Walkthrough
- `16.1` Underrun Simulation + Diagnosis
- `17a` ALSA Underrun Recovery Walkthrough
- `19a` Mixer Debugging Walkthrough
- `19b` Audio Warning Severity Walkthrough
- `20a` Focus + Pause Lifecycle Walkthrough
- `21a` Sample-Accurate Sequencing Walkthrough
- `21b` Sequencer + Mixer Layering Walkthrough

### Scene-Lab and Capstone Continuation

After the `01-21b` foundation snapshot, the flat bridge continues through the live scene runtime and final integration block:

- `08-game-facade-and-scene-machine` — scene ids, rebuild flow, overrides, and facade boundaries (`36-40`)
- `09-runtime-instrumentation-and-proof-panels` — prompts, trace panels, warning context, and full HUD composition (`41-45`)
- `10-arena-lifetimes-lab` — arena proof lab (`46-51`)
- `11-level-reload-lab` — level rebuild proof lab (`52-57`)
- `12-pool-reuse-lab` — free-list reuse proof lab (`58-63`)
- `13-slab-audio-lab` — slab growth and transport pressure proof lab (`64-69`)
- `14-runtime-integration-capstone` — bench mode, override/failure control, render composition, scene isolation, and final verification (`70-75`)

---

## Build Instructions

### Raylib backend

```bash
sudo apt install libraylib-dev   # Ubuntu 22.04+
cd course/
chmod +x build-dev.sh
./build-dev.sh --backend=raylib -r

# Boot directly into scene 3 and lock it there for focused testing
./build-dev.sh --backend=raylib --force-scene=3 --lock-scene -r
````

### X11/GLX backend

```bash
sudo apt install libx11-dev libgl-dev libxkbcommon-dev libasound2-dev
cd course/
./build-dev.sh --backend=x11 -r
```

### Debug build (AddressSanitizer + UBSanitizer)

```bash
./build-dev.sh --backend=raylib -d -r
```

### Focused runtime checks

```bash
# Timed profiler run
./build-dev.sh --backend=raylib --bench=5 -r

# Locked scene checks
./build-dev.sh --backend=raylib --force-scene=0 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=1 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=2 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=3 --lock-scene -r
```

---

## Architecture Overview

```
Platform layer (I/O, timing, OS events, audio device)
        ↓ raw input (GameInput), delta_time
   game_update()   →   GameState (mutated in place)
        ↓
   game_render()   →   Backbuffer (written pixel by pixel)
        ↓
Platform layer (upload backbuffer to GPU, push PCM samples)
```

The game layer (`game/`) never calls X11 or Raylib directly. The platform layer (`platforms/*/`) never contains game logic. `platform.h` defines the exact boundary.

---

## How Game Courses Use This Template

After completing this course, when starting a new game course:

1. **Copy** `course/src/` to the new game's directory
2. **Delete or replace** the course-only demo/runtime files:
   - `game/main.c` (course facade + scene labs)
   - `game/audio_demo.c` (course audio demo and scene profiles)
   - optionally `game/demo_explicit.c` if you do not need the older reference demo
3. **Adapt** the game-specific parts:
   - `platform.h` → update `TITLE` macro
   - `platforms/x11/main.c` and `platforms/raylib/main.c` → set `GAME_W`, `GAME_H`; update key bindings
   - `game/base.h` → rename `GameInput` button fields; set `BUTTON_COUNT`
4. **Add** game-specific files: `game/main.h`, `game/main.c`, `game/audio.c`, optionally `game/levels.c`

Game course lessons start immediately with game-specific content. When a lesson requires a platform change (e.g., a new key binding), it shows only the changed lines and references this course for the full explanation.

---

## Reference Implementations

- `games/snake/` — Stage B layout matching this course's output
- `games/tetris/` — Stage A layout (single-file backends; useful for simpler games)
- `ai-llm-knowledge-dump/prompt/course-builder.md` — full course-building guidelines
