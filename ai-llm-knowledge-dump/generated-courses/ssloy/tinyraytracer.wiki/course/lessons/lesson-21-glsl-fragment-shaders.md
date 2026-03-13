# Lesson 21 — GLSL Fragment Shaders

> **What you'll build:** By the end of this lesson, pressing **Shift+L** exports a complete Shadertoy-compatible GLSL fragment shader (`raytracer.glsl`) that replicates our CPU raytracer on the GPU. You can paste it directly into [shadertoy.com](https://www.shadertoy.com) and watch the same scene render in real time at full resolution -- every pixel computed in parallel by the GPU, no pthreads needed.

## Observable outcome

The HUD now shows `L:glsl` as an available export. Pressing **Shift+L** writes a file `raytracer.glsl` to the working directory and prints `Exported GLSL shader: raytracer.glsl (N bytes)` to stdout. Opening that file in Shadertoy produces the familiar 4-sphere scene with ivory, rubber, mirror, and glass materials, 3 point lights, Phong shading, a checkerboard floor, hard shadows, and reflections -- all running at 60 fps on a modern GPU.

## New concepts

- Storing a complete GLSL program as a C string constant (`static const char[]`) for export
- C-to-GLSL mapping -- how our manual `vec3_add`, `vec3_dot`, `vec3_reflect` calls become native operators and built-in functions
- Iterative reflection with a weight accumulator -- GLSL forbids recursion, so `cast_ray` uses a `for` loop instead of calling itself
- `out` parameters -- GLSL's replacement for returning structs (replaces our `HitRecord`)
- `mainImage(out vec4, in vec2)` -- the per-pixel entry point that replaces our `render_scene` loop; the GPU launches one thread per pixel
- `fputs` for writing a C string constant to a file

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/shader_glsl.h` | Created | `GLSL_RAYTRACER_SOURCE[]` C string constant containing the complete GLSL shader; `shader_export_glsl()` writes it to a file |
| `game/main.c` | Modified | `#include "shader_glsl.h"`; Shift+L triggers export; HUD shows `L:glsl` |
| `game/main.h` | Modified | `export_glsl_requested` flag added to `RaytracerState` |
| `game/base.h` | Modified | `export_glsl` button added to `GameInput` |

## Background -- why this works

### JS analogy

In JavaScript, you write GLSL shaders as template literal strings and pass them to `WebGLRenderingContext.shaderSource()`. The browser compiles and runs them on the GPU. Shadertoy abstracts this further -- you just write the `mainImage` function and the site handles compilation, uniform injection (`iResolution`, `iMouse`, `iTime`), and the fullscreen quad.

Our approach is the same idea in C: store the shader source as a string, write it to a file, and let the user paste it into Shadertoy. We are not compiling GLSL ourselves -- we are exporting a text file.

### Why GLSL has no recursion

GPU hardware runs thousands of threads simultaneously on a SIMD architecture. Each thread has a tiny fixed-size stack (or no stack at all -- registers only). Recursion would require a variable-size call stack per thread, which the hardware cannot provide efficiently. The GLSL spec therefore forbids recursive function calls entirely.

Our C `cast_ray` calls itself for reflections and refractions. The GLSL version replaces this with a `for` loop that accumulates a `weight` multiplier:

```
color  = vec3(0.0);
weight = 1.0;

for (depth = 0; depth < MAX_DEPTH; depth++) {
    if (miss) { color += weight * sky; break; }
    color += weight * (diffuse + specular);  // local lighting
    weight *= reflectivity;                  // attenuate
    dir = reflect(dir, N);                   // bounce
    orig = hit_point + N * epsilon;          // step off surface
}
```

Each iteration handles one bounce. The `weight` variable tracks how much energy the reflected ray carries -- it decreases geometrically with each reflection. When `weight` is multiplied into the accumulated color, the result is mathematically equivalent to the recursive version (for reflections; refractions are omitted for simplicity in this shader).

### C-to-GLSL operation mapping

