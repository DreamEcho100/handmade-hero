# Lesson 02 — Vec3 Advanced + PPM Export

> **What you'll build:** By the end of this lesson, your gradient image is saved as a PPM file you can open in any image viewer.

## Observable outcome

Pressing P exports the current backbuffer to `out.ppm` in the working directory. Open it with `feh out.ppm`, `eog out.ppm`, or any image viewer that supports PPM. The file contains the same gradient from Lesson 01, now persisted to disk.

## New concepts

- `vec3_dot`/`vec3_length`/`vec3_normalize` — dot product (scalar result), Euclidean length via `sqrtf`, and unit-vector normalization
- PPM P6 binary format — a header (`P6\n<width> <height>\n255\n`) followed by raw RGB byte triplets
- `fopen`/`fwrite`/`fclose` — C file I/O (three explicit steps; no garbage collector closes the file for you)

## Files changed

| File          | Change type | Summary                                         |
| ------------- | ----------- | ----------------------------------------------- |
| `game/vec3.h` | Modified    | Add `vec3_dot`, `vec3_length`, `vec3_normalize` |
| `game/ppm.h`  | Created     | PPM P6 binary writer (`ppm_export`)             |
| `game/main.c` | Modified    | Call `ppm_export` on P key press                |

## Background — why this works

### JS analogy

In JavaScript you'd compute a dot product manually: `a.x*b.x + a.y*b.y + a.z*b.z`. There's no built-in vector dot product. The same is true in C — we write an explicit function. The difference is that in C, these `static inline` functions compile to raw CPU instructions with zero overhead.

For file export, in JS you'd write `fs.writeFileSync('out.ppm', buffer)` — one call, done. In C, file I/O is a three-step process: `fopen` (open), `fwrite` (write bytes), `fclose` (close). If you forget `fclose`, the data may never reach disk.

### Dot product — what it means geometrically

The dot product `a . b = ax*bx + ay*by + az*bz` returns a scalar (a single number). Geometrically:

- If the result is positive, the vectors point in roughly the same direction
- If zero, they're perpendicular (90 degrees)
- If negative, they point in opposite directions

This single operation is the workhorse of raytracing. It measures:

- **How bright a surface is** (dot of light direction and surface normal = cosine of angle)
- **Whether a ray hits a sphere** (projection of center onto ray)
- **How shiny a highlight is** (dot of reflected light and view direction)

### Length and normalize

`vec3_length(v)` computes `sqrt(x^2 + y^2 + z^2)` — the Euclidean distance from the origin. But `sqrt(dot(v, v))` is the same thing, so we build `vec3_length` on top of `vec3_dot`:

```c
static inline float vec3_length(Vec3 v) {
  return sqrtf(vec3_dot(v, v));  /* dot(v,v) = x*x + y*y + z*z */
}
```

`vec3_normalize(v)` divides each component by the length, producing a unit vector (length = 1.0). Unit vectors are essential — they represent pure directions without any magnitude. Every ray direction, surface normal, and light direction in our raytracer will be normalized.

### PPM P6 format

PPM (Portable PixMap) is the simplest image format that supports color. The P6 variant is binary:

```
P6\n          <- magic number (binary PPM)
800 600\n     <- width height
255\n         <- max color value
RGBRGBRGB...  <- width*height RGB triplets, 1 byte each
```

No compression, no metadata, no library needed. Any image viewer on Linux can open it. We use this format because it lets us export a raytraced image with ~20 lines of C code.

### Why `"wb"` mode

In C, `fopen("out.ppm", "wb")` opens a file for **w**riting in **b**inary mode. The `b` flag prevents the C runtime from converting `\n` to `\r\n` on Windows. Since PPM P6 contains raw bytes (pixel data), any such conversion would corrupt the image.

## Code walkthrough

### `game/vec3.h` — new functions (added to existing file)

```c
/* ── LESSON 02 — Dot product, length, normalize ─────────────────────────
 * vec3_dot returns a scalar: a.b = ax*bx + ay*by + az*bz.
 * Used for lighting (cos of angle), intersection tests, and more.
 *
 * JS equivalent: no built-in — you'd write a.x*b.x + a.y*b.y + a.z*b.z */

static inline float vec3_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float vec3_length(Vec3 v) {
  return sqrtf(vec3_dot(v, v));
}

static inline Vec3 vec3_normalize(Vec3 v) {
  float len = vec3_length(v);
  if (len > 0.0f) {
    float inv = 1.0f / len;
    return (Vec3){v.x * inv, v.y * inv, v.z * inv};
  }
  return v;
}
```

**Key lines:**

- `vec3_dot` returns `float`, not `Vec3` — it's a scalar projection. This is the only vec3 operation that doesn't return a vector.
- `vec3_length` reuses `vec3_dot(v, v)` — `v . v = |v|^2`, so `sqrt(v . v) = |v|`.
- `vec3_normalize` guards against zero-length vectors (`len > 0.0f`). Dividing by zero would produce `NaN`/`Inf`, which propagate silently through all subsequent math and produce a black screen.
- `1.0f / len` computes the inverse once, then multiplies three times — one division is cheaper than three.

### `game/ppm.h` — complete file

