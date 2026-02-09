# DE100 - Multi-Backend Engine Architecture (Learning-Oriented)

> **Purpose of this document**
> This README exists to remove ambiguity. It explains **what this project is**, **why it is structured the way it is**, and **how it intentionally differs from conventional game engines**.
> If you are reading this for the first time (including future‑me or an AI assistant), this document is the ground truth.

---

## What this project is (and is not)

### What it _is_

- A **learning‑first, systems‑oriented game engine project** inspired by _Handmade Hero_.
- A deliberate exploration of:
  - C as a low‑level language
  - OS/platform APIs (For now X11 and Raylib for comparison and cross-platformness).
  - Cross‑platform abstractions and their real costs
  - Engine vs game responsibility boundaries
  - A project that **intentionally supports multiple backend styles at once**:
    - _Native OS backends_ (e.g. X11)
    - _Cross‑platform library backends_ (e.g. Raylib)

### What it is _not_

- Not a production‑ready, one‑size‑fits‑all engine _(for now, maybe on the future for production can be focused one one backend like Raylib)_.
- Not an SDL/GLFW/Raylib replacement
- Not a "clean abstraction at all costs" architecture
- Not optimized for maximum reuse _yet_

> **Key philosophy**: understanding > abstraction > reuse

---

## The core design philosophy

This project follows a few non‑negotiable principles:

1. **No fake abstraction**
   Abstractions are introduced only when the cost and benefit are fully understood.

2. **Honest coupling is better than dishonest decoupling**
   If two things are coupled in reality, the code should show it.

3. **Learning visibility beats elegance**
   Platform quirks, backend differences, and tradeoffs should be visible, not hidden.

4. **The engine must not own game policy**
   Especially input semantics, gameplay meaning, or binding decisions.

5. **Portability is explored, not assumed**
   The project deliberately compares low‑level OS APIs with higher‑level libraries.

---

## Why this architecture is different from most engines

Most engines choose **one axis**:

- Either:
  - One cross‑platform backend (SDL / GLFW / Raylib)

- Or:
  - Multiple OS‑specific backends

### This project does **both** — intentionally

| Goal                   | Result                             |
| ---------------------- | ---------------------------------- |
| Learn OS reality       | Native backends (X11, Win32, etc.) |
| Learn abstraction cost | Cross‑platform backend (Raylib)    |
| Compare tradeoffs      | Both exist side‑by‑side            |

In here currently, for example, Raylib is treated **as a backend**, not as _"the engine"_

---

## High‑level directory structure _(rough overview)_

**Engine folder structure:**

```
├── backend.h
├── build-common.sh # build utils and helpers for the games build process
├── _common/ # Cross‑platform utilities
│   └── ... # (memory, files, time, threading, DLLs, etc.)
├── engine.c
├── engine.h
├── game/
│   └── ...
├── hooks/
│   └── ...
├── _internal/
│   └── ...
├── main.c
├── platforms/
│   ├── _common/ # Shared platform logic (NOT game logic)
│   │   └── ...
│   └── [platform]/ # platform _(raylib/x11)_ logic here like handling audio, game-loop, inputs...
│       ├── hooks/ # a platform interfaces that are exposed to the game, either as utilities to be used like in `de100_get_frame_time` or headers to implement on the game in case of specific inputs like keyboard and joystick
│       │   ├─── inputs/ # _(joystick.h, keyboard.h)_
│       │   │   └── ...
│       │   └── utils.c Like `de100_get_frame_time`, `de100_get_time`, `de100_get_fps`, ...
│       └── ... # other platform specific code like backend.c, audio.c, inputs/mouse.c
└── todos.md
```

**Game example folder structure:**