| C (our code) | GLSL equivalent | Notes |
|---|---|---|
| `vec3_add(a, b)` | `a + b` | GLSL overloads `+` for vec types |
| `vec3_sub(a, b)` | `a - b` | Same for subtraction |
| `vec3_scale(v, s)` | `v * s` | Scalar-vector multiply is built in |
| `vec3_dot(a, b)` | `dot(a, b)` | Built-in; maps to a single GPU instruction |
| `vec3_cross(a, b)` | `cross(a, b)` | Built-in |
| `vec3_normalize(v)` | `normalize(v)` | Built-in |
| `vec3_reflect(I, N)` | `reflect(I, N)` | Built-in; same formula |
| `vec3_lerp(a, b, t)` | `mix(a, b, t)` | GLSL calls lerp "mix" |
| `HitRecord` struct return | `out float t, out vec3 N, out Material mat` | GLSL uses `out` parameters instead |
| `for j/i` loop in `render_scene` | GPU launches `mainImage` per pixel | Implicit parallelism |

### How `mainImage` maps to our render loop

In our C code, `render_scene` iterates over every pixel in a double `for` loop, computing a ray direction and calling `cast_ray`. In GLSL, Shadertoy calls `mainImage(out vec4 fragColor, in vec2 fragCoord)` once per pixel, and the GPU runs all of them in parallel. The `fragCoord` argument gives the pixel coordinate (like our loop variables `i, j`), and `iResolution` gives the viewport size (like our `width, height`). The ray direction computation is identical:

```
// C version (in render_scene):
float x =  (2.0f * (i + 0.5f) / width  - 1.0f) * tan(fov/2) * aspect;
float y = -(2.0f * (j + 0.5f) / height - 1.0f) * tan(fov/2);

// GLSL version (in mainImage):
vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
vec3 dir = normalize(vec3(uv * tan(FOV / 2.0), -1.0));
```

The GLSL version is more compact because GLSL supports vector arithmetic natively -- `2.0 * fragCoord - iResolution.xy` does component-wise subtraction on `vec2`, and dividing by `iResolution.y` normalizes both components by the height (baking in the aspect ratio).

## Code walkthrough

### `game/shader_glsl.h` -- complete file