```c
#ifndef GAME_PPM_H
#define GAME_PPM_H

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "../utils/backbuffer.h"

/* Clamp a float [0,1] to a byte [0,255]. */
static inline uint8_t float_to_byte(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  return (uint8_t)(v * 255.0f + 0.5f);
}

/* Export the backbuffer as a PPM P6 file.
 * The backbuffer stores pixels as 0xAARRGGBB (GAME_RGB format).           */
static inline void ppm_export(const Backbuffer *bb, const char *filename) {
  FILE *f = fopen(filename, "wb");
  if (!f) {
    fprintf(stderr, "ppm_export: cannot open '%s'\n", filename);
    return;
  }

  fprintf(f, "P6\n%d %d\n255\n", bb->width, bb->height);

  int stride = bb->pitch / 4;
  for (int y = 0; y < bb->height; y++) {
    for (int x = 0; x < bb->width; x++) {
      uint32_t pixel = bb->pixels[y * stride + x];
      /* GAME_RGB stores as 0x00BBGGRR on little-endian (R at bits 0-7):
       * Extract R, G, B from the packed uint32_t.                         */
      uint8_t r = (uint8_t)((pixel >>  0) & 0xFF);
      uint8_t g = (uint8_t)((pixel >>  8) & 0xFF);
      uint8_t b = (uint8_t)((pixel >> 16) & 0xFF);
      uint8_t rgb[3] = {r, g, b};
      fwrite(rgb, 1, 3, f);
    }
  }

  fclose(f);
  printf("Exported %s (%dx%d)\n", filename, bb->width, bb->height);
}

#endif /* GAME_PPM_H */
```

**Key lines:**

- `fopen(filename, "wb")` — `"wb"` = write + binary. Always check the return value: `fopen` returns `NULL` if the file can't be created (e.g., read-only directory).
- `fprintf(f, "P6\n%d %d\n255\n", ...)` — writes the PPM header as text. `fprintf` to a `FILE*` works just like `printf` to stdout.
- `(pixel >> 0) & 0xFF` — bitwise extraction. `GAME_RGB` packs R in bits 0-7, G in bits 8-15, B in bits 16-23. The `& 0xFF` masks out one byte.
- `fwrite(rgb, 1, 3, f)` — writes exactly 3 bytes (one RGB triplet). The arguments are: data pointer, element size, element count, file.
- `fclose(f)` — flushes the write buffer to disk and releases the file handle. Without this, data may be lost.
- `float_to_byte` includes `+ 0.5f` for rounding (0.5 rounds to 128, not 127). This utility will be reused in later lessons.

### `game/main.c` — PPM export wiring (relevant changes)

```c
#include "ppm.h"

void game_render(RaytracerState *state, Backbuffer *bb) {
  /* ... existing gradient render loop ... */

  /* Export on P key press. */
  if (state->export_ppm_requested) {
    ppm_export(bb, "out.ppm");
    state->export_ppm_requested = 0;
  }
}
```

The `export_ppm_requested` flag is set in `game_update` when the P key is pressed, then consumed in `game_render` after the frame is drawn. This ensures we export the current frame, not a blank buffer.

## Common mistakes

| Symptom                             | Cause                                                | Fix                                                                   |
| ----------------------------------- | ---------------------------------------------------- | --------------------------------------------------------------------- |
| `out.ppm` is 0 bytes                | Missing `fclose(f)` — data stuck in write buffer     | Always call `fclose(f)` after `fwrite`                                |
| Colors are wrong in PPM             | Bit-shift order doesn't match `GAME_RGB` packing     | Check your platform's `GAME_RGB` macro — R/G/B channel positions vary |
| `ppm_export: cannot open 'out.ppm'` | No write permission in the working directory         | Run from a directory where you have write access                      |
| `vec3_normalize` produces `NaN`     | Zero-length vector passed (e.g., `vec3_make(0,0,0)`) | The guard `if (len > 0.0f)` prevents this — make sure it's there      |
| Image viewer shows garbage          | Opened file in text mode (`"w"` instead of `"wb"`)   | Use `"wb"` to prevent newline conversion of binary data               |

## Exercise

> Modify `ppm_export` to export a 100x100 solid red image (ignore the backbuffer). Create a `uint8_t rgb[3] = {255, 0, 0}` and `fwrite` it 10,000 times. Open the result to verify a red square.

## JS ↔ C concept map

| JS / Web concept                               | C equivalent in this lesson                 | Key difference                                      |
| ---------------------------------------------- | ------------------------------------------- | --------------------------------------------------- |
| `a.x*b.x + a.y*b.y + a.z*b.z`                  | `vec3_dot(a, b)`                            | Named function; returns `float`, not `Vec3`         |
| `Math.sqrt(x)`                                 | `sqrtf(x)` from `<math.h>`                  | `sqrtf` for `float`; requires `-lm` linker flag     |
| `v.normalize()` / `v.divideScalar(v.length())` | `vec3_normalize(v)`                         | Guards against zero-length; computes `1/len` once   |
| `fs.writeFileSync('out.ppm', buf)`             | `fopen` + `fwrite` + `fclose`               | Three steps; must close manually; `"wb"` for binary |
| `buffer[i]` (Uint8Array)                       | `fwrite(rgb, 1, 3, f)`                      | Write raw bytes to file; no encoding layer          |
| `try { fs.writeFileSync(...) } catch(e) {}`    | `if (!f) { fprintf(stderr, ...); return; }` | Check `fopen` return; no exceptions in C            |