```
├── build/ # build output
│   └── ...
├── build-dev.sh # build the game in dev mode
└── src/
    ├── adapters/ # game/platform adapters code
    │   ├── [platform]/
    │       └── inputs/
    │           ├── joystick.c
    │           └── keyboard.c
    ├── init.c # Defines the `GAME_INIT`, called when the game is loaded after startup, before the game loop
    ├── inputs.h # Defines the game _actions_, which should be imported on the `src/adapters/[platform]/inputs/**.c` at the start
    ├── main.c # Defines the `GAME_UPDATE_AND_RENDER` and `GAME_GET_AUDIO_SAMPLES`, called every frame
    ├── main.h
    ├── startup.c # Defines the `GAME_STARTUP`, called when the game is loaded before the game loop
    └── ... other game specific code
```

---

## Layer responsibilities (step‑by‑step)

### 5.1 `engine/_common/`

- Pure C utilities
- Platform APIs are used with macro guards, currently support windows, linux, macos, with linux is what's currently tested while the others need to be _(mostly implemented by LLMs with others since I have less experience with them, that's why I need to test them later)_
- No game knowledge
- Reusable everywhere

Examples:

- Memory arenas
- File I/O wrappers
- Time utilities
- Error handling

---

### 5.2 `engine/platform/`

This layer deals with **platform reality**.

Responsibilities:

- Window creation
- Input handling _(polling/events)_
- Hot reload (mostly dev mode, it's planned to be disabled on prod builds and have only static linking)\_
- Audio output
- Timing
- Backend‑specific APIs

Rules:

- ❌ No game logic
- ❌ No gameplay meaning
- ❌ No action mapping

The platform produces **raw data**, nothing more.

---

### 5.3 `engine/engine.c`

The engine is an **orchestrator**, not a policy owner.

Responsibilities:

- Startup / shutdown
- dynamic loading _(for game code and adapters)_
- Calling game entry points
- Passing platform data through

The engine:

- Does not interpret input
- Does not define bindings
- Does not know about actions

---

### 5.4 `[game]/` (game code)

This is where **meaning exists**.

Responsibilities:

- Game logic
- Rendering
- Audio generation
- Input semantics

The game is allowed to:

- Define actions
- Define combos
- Define bindings
- Be opinionated

---

## The game adapters concept (critical) _(planned, not fully implemented yet)_

### What adapters are

Adapters are **game‑owned, backend‑specific glue code**.

They:

- Live inside the game directory
- Are compiled/linked per backend
- Are allowed to include backend headers
- Mostly for translating what normalizing between different backends/games/cases would be a hurdle/cause-performance-issues, for example: backend input → game actions. at least until I find a better way to do it.

### What adapters are _not_

- Not part of the engine
- Not reusable across games
- Not pretending to be universal behaviors/bindings between backends/games

---

## Input handling philosophy (keyboard example)

### The problem

Keyboard input differs across:

- Platforms
- Libraries
- Layouts
- Games

Normalizing input at the engine level forces **policy decisions** that don't belong there.

### Example of the solution used here

```
Platform (X11 / Raylib)
    ↓ raw events
Engine (pass‑through)
    ↓
Game Adapter (backend‑specific)
    ↓ actions
Game Logic
```

#### Why this is intentional

- Each game has different actions
- Each game wants different bindings
- Combos and chords are game semantics
- Text input ≠ control input

Adapters keep all of this **local and explicit**.

---

#### Remapping, combos, and advanced input

Because adapters already map input → actions:

- Remapping is just data or code in the adapter
- Combos and sequences live next to bindings
- Multiple binding layers are trivial

No engine involvement required.

---

## Hot reload & dev vs prod _(planned, not fully implemented yet)_

- Game code (including adapters) is dynamically loaded in dev
- Input logic can be hot‑reloaded
- Engine and platform stay stable

Prod builds can:

- Disable hot reload
- Link statically
- Strip debug features

---

## Why duplication is acceptable here

Some adapter code will be duplicated.

This is **intentional**:

- Bindings are not logic
- Bindings are policy
- Policy should be visible

Hiding duplication behind abstraction would reduce learning value.

---

## Future evolution (without commitment)

Possible future steps:

- Lift common patterns from adapters upward
- Introduce optional shared input helpers
- Add data‑driven bindings
- Integrate third‑party input libraries (game‑side only)

Nothing in the current design blocks this.

---

## Final note

This project prioritizes:

> **Clarity, honesty, and understanding**

over:

> **Premature elegance or forced reuse**

If something looks more manual than expected, it is probably by design.

## Code Style Guide

> Consistent, C-idiomatic, Casey-inspired.

---

### Namespace Strategy

The engine uses `De100` / `de100_` prefix to avoid collisions when integrated into games.

| Code Location    | Types             | Functions               | Enums                   |
| ---------------- | ----------------- | ----------------------- | ----------------------- |
| `engine/*`       | `De100PascalCase` | `de100_module_action()` | `DE100_SCREAMING_SNAKE` |
| `yourgame/src/*` | `PascalCase`      | `snake_case()`          | `SCREAMING_SNAKE`       |

---

### Quick Reference

| Element          | Style             | Example                                     |
| ---------------- | ----------------- | ------------------------------------------- |
| Engine types     | `De100PascalCase` | `De100MemoryBlock`, `De100GameInput`        |
| Engine enums     | `DE100_SCREAMING` | `DE100_MEMORY_OK`, `DE100_FILE_ERROR`       |
| Engine functions | `de100_snake`     | `de100_memory_alloc()`, `de100_file_copy()` |
| Game types       | `PascalCase`      | `PlayerState`, `EnemyConfig`                |
| Game enums       | `SCREAMING_SNAKE` | `PLAYER_IDLE`, `ENEMY_ATTACKING`            |
| Game functions   | `snake_case`      | `update_player()`, `spawn_enemy()`          |
| Variables        | `snake_case`      | `frame_counter`, `is_valid`                 |
| Globals          | `g_snake_case`    | `g_frame_counter`, `g_is_running`           |
| Macros/Constants | `DE100_SCREAMING` | `DE100_KILOBYTES()`, `DE100_INTERNAL`       |
| Files            | `kebab-case`      | `debug-file-io.c`, `game-loader.h`          |

---

### Engine Types

```c
// engine/_common/memory.h
typedef struct {
    void *base;
    size_t size;
    bool is_valid;
} De100MemoryBlock;

typedef enum {
    DE100_MEMORY_OK = 0,
    DE100_MEMORY_ERR_OUT_OF_MEMORY,
    DE100_MEMORY_ERR_INVALID_SIZE,
} De100MemoryError;

typedef enum {
    DE100_MEMORY_FLAG_NONE    = 0,
    DE100_MEMORY_FLAG_READ    = 1 << 0,
    DE100_MEMORY_FLAG_WRITE   = 1 << 1,
    DE100_MEMORY_FLAG_EXECUTE = 1 << 2,
} De100MemoryFlags;
```

---

### Engine Functions

```c
// Pattern: de100_module_action()
De100MemoryBlock de100_memory_alloc(void *base, size_t size, De100MemoryFlags flags);
De100MemoryError de100_memory_free(De100MemoryBlock *block);
const char *de100_memory_error_str(De100MemoryError error);

De100FileResult de100_file_copy(const char *src, const char *dst);
De100PathResult de100_path_join(const char *dir, const char *file);

real64 de100_time_get_wall_clock(void);
void de100_time_sleep_ms(uint32 ms);
```

---

### Game Types (Your Game - No Prefix)

```c
// games/handmade-hero/src/main.h
typedef struct {
    real32 frequency;
    real32 volume;
} SoundSource;

typedef struct {
    SoundSource tone;
    int offset_x;
    int offset_y;
} GameState;
```

---

### Game Functions (Your Game - No Prefix)

```c
// games/handmade-hero/src/main.c
void update_player(GameState *state, De100GameInput *input);
void render_gradient(De100GameBackBuffer *buffer, int offset_x, int offset_y);
```

---

### Variables

```c
// Local variables
int32 frame_counter = 0;
real32 delta_time = 0.016f;
bool is_running = true;

// Globals (g_ prefix)
file_scoped_global_var bool g_is_running = true;
file_scoped_global_var int32 g_frame_counter = 0;

// Struct members
typedef struct {
    int32 sample_rate;
    int32 buffer_size;
    bool is_initialized;
} AudioConfig;
```

---

### Macros & Constants

```c
// Engine macros - DE100_ prefix
#define DE100_KILOBYTES(n) ((n) * 1024LL)
#define DE100_MEGABYTES(n) (DE100_KILOBYTES(n) * 1024LL)
#define DE100_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DE100_INTERNAL 1
#define DE100_SLOW 1

// Game macros - no prefix
#define MAX_ENEMIES 100
#define PLAYER_SPEED 5.0f
```

---

### File Structure

```
project/
├── engine/
│   ├── _common/
│   │   ├── base.h
│   │   ├── memory.h          ← De100MemoryBlock
│   │   ├── memory.c          ← de100_memory_alloc()
│   │   ├── debug-file-io.h
│   │   └── debug-file-io.c
│   ├── game/
│   │   ├── game-loader.h     ← De100GameCode
│   │   └── input.h           ← De100GameInput
│   └── platform/
│       └── x11/
│           ├── backend.c
│           └── audio.c
└── games/handmade-hero/
    └── src/
        ├── main.c            ← GameState (no prefix)
        └── startup.c
```

---

### Header Guards

```c
// Engine headers
#ifndef DE100_COMMON_MEMORY_H
#define DE100_COMMON_MEMORY_H
// ...
#endif // DE100_COMMON_MEMORY_H

// Game headers
#ifndef HANDMADE_HERO_MAIN_H
#define HANDMADE_HERO_MAIN_H
// ...
#endif // HANDMADE_HERO_MAIN_H
```

---

### Comments

```c
// Single-line for brief notes

/*
 * Multi-line for longer explanations
 * that span multiple lines.
 */