```c
#ifndef GAME_SHADER_GLSL_H
#define GAME_SHADER_GLSL_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/shader_glsl.h — TinyRaytracer Course (L21)
 * ═══════════════════════════════════════════════════════════════════════════
 * Complete GLSL Shadertoy raytracer as a C string constant.
 * Press Shift+L to export to "raytracer.glsl".
 *
 * This shader replicates our CPU raytracer on the GPU:
 *   - 4 spheres with ivory/rubber/mirror/glass materials
 *   - 3 point lights with Phong shading
 *   - Checkerboard floor
 *   - Hard shadows + reflections (iterative, no recursion in GLSL)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>

/* ── The complete GLSL fragment shader ─────────────────────────────────── */
static const char GLSL_RAYTRACER_SOURCE[] =
"// ═══════════════════════════════════════════════════════════════════════\n"
"// TinyRaytracer — Shadertoy GLSL port\n"
"// ═══════════════════════════════════════════════════════════════════════\n"
"// Paste into https://www.shadertoy.com as a new shader.\n"
"// Matches the CPU raytracer from the TinyRaytracer Course.\n"
"//\n"
"// PERFORMANCE TIPS:\n"
"//   1. GPU executes mainImage() in parallel for every pixel — no threads\n"
"//      needed, the hardware handles it.\n"
"//   2. Avoid divergent branches (if/else where threads take different\n"
"//      paths) — modern GPUs handle this with masking but it still costs.\n"
"//   3. Use built-in functions: reflect(), mix(), clamp(), dot() — these\n"
"//      map to single GPU instructions.\n"
"//   4. Minimize texture reads; our shader uses none (procedural only).\n"
"//   5. Loop unrolling: the compiler does this automatically for small\n"
"//      constant-bound loops (our N_SPHERES=4, N_LIGHTS=3).\n"
"//   6. To add iChannel0 environment map: replace the sky gradient in\n"
"//      cast_ray with: texture(iChannel0, dir).rgb\n"
"// ═══════════════════════════════════════════════════════════════════════\n"
"\n"
"const int   MAX_DEPTH   = 3;\n"
"const float FOV         = 1.0472; // PI/3\n"
"const vec3  BG_TOP      = vec3(0.2, 0.7, 0.8);\n"
"const vec3  BG_BOT      = vec3(1.0, 1.0, 1.0);\n"
"\n"
"// ── Materials ────────────────────────────────────────────────────────\n"
"struct Material {\n"
"    vec3  color;\n"
"    vec4  albedo;  // diffuse, specular, reflect, refract\n"
"    float spec_exp;\n"
"    float ior;\n"
"};\n"
"\n"
"const Material ivory   = Material(vec3(0.4,0.4,0.3), vec4(0.6,0.3,0.1,0.0), 50.0, 1.0);\n"
"const Material rubber  = Material(vec3(0.3,0.1,0.1), vec4(0.9,0.1,0.0,0.0), 10.0, 1.0);\n"
"const Material mirror  = Material(vec3(1.0,1.0,1.0), vec4(0.0,10.,0.8,0.0), 1425., 1.0);\n"
"const Material glass   = Material(vec3(0.6,0.7,0.8), vec4(0.0,0.5,0.1,0.8), 125., 1.5);\n"
"\n"
"// ── Spheres ─────────────────────────────────────────────────────────\n"
"const int N_SPHERES = 4;\n"
"const vec4 spheres[4] = vec4[4](       // xyz = center, w = radius\n"
"    vec4(-3.0, 0.0,-16.0, 2.0),\n"
"    vec4(-1.0,-1.5,-12.0, 2.0),\n"
"    vec4( 1.5,-0.5,-18.0, 3.0),\n"
"    vec4( 7.0, 5.0,-18.0, 4.0)\n"
");\n"
"const int sphere_mat[4] = int[4](0, 3, 1, 2); // material indices\n"
"\n"
"// ── Lights ──────────────────────────────────────────────────────────\n"
"const int N_LIGHTS = 3;\n"
"const vec3 light_pos[3] = vec3[3](\n"
"    vec3(-20., 20., 20.),\n"
"    vec3( 30., 50.,-25.),\n"
"    vec3( 30., 20., 30.)\n"
");\n"
"const float light_int[3] = float[3](1.5, 1.8, 1.7);\n"
"\n"
"// ── Ray-sphere intersection ─────────────────────────────────────────\n"
"// Same algorithm as our C sphere_intersect:\n"
"//   L = center - origin\n"
"//   tca = dot(L, dir)       → projection of L onto ray\n"
"//   d2 = |L|^2 - tca^2     → squared distance from center to ray\n"
"//   thc = sqrt(r^2 - d2)   → half-chord length\n"
"//   t = tca - thc           → nearest intersection\n"
"float sphere_hit(vec3 orig, vec3 dir, vec4 sph) {\n"
"    vec3  L   = sph.xyz - orig;\n"
"    float tca = dot(L, dir);\n"
"    float d2  = dot(L, L) - tca * tca;\n"
"    float r2  = sph.w * sph.w;\n"
"    if (d2 > r2) return -1.0;\n"
"    float thc = sqrt(r2 - d2);\n"
"    float t0  = tca - thc;\n"
"    float t1  = tca + thc;\n"
"    if (t0 < 0.0) t0 = t1;\n"
"    if (t0 < 0.0) return -1.0;\n"
"    return t0;\n"
"}\n"
"\n"
"// ── Scene intersection ──────────────────────────────────────────────\n"
"bool scene_hit(vec3 orig, vec3 dir,\n"
"               out float t, out vec3 N, out Material mat) {\n"
"    t = 1e10;\n"
"    // Spheres\n"
"    for (int i = 0; i < N_SPHERES; i++) {\n"
"        float d = sphere_hit(orig, dir, spheres[i]);\n"
"        if (d > 0.001 && d < t) {\n"
"            t = d;\n"
"            vec3 pt = orig + dir * d;\n"
"            N   = normalize(pt - spheres[i].xyz);\n"
"            mat = (sphere_mat[i]==0) ? ivory :\n"
"                  (sphere_mat[i]==1) ? rubber :\n"
"                  (sphere_mat[i]==2) ? mirror : glass;\n"
"        }\n"
"    }\n"
"    // Checkerboard floor (same as our plane intersection y=-4)\n"
"    if (abs(dir.y) > 1e-3) {\n"
"        float d = -(orig.y + 4.0) / dir.y;\n"
"        if (d > 0.001 && d < t) {\n"
"            vec3 pt = orig + dir * d;\n"
"            if (abs(pt.x) < 30.0 && abs(pt.z) < 50.0) {\n"
"                t = d;\n"
"                N = vec3(0., 1., 0.);\n"
"                int checker = int(floor(pt.x) + floor(pt.z)) & 1;\n"
"                mat = Material(\n"
"                    checker==1 ? vec3(0.3) : vec3(0.3,0.2,0.1),\n"
"                    vec4(1.,0.,0.,0.), 1.0, 1.0);\n"
"            }\n"
"        }\n"
"    }\n"
"    return t < 1e9;\n"
"}\n"
"\n"
"// ── Refraction (Snell's law) ────────────────────────────────────────\n"
"// Same as our refract() in refract.h, but GLSL has a built-in refract().\n"
"// We use the built-in here.\n"
"\n"
"// ── cast_ray — iterative (GLSL has no recursion) ────────────────────\n"
"// Our C version is recursive: cast_ray calls cast_ray for reflections\n"
"// and refractions.  GLSL forbids recursion, so we unroll into a loop\n"
"// with a weight accumulator.\n"
"//\n"
"// Optimization note: this loop has at most MAX_DEPTH=3 iterations.\n"
"// The GLSL compiler may fully unroll it for better GPU performance.\n"
"vec3 cast_ray(vec3 orig, vec3 dir) {\n"
"    vec3  color  = vec3(0.0);\n"
"    float weight = 1.0;\n"
"\n"
"    for (int depth = 0; depth < MAX_DEPTH; depth++) {\n"
"        float t;\n"
"        vec3  N;\n"
"        Material mat;\n"
"\n"
"        if (!scene_hit(orig, dir, t, N, mat)) {\n"
"            // Sky gradient (replace with texture(iChannel0, dir).rgb\n"
"            // for environment mapping)\n"
"            float sky = 0.5 * (dir.y + 1.0);\n"
"            color += weight * mix(BG_TOP, BG_BOT, sky * 0.3);\n"
"            break;\n"
"        }\n"
"\n"
"        vec3 pt = orig + dir * t;\n"
"\n"
"        // ── Phong lighting (same as our compute_lighting) ──────\n"
"        float diff_i = 0.0, spec_i = 0.0;\n"
"        for (int l = 0; l < N_LIGHTS; l++) {\n"
"            vec3  ldir = normalize(light_pos[l] - pt);\n"
"            float ln   = dot(ldir, N);\n"
"\n"
"            // Shadow ray\n"
"            float st; vec3 sN; Material sM;\n"
"            vec3 sOrig = pt + N * 0.001;\n"
"            if (scene_hit(sOrig, ldir, st, sN, sM) &&\n"
"                st < length(light_pos[l] - pt)) continue;\n"
"\n"
"            diff_i += light_int[l] * max(0.0, ln);\n"
"            // reflect(-ldir, N) = our vec3_reflect\n"
"            spec_i += light_int[l] *\n"
"                pow(max(0.0, dot(reflect(-ldir, N), -dir)), mat.spec_exp);\n"
"        }\n"
"\n"
"        vec3 diffuse  = mat.color * diff_i;\n"
"        vec3 specular = vec3(spec_i);\n"
"\n"
"        color += weight * (diffuse * mat.albedo.x + specular * mat.albedo.y);\n"
"\n"
"        // ── Reflection (iterative replacement for recursion) ───\n"
"        if (mat.albedo.z > 0.0) {\n"
"            weight *= mat.albedo.z;\n"
"            dir  = reflect(dir, N);\n"
"            orig = pt + N * 0.001;\n"
"        } else {\n"
"            break;\n"
"        }\n"
"    }\n"
"    return color;\n"
"}\n"
"\n"
"// ── Main entry point ────────────────────────────────────────────────\n"
"// Shadertoy calls this for every pixel every frame.\n"
"// fragCoord = pixel coordinates, iResolution = viewport size.\n"
"// This is equivalent to our render_scene loop + trace_pixel.\n"
"//\n"
"// Optimization: the entire mainImage runs in parallel on the GPU —\n"
"// one thread per pixel.  A 1920x1080 frame launches ~2M threads.\n"
"// Our CPU version needs pthreads for parallelism; the GPU gets it\n"
"// for free.\n"
"void mainImage(out vec4 fragColor, in vec2 fragCoord) {\n"
"    // Map pixel to normalized device coordinates [-1,1]\n"
"    // Same as our: x = (2*px/width - 1) * tan(fov/2) * aspect\n"
"    vec2  uv  = (2.0 * fragCoord - iResolution.xy) / iResolution.y;\n"
"    float fov = tan(FOV / 2.0);\n"
"    vec3  dir = normalize(vec3(uv * fov, -1.0));\n"
"\n"
"    // Optional: interactive camera with iMouse\n"
"    // Uncomment to enable mouse orbit (like our OrbitControls):\n"
"    // vec2 m = iMouse.xy / iResolution.xy - 0.5;\n"
"    // float yaw   = m.x * 6.28;\n"
"    // float pitch = m.y * 1.5;\n"
"    // mat3 rot = mat3(cos(yaw), 0, sin(yaw),\n"
"    //                 0, 1, 0,\n"
"    //                 -sin(yaw), 0, cos(yaw));\n"
"    // dir = rot * dir;\n"
"\n"
"    fragColor = vec4(cast_ray(vec3(0.0), dir), 1.0);\n"
"}\n";

/* ── Export function ──────────────────────────────────────────────────── */
static inline int shader_export_glsl(const char *filename) {
  FILE *f = fopen(filename, "w");
  if (!f) {
    fprintf(stderr, "shader_export_glsl: cannot write '%s'\n", filename);
    return -1;
  }
  fputs(GLSL_RAYTRACER_SOURCE, f);
  fclose(f);
  printf("Exported GLSL shader: %s (%d bytes)\n",
         filename, (int)sizeof(GLSL_RAYTRACER_SOURCE) - 1);
  return 0;
}

#endif /* GAME_SHADER_GLSL_H */
```

