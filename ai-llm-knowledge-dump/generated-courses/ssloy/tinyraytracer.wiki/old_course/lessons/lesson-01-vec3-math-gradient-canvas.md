# Lesson 01 — Vec3 Math + Gradient Canvas

> **What you'll build:** By the end of this lesson, a color gradient fills the window — your first pixels rendered via the raytracing pipeline.

## Observable outcome

A window opens showing a smooth color gradient: red increases top-to-bottom, green increases left-to-right, producing a black-green-red-yellow gradient (no blue component). The gradient is rendered by writing directly to the platform backbuffer via `GAME_RGB`. This proves the rendering pipeline works end-to-end.

## New concepts

- `Vec3` struct + `vec3_make` compound literal constructor — a 3D vector type for positions, directions, and colors
- `vec3_add`/`vec3_sub`/`vec3_scale` — component-wise arithmetic (replaces C++ operator overloading)
- Float-to-pixel conversion — mapping `[0.0, 1.0]` float colors to `GAME_RGB(r, g, b)` with `r,g,b` in `[0, 255]`

## Files changed

| File                      | Change type | Summary                                                                                      |
| ------------------------- | ----------- | -------------------------------------------------------------------------------------------- |
| `build-dev.sh`            | Adapted     | Game sources, `-lm` for math, title "TinyRaytracer"                                          |
| `platform.h`              | Adapted     | `GAME_TITLE "TinyRaytracer"`, `GAME_W 800`, `GAME_H 600`                                     |
| `game/base.h`             | Adapted     | `quit` button only (1 button); `play_tone` removed                                           |
| `game/vec3.h`             | Created     | `Vec3` struct + `vec3_make`, `vec3_add`, `vec3_sub`, `vec3_scale`, `vec3_negate`, `vec3_mul` |
| `game/main.h`             | Created     | `RaytracerState` stub; `game_init`/`game_update`/`game_render` declarations                  |
| `game/main.c`             | Created     | Stub implementations; gradient render loop                                                   |
| `game/render.h`           | Created     | Minimal — `RtCamera` stub, `render_scene` declaration                                        |
| `game/render.c`           | Created     | Simple per-pixel loop writing gradient to backbuffer                                         |
| `platforms/raylib/main.c` | Adapted     | Audio stripped; `game_update`/`game_render` calls                                            |
| `platforms/x11/main.c`    | Adapted     | Audio stripped; `game_update`/`game_render` calls                                            |

## Background — why this works

### JS analogy

In JavaScript you'd write `class Vec3 { constructor(x,y,z) { this.x = x; ... } }` with methods like `add(other)`. In C there are no classes, constructors, or operator overloading. Instead:

```c
typedef struct { float x, y, z; } Vec3;

/* "Constructor" — returns a value, no heap allocation */
static inline Vec3 vec3_make(float x, float y, float z) {
  return (Vec3){x, y, z};  /* C99 compound literal */
}
```

The `(Vec3){x, y, z}` syntax is a **compound literal** — C99's equivalent of `new Vec3(x,y,z)` but allocated on the stack, not the heap. No `malloc`, no `free`.

### How it works in C

Every vec3 operation is an explicit function call:

```c
Vec3 a = vec3_make(1.0f, 0.0f, 0.0f);
Vec3 b = vec3_make(0.0f, 1.0f, 0.0f);
Vec3 c = vec3_add(a, b);              /* JS: a.add(b) or a + b */
Vec3 d = vec3_scale(c, 0.5f);         /* JS: c.multiply(0.5)   */
```

All functions are `static inline` because they'll be called millions of times per frame (once per pixel × multiple operations). The compiler inlines them at the call site — zero function-call overhead.

### Why float, not double

We use `float` (32-bit) not `double` (64-bit) because:

1. GPU rendering uses float exclusively — learning with float matches real-world practice
2. Half the memory bandwidth — 3 floats = 12 bytes vs 3 doubles = 24 bytes
3. `sqrtf`, `sinf`, `cosf` are the float variants of math functions (requires `-lm` linker flag)

### The rendering loop

Each frame, `game_render` writes to the backbuffer pixel by pixel:

```c
for (int j = 0; j < bb->height; j++) {
  for (int i = 0; i < bb->width; i++) {
    float r = (float)j / (float)bb->height;  /* 0→1 top to bottom */
    float g = (float)i / (float)bb->width;   /* 0→1 left to right */
    float b = 0.0f;
    bb->pixels[j * stride + i] = GAME_RGB(
      (int)(r * 255.0f),
      (int)(g * 255.0f),
      (int)(b * 255.0f)
    );
  }
}
```

`GAME_RGB(r,g,b)` packs three 0-255 byte values into a single `uint32_t` pixel. The platform layer uploads this pixel array to the GPU each frame.

## Code walkthrough