// ═══════════════════════════════════════════════════════════
// SECTION HEADERS (for major divisions)
// ═══════════════════════════════════════════════════════════
```

---

### Standard Macros (from `base.h`)

```c
file_scoped_fn          // static function
file_scoped_global_var  // static global
local_persist_var       // static local

DE100_ASSERT(expr)      // Always active
DE100_DEV_ASSERT(expr)  // Only in DE100_SLOW builds
```

---

### Do's and Don'ts

```c
// ✅ DO - Engine code
typedef struct { ... } De100GameState;
De100MemoryBlock de100_memory_alloc(...);
#define DE100_MAX_CONTROLLERS 4

// ✅ DO - Game code
typedef struct { ... } PlayerState;
void update_player(PlayerState *state);
#define MAX_ENEMIES 100

// ❌ DON'T
typedef struct { ... } memoryBlock;     // Wrong: camelCase type
void De100MemoryAlloc(...);             // Wrong: PascalCase function
int32 frameCounter = 0;                 // Wrong: camelCase variable
```

---

### Summary

```
ENGINE (reusable):
  Types:      De100PascalCase
  Functions:  de100_module_action
  Enums:      DE100_SCREAMING_SNAKE
  Macros:     DE100_SCREAMING_SNAKE

GAME (your code):
  Types:      PascalCase
  Functions:  snake_case
  Enums:      SCREAMING_SNAKE
  Macros:     SCREAMING_SNAKE

SHARED:
  Variables:  snake_case
  Globals:    g_snake_case
  Files:      kebab-case
```