**Key lines:**

- **`static const char GLSL_RAYTRACER_SOURCE[]` (line 21):** The entire GLSL shader is stored as a single C string constant. Each line is a separate string literal -- the C compiler concatenates adjacent string literals into one. The `static` keyword prevents duplicate-symbol link errors if the header is included by multiple .c files.

- **`struct Material` in GLSL (inside the string):** GLSL supports structs like C, but with built-in constructor syntax: `Material(vec3(...), vec4(...), 50.0, 1.0)`. In C we use designated initializers or field-by-field assignment instead.

- **`out float t, out vec3 N, out Material mat` in `scene_hit`:** GLSL `out` parameters are the equivalent of pointer parameters in C. Where our C code fills a `HitRecord *hit` struct, the GLSL version writes into three separate `out` variables. GLSL has no pointers.

- **Iterative `cast_ray` with `weight` accumulator:** The loop replaces recursion. `color += weight * local_contribution` adds the lighting for this bounce, then `weight *= mat.albedo.z` attenuates for the next bounce. When `albedo.z == 0` (non-reflective surface), the loop breaks early.

- **`reflect(dir, N)` and `mix(BG_TOP, BG_BOT, sky * 0.3)` (built-ins):** These replace our manual `vec3_reflect` and `vec3_lerp`. GLSL built-ins compile to single GPU instructions.