### `game/vec3.h` — complete file

```c
#ifndef GAME_VEC3_H
#define GAME_VEC3_H

#include <math.h>

typedef struct {
  float x, y, z;
} Vec3;

static inline Vec3 vec3_make(float x, float y, float z) {
  return (Vec3){x, y, z};
}

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
  return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
  return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 vec3_scale(Vec3 v, float s) {
  return (Vec3){v.x * s, v.y * s, v.z * s};
}

static inline Vec3 vec3_negate(Vec3 v) {
  return (Vec3){-v.x, -v.y, -v.z};
}

/* Component-wise multiply — used for color blending: material × light. */
static inline Vec3 vec3_mul(Vec3 a, Vec3 b) {
  return (Vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}

#endif /* GAME_VEC3_H */
```

**Key lines:**

- Line 7: `typedef struct { float x, y, z; } Vec3;` — the entire type is 12 bytes. No vtable, no methods, no hidden fields.
- Line 10: `return (Vec3){x, y, z};` — compound literal. This is stack-allocated and returned by value (the compiler optimizes the copy away).
- All functions take `Vec3` by value, not by pointer — for small structs (≤16 bytes) this is faster because the values stay in CPU registers.

### `game/main.c` — gradient rendering (stub)

```c
#include "main.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../platform.h"
#include <string.h>

void game_init(RaytracerState *state) {
  memset(state, 0, sizeof(*state));
}

void game_update(RaytracerState *state, GameInput *input, float delta_time) {
  (void)state; (void)input; (void)delta_time; /* unused for now */
}

void game_render(RaytracerState *state, Backbuffer *bb) {
  (void)state;
  int stride = bb->pitch / 4;

  for (int j = 0; j < bb->height; j++) {
    for (int i = 0; i < bb->width; i++) {
      /* Map pixel position to [0,1] float color. */
      float r = (float)j / (float)bb->height;
      float g = (float)i / (float)bb->width;
      float b = 0.0f;

      bb->pixels[j * stride + i] = GAME_RGB(
        (int)(r * 255.0f),
        (int)(g * 255.0f),
        (int)(b * 255.0f)
      );
    }
  }
}
```

**Key lines:**

- Line 8: `memset(state, 0, sizeof(*state))` — zero-init all fields. C doesn't auto-initialize like JS.
- Line 17: `bb->pitch / 4` — pitch is in bytes, pixels are 4 bytes each. `pitch` may differ from `width * 4` due to row alignment.
- Line 23-26: Float-to-pixel conversion. `r` ranges 0.0→1.0, multiply by 255 for byte range.

### Platform changes

> **Platform change:** The backends are adapted from the Platform Foundation Course template. Only the changed lines are shown. See **Platform Foundation Course** for full context.

**`game/base.h`** — simplified to 1 button:

```c
typedef struct {
  union {
    GameButtonState all[1];
    struct {
      GameButtonState quit;
    };
  } buttons;
} GameInput;
```

**Both backends** — replace `demo_render(&bb, ...)` with:

```c
game_update(&state, curr_input, dt);
game_render(&state, &bb);
```

## Common mistakes

| Symptom                    | Cause                               | Fix                                                                         |
| -------------------------- | ----------------------------------- | --------------------------------------------------------------------------- |
| Black window               | `GAME_RGB` arguments in wrong order | Check: `GAME_RGB(red, green, blue)` — red first                             |
| Gradient is sideways       | `i` and `j` swapped in the formula  | `j / height` = vertical (top→bottom), `i / width` = horizontal (left→right) |
| Compile error: `-lm`       | Missing math library link           | Add `-lm` to `BACKEND_LIBS` in `build-dev.sh`                               |
| Gradient has visible bands | Integer division instead of float   | Use `(float)j / (float)bb->height`, not `j / bb->height`                    |

## Exercise

> Change the gradient to produce a smooth blue→purple gradient instead of green→red. Hint: set `r` based on vertical position, `b` based on vertical position, and `g = 0`.

## JS ↔ C concept map

| JS / Web concept                            | C equivalent in this lesson                                   | Key difference                                    |
| ------------------------------------------- | ------------------------------------------------------------- | ------------------------------------------------- |
| `class Vec3 { constructor(x,y,z) { ... } }` | `typedef struct { float x, y, z; } Vec3` + `vec3_make(x,y,z)` | No constructors; compound literal `(Vec3){x,y,z}` |
| `vec.add(other)`                            | `vec3_add(a, b)`                                              | No operator overloading; explicit function calls  |
| `canvas.getContext('2d').fillRect(...)`     | `bb->pixels[j * stride + i] = GAME_RGB(r,g,b)`                | Direct pixel write; no canvas API                 |
| `requestAnimationFrame(render)`             | Platform loop calls `game_render()` each frame                | You don't own the loop; the platform calls you    |