- **`mainImage(out vec4 fragColor, in vec2 fragCoord)`:** The Shadertoy entry point. `fragCoord` is the pixel position (equivalent to our loop variables `i, j`). `iResolution` is a Shadertoy-provided uniform (equivalent to our `width, height`). The `out vec4 fragColor` is the pixel color output.

- **`shader_export_glsl` (line 229):** Opens a file, writes the entire string with `fputs`, closes it. `sizeof(GLSL_RAYTRACER_SOURCE) - 1` gives the string length without the null terminator -- this works because `sizeof` on a char array includes the null byte.

### Integration in `game/main.h` -- relevant additions

```c
typedef struct {
  Scene          scene;
  RtCamera       camera;
  RenderSettings settings;
  /* Adaptive quality state. */
  int            still_frames;
  int            active_scale;
  /* Performance metrics. */
  float          frame_time_ms;     /* last frame render time              */
  float          fps_smoothed;      /* exponential moving average FPS      */
  /* Export flags. */
  int            export_ppm_requested;
  int            export_anaglyph_requested;
  int            export_sidebyside_requested;
  int            export_stereogram_requested;
  int            export_stereogram_cross_requested;
  int            export_glsl_requested;
} RaytracerState;
```

**Key lines:**

- **`export_glsl_requested` (line 26):** A simple boolean flag. Set to 1 by `game_update` when Shift+L is pressed, consumed and cleared by `game_render` after the export completes. This is the same one-shot pattern used for all other exports (PPM, anaglyph, stereogram).

### Integration in `game/base.h` -- relevant additions

```c
typedef struct {
  union {
    GameButtonState all[26];
    struct {
      /* Navigation */
      GameButtonState quit;
      GameButtonState camera_left;
      GameButtonState camera_right;
      GameButtonState camera_up;
      GameButtonState camera_down;
      GameButtonState camera_forward;
      GameButtonState camera_backward;
      /* Exports */
      GameButtonState export_ppm;
      GameButtonState export_anaglyph;
      GameButtonState export_sidebyside;
      GameButtonState export_stereogram;
      GameButtonState export_stereogram_cross;
      GameButtonState export_glsl;          /* Shift+L (L21) */
      /* Feature toggles */
      GameButtonState toggle_voxels;      /* V */
      GameButtonState toggle_floor;       /* F */
      GameButtonState toggle_boxes;       /* B */
      GameButtonState toggle_meshes;      /* M */
      GameButtonState toggle_reflections; /* R */
      GameButtonState toggle_refractions; /* T */
      GameButtonState toggle_shadows;     /* H */
      GameButtonState toggle_envmap;      /* E */
      GameButtonState toggle_aa;          /* X */
      GameButtonState cycle_envmap_mode;  /* C (L20) */
      /* Render scale */
      GameButtonState scale_cycle;        /* Tab */
    };
  } buttons;
  MouseState mouse;
} GameInput;
```

**Key lines:**

- **`export_glsl` (line 62):** Added to the named button struct alongside the other export buttons. The platform layer (X11 or Raylib) maps Shift+L to this button via `UPDATE_BUTTON`. The `all[26]` array size must be large enough to cover all named fields -- if you add a button and forget to bump this count, the last buttons in the struct overlap with `mouse`, causing silent corruption.

### Integration in `game/main.c` -- relevant additions

```c
#include "shader_glsl.h"
```

In `game_update`:

```c
  if (button_just_pressed(input->buttons.export_glsl))
    state->export_glsl_requested = 1;
```

In `game_render`:

```c
  if (state->export_glsl_requested) {
    shader_export_glsl("raytracer.glsl");
    state->export_glsl_requested = 0;
  }
```

In the HUD line 4:

```c
  snprintf(l4, sizeof(l4),
    "dist:%.1f yaw:%.0f pitch:%.0f | P:ppm G:sirds L:glsl",
    state->camera.orbit_radius,
    state->camera.yaw   * (180.0f / (float)M_PI),
    state->camera.pitch  * (180.0f / (float)M_PI));
```

**Key lines:**

- **`#include "shader_glsl.h"` (line 13):** Brings in both the string constant and the export function. Because both are `static`, they exist only in this translation unit -- no duplicate symbol issues.

- **`shader_export_glsl("raytracer.glsl")` (line 212):** The export call. The filename is hardcoded. The file is written to the current working directory (wherever the binary was launched from).

- **`L:glsl` in HUD (line 175):** Added to the fourth HUD line so the user knows the keybinding exists. Follows the same terse format as `P:ppm` and `G:sirds`.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Shadertoy shows a compile error on `Material(...)` constructor | Shadertoy is running GLSL ES 1.0 which lacks struct constructors | Ensure you paste into a "New Shader" on shadertoy.com (GLSL ES 3.0); do not use the mobile preview |
| Scene appears but all surfaces are black | Lights are missing or `light_int` values are zero | Verify `N_LIGHTS = 3` and `light_int` array matches the source |
| No reflections on the mirror sphere | `MAX_DEPTH` set to 1 | Ensure `MAX_DEPTH = 3`; each iteration handles one bounce |
| Shader runs but at < 10 fps | Very old GPU or large viewport | Reduce Shadertoy resolution (click the resolution button); reduce `MAX_DEPTH` |
| Export does nothing / no file created | Working directory is read-only or wrong path | Check `stderr` for the `cannot write` message; run the binary from a writable directory |
| Exported file is 0 bytes | `fputs` failed silently | Verify `GLSL_RAYTRACER_SOURCE` is not empty; check disk space |
| Checkerboard floor has Moire patterns in Shadertoy | No anti-aliasing in the shader | The CPU version uses AA samples; the GLSL version does not. Add supersampling in `mainImage` by averaging 4 sub-pixel rays |
| Refractions missing (glass sphere is opaque) | The iterative loop only handles reflections, not refractions | This is a deliberate simplification; adding refractions requires a second ray per bounce or a more complex loop structure |

## Exercise

> Export `raytracer.glsl` by pressing Shift+L. Open [shadertoy.com](https://www.shadertoy.com), create a new shader, and replace the default code with the contents of `raytracer.glsl`. Verify the scene matches your CPU raytracer output. Then uncomment the `iMouse` camera rotation block in `mainImage` (the 6 lines starting with `vec2 m = iMouse.xy`), click "Compile", and drag the mouse across the viewport to orbit the camera -- compare the experience with our CPU orbit controls.

## JS / C / GLSL concept map

| JS / Web concept | C equivalent (our code) | GLSL equivalent (this lesson) | Key difference |
|---|---|---|---|
| `vec3.add(a, b)` (Three.js) | `vec3_add(a, b)` | `a + b` | GLSL has operator overloading; C does not |
| `vec3.dot(a, b)` | `vec3_dot(a, b)` | `dot(a, b)` | GLSL built-in compiles to a single GPU instruction |
| `vec3.reflect(I, N)` | `vec3_reflect(I, N)` | `reflect(I, N)` | All three use the same formula: `I - 2 * dot(I,N) * N` |
| `THREE.MathUtils.lerp(a, b, t)` | `vec3_lerp(a, b, t)` | `mix(a, b, t)` | GLSL calls it `mix`; same linear interpolation |
| Recursive `castRay(depth + 1)` | `cast_ray(ray, scene, depth + 1)` | `for` loop with `weight` accumulator | GLSL forbids recursion; weight tracks energy |
| `{ t, normal, material }` object return | `HitRecord *hit` pointer parameter | `out float t, out vec3 N, out Material mat` | GLSL uses `out` params; no pointers, no structs as return |
| `for (let y ...) for (let x ...)` render loop | `for (int j ...) for (int i ...)` in `render_scene` | `mainImage()` called per pixel by GPU | GPU implicit parallelism replaces explicit loops |
| `gl.shaderSource(shader, src)` | `static const char GLSL_RAYTRACER_SOURCE[]` | N/A (the shader IS the source) | We store it as a C string; Shadertoy compiles it |
| `canvas.toBlob()` / `URL.createObjectURL()` | `fputs(GLSL_RAYTRACER_SOURCE, f)` | N/A | Text file export; no binary encoding needed |
| Web Workers for parallelism | `pthread_create` | GPU hardware threads | GPU runs one thread per pixel; ~2M threads for 1080p |
