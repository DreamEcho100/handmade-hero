# Lesson 24 — GPU Scene Data Upload

> **What you'll build:** By the end of this lesson, the GPU rendering mode shows the complete scene -- 4 spheres, checkerboard floor, box, and 3 lights -- driven entirely by uniform data uploaded from the CPU-side `Scene` struct. Instead of the hardcoded L21 Shadertoy shader, a new data-driven GLSL body reads materials, spheres, lights, and boxes from uniform arrays. Feature toggles (V/F/B/R/T/H) now work in GPU mode. The CPU and GPU scenes visually match.

## Observable outcome

Press **N** twice to enter GPU mode. The scene now includes:

- 4 spheres with correct per-material ivory/rubber/mirror/glass shading
- Checkerboard floor (toggleable with F)
- Box geometry (toggleable with B)
- Hard shadows (toggleable with H)
- Reflections (toggleable with R)
- Refractions through the glass sphere (toggleable with T)

The HUD still shows GPU renderer info on line 5. Pressing V/F/B/R/T/H toggles features in real time -- the GPU shader reads the feature bitfield each frame.

## New concepts

- **gpu_scene_glsl.h** -- a complete data-driven GLSL raytracer body (~500 lines as a C string) that reads all geometry from uniforms instead of hardcoded constants
- **gpu_upload.h** -- `GpuSceneData` struct and `gpu_pack_scene()` function that flatten the CPU `Scene` into arrays matching the GLSL uniform layout
- **Feature bitfield** -- `uFeatures` int uniform with one bit per toggle (bit0=voxels, bit1=floor, bit2=boxes, etc.), updated every frame
- **Uniform array upload** -- `glUniform3fv`, `glUniform4fv`, `glUniform1fv`, `glUniform1iv` for bulk uploading flat float/int arrays as GLSL uniform arrays
- **Per-face ray-AABB intersection** in GLSL -- iterates 3 axes x 2 sides = 6 face planes, checks point-in-rectangle for each hit

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/gpu_scene_glsl.h` | Created | `GPU_SCENE_UNIFORMS` declarations + `GPU_SCENE_GLSL_SOURCE` body: material lookup, sphere/box intersection, scene_hit, cast_ray with feature toggles |
| `game/gpu_upload.h` | Created | `GpuSceneData` struct (flat arrays), `gpu_pack_features()`, `gpu_pack_scene()`, `gpu_scene_data_free()` |
| `game/gpu_shader.h` | Modified | `gpu_build_scene_fragment_source()` concatenates preamble + scene uniforms + scene GLSL body + epilogue |
| `platforms/x11/main.c` | Modified | 7 new GL function pointers, ~30 uniform locations, `gpu_upload_scene()`, texture unit binding in render loop |
| `platforms/raylib/main.c` | Modified | dlsym-loaded GL functions, same uniform upload pattern, texture binding to units 3-5 |

## Background -- from hardcoded to data-driven

### The problem with the L21/L23 GPU shader

The Shadertoy-compatible shader from L21 hardcodes everything: 4 spheres at fixed positions, 3 lights with fixed intensities, materials as GLSL `const` values. It cannot respond to feature toggles, cannot show boxes or voxels, and cannot be extended without editing the GLSL string. To make the GPU mode useful, we need the shader to read scene data at runtime.

### The uniform approach

OpenGL uniforms are the simplest data transfer mechanism from CPU to GPU. Each uniform is a named variable in the shader that the host sets before drawing. GLSL supports uniform arrays (`uniform vec4 uSphere[16]`), and OpenGL provides bulk-upload functions (`glUniform4fv`) that transfer entire arrays in one call.

For our scene: 5 materials (4 floats each for albedo, 3 for color, 1 each for specular exponent and IOR), 4 spheres (vec4: xyz + radius), 3 lights (vec4: xyz + intensity), and 1 box (center + half-size). This totals roughly 356 uniform floats -- well under the GL 3.3 minimum of 1024 vec4 uniforms per stage.

### JS analogy

In WebGL2, you'd call `gl.uniform4fv(location, new Float32Array([...]))` to upload an array of vec4 values. Three.js wraps this with `uniforms: { uSphere: { value: [...] } }` in a `ShaderMaterial`. Our C code is the equivalent of the raw WebGL path: get the location, pack the data into a flat array, call the upload function.

## Code walkthrough

### 1. gpu_upload.h -- flattening the Scene

The `GpuSceneData` struct mirrors the GLSL uniform layout with flat C arrays:

```c
typedef struct {
  int mat_count, sphere_count, light_count, box_count;
  int features;       /* bitfield from RenderSettings */

  float mat_color[16 * 3];    /* vec3 per material */
  float mat_albedo[16 * 4];   /* vec4 per material */
  float mat_spec[16];         /* float per material */
  float mat_ior[16];          /* float per material */

  float sphere[16 * 4];       /* vec4: xyz=center, w=radius */
  int   sphere_mat[16];       /* material index per sphere */

  float light[8 * 4];         /* vec4: xyz=pos, w=intensity */

  float box_center[8 * 3];    /* vec3 per box */
  float box_half[8 * 3];      /* vec3 per box */
  int   box_mat[8];           /* material index per box */
  /* ... voxels, meshes (covered in L25) ... */
} GpuSceneData;
```

`gpu_pack_scene()` iterates the CPU Scene struct and writes into these flat arrays:

```c
static inline void gpu_pack_scene(GpuSceneData *d, const Scene *s,
                                  const RenderSettings *r) {
  memset(d, 0, sizeof(*d));

  d->mat_count = s->material_count;
  for (int i = 0; i < s->material_count && i < 16; i++) {
    d->mat_color[i * 3 + 0] = s->materials[i].diffuse_color.x;
    d->mat_color[i * 3 + 1] = s->materials[i].diffuse_color.y;
    d->mat_color[i * 3 + 2] = s->materials[i].diffuse_color.z;
    d->mat_albedo[i * 4 + 0] = s->materials[i].albedo[0];
    /* ... etc ... */
  }
  /* Spheres, lights, boxes follow the same pattern */
  d->features = gpu_pack_features(r);
}
```

The feature bitfield packs toggle states into a single int:

```c
#define GPU_FEAT_VOXELS   (1 << 0)
#define GPU_FEAT_FLOOR    (1 << 1)
#define GPU_FEAT_BOXES    (1 << 2)
/* ... */

static inline int gpu_pack_features(const RenderSettings *r) {
  int f = 0;
  if (r->show_voxels)      f |= GPU_FEAT_VOXELS;
  if (r->show_floor)       f |= GPU_FEAT_FLOOR;
  if (r->show_boxes)       f |= GPU_FEAT_BOXES;
  if (r->show_reflections) f |= GPU_FEAT_REFLECT;
  if (r->show_refractions) f |= GPU_FEAT_REFRACT;
  if (r->show_shadows)     f |= GPU_FEAT_SHADOWS;
  if (r->show_envmap)      f |= GPU_FEAT_ENVMAP;
  return f;
}
```

### 2. gpu_scene_glsl.h -- the data-driven GLSL body

The GLSL uniform declarations are a separate macro so `gpu_build_scene_fragment_source()` can insert them between the preamble and the shader body:

```c
#define GPU_SCENE_UNIFORMS \
  "uniform int   uMatCount;\n" \
  "uniform vec3  uMatColor[16];\n" \
  "uniform vec4  uMatAlbedo[16];\n" \
  "uniform float uMatSpec[16];\n" \
  "uniform float uMatIOR[16];\n" \
  "uniform int   uSphereCount;\n" \
  "uniform vec4  uSphere[16];\n" \
  "uniform int   uSphereMat[16];\n" \
  "uniform int   uLightCount;\n" \
  "uniform vec4  uLight[8];\n" \
  "uniform int   uBoxCount;\n" \
  "uniform vec3  uBoxCenter[8];\n" \
  "uniform vec3  uBoxHalf[8];\n" \
  "uniform int   uBoxMat[8];\n" \
  "uniform int   uFeatures;\n"
```

The GLSL body uses helper functions to test feature bits:

```glsl
bool feat_floor()   { return (uFeatures & 0x02) != 0; }
bool feat_boxes()   { return (uFeatures & 0x04) != 0; }
bool feat_reflect() { return (uFeatures & 0x10) != 0; }
```

Material lookup reads from the uniform arrays:

```glsl
Material get_material(int idx) {
    return Material(uMatColor[idx], uMatAlbedo[idx],
                    uMatSpec[idx], uMatIOR[idx]);
}
```

### 3. Per-face ray-AABB intersection in GLSL

The CPU uses a slab test for AABB detection, but for boxes as visible geometry (with correct face normals), we need per-face intersection. The GLSL `box_hit` iterates 3 axes and 2 sides per axis:

```glsl
float box_hit(vec3 orig, vec3 dir, vec3 center, vec3 half_sz, out vec3 N) {
    float best_t = 1e10;
    for (int axis = 0; axis < 3; axis++) {
        if (abs(dir[axis]) < 1e-5) continue;
        for (int side = -1; side <= 1; side += 2) {
            float face = center[axis] + float(side) * half_sz[axis];
            float t = (face - orig[axis]) / dir[axis];
            if (t < 0.001 || t >= best_t) continue;
            vec3 pt = orig + dir * t;
            int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
            if (pt[a1] >= center[a1] - half_sz[a1] &&
                pt[a1] <= center[a1] + half_sz[a1] &&
                pt[a2] >= center[a2] - half_sz[a2] &&
                pt[a2] <= center[a2] + half_sz[a2]) {
                best_t = t;
                N = vec3(0.0);
                N[axis] = float(side);
            }
        }
    }
    return best_t < 1e9 ? best_t : -1.0;
}
```

This produces correct normals pointing outward from whichever face was hit -- essential for lighting and reflection.

### 4. Sphere intersection threshold fix

The L21 shader used `t0 < 0.0` to reject back-hits. The data-driven version uses `t0 < 0.001`, matching the CPU epsilon. Without this, rays originating at a surface (shadow rays, reflection rays) re-intersect the same sphere at `t ~ 0.0`:

```glsl
if (t0 < 0.001) t0 = t1;
if (t0 < 0.001) return -1.0;
```

### 5. Reflection origin offset

The CPU code offsets the reflection ray origin along or against the surface normal depending on which side of the surface the reflected ray exits. The GLSL version matches:

```glsl
orig = (dot(dir, N) < 0.0)
       ? pt - N * 0.001
       : pt + N * 0.001;
```

This prevents self-intersection artifacts (bright dots or black spots on reflective surfaces).

### 6. cast_ray -- exhausted flag for sky fallback

The iterative `cast_ray` loop needs to handle the case where all MAX_DEPTH bounces are used up (e.g., a ray bouncing between two mirror surfaces). The CPU's recursive version naturally returns the sky color at the terminal depth. The GPU version uses an `exhausted` flag:

```glsl
bool exhausted = true;
for (int depth = 0; depth < MAX_DEPTH; depth++) {
    if (!scene_hit(orig, dir, ...)) {
        color += weight * sky;
        exhausted = false;  /* normal exit: hit sky */
        break;
    }
    /* ... lighting, reflection ... */
    if (mat.albedo.z <= 0.0) {
        exhausted = false;  /* normal exit: non-reflective surface */
        break;
    }
}
if (exhausted && weight > 0.001)
    color += weight * sky;  /* loop ran out: add remaining energy as sky */
```

### 7. X11 backend -- uniform upload

Seven new GL function pointers handle array uploads:

```c
LOAD_GL(Uniform1i);  LOAD_GL(Uniform1iv);  LOAD_GL(Uniform1fv);
LOAD_GL(Uniform3fv); LOAD_GL(Uniform4fv);  LOAD_GL(Uniform1uiv);
LOAD_GL(ActiveTexture);
```

The `gpu_upload_scene()` function is called once at startup. It packs the scene, binds the shader program, and uploads all uniform arrays:

```c
GpuSceneData d;
gpu_pack_scene(&d, scene, settings);
g_gpu.UseProgram(g_gpu.program);

g_gpu.Uniform1i(g_gpu.u_mat_count, d.mat_count);
g_gpu.Uniform3fv(g_gpu.u_mat_color,  d.mat_count, d.mat_color);
g_gpu.Uniform4fv(g_gpu.u_mat_albedo, d.mat_count, d.mat_albedo);
g_gpu.Uniform1fv(g_gpu.u_mat_spec,   d.mat_count, d.mat_spec);
g_gpu.Uniform1fv(g_gpu.u_mat_ior,    d.mat_count, d.mat_ior);
/* ... spheres, lights, boxes follow the same pattern ... */
```

Feature toggles and envmap mode are the only per-frame uniforms:

```c
/* In gpu_render_scene(), called every frame: */
g_gpu.Uniform1i(g_gpu.u_features, gpu_pack_features(settings));
```

### 8. gpu_build_scene_fragment_source -- assembly

The new build function in `gpu_shader.h` concatenates four pieces:

```c
static inline int gpu_build_scene_fragment_source(char *buf, int bufsize) {
  buf[0] = '\0';
  strcat(buf, GPU_FRAG_PREAMBLE);     /* #version, camera uniforms */
  strcat(buf, GPU_SCENE_UNIFORMS);    /* scene uniform declarations */
  strcat(buf, GPU_SCENE_GLSL_SOURCE); /* data-driven raytracer body */
  strcat(buf, GPU_FRAG_EPILOGUE);     /* main() with camera ray */
  return 0;
}
```

The L21 `gpu_build_fragment_source()` (which uses the hardcoded `GLSL_RAYTRACER_SOURCE`) is preserved for Shadertoy export. The backends now call `gpu_build_scene_fragment_source()` for in-process rendering.

## Key takeaways

1. **Uniform arrays are the simplest CPU-to-GPU data channel** -- no buffer objects, no SSBOs, no compute shaders. Just `glUniform*v` calls with flat float arrays. For small scenes (< 1024 uniforms), this is sufficient and portable across all GL 3.3 implementations
2. **The `GpuSceneData` struct is a serialization format** -- it flattens nested C structs (Scene -> Sphere -> Vec3) into contiguous arrays matching the GLSL layout. This separation keeps the game layer GPU-agnostic
3. **Feature toggles via bitfield** allow per-frame control without recompiling the shader. The GLSL compiler cannot optimize away the branches (the uniform value is unknown at compile time), but the cost is negligible for our scene complexity
4. **The scene upload is one-time** for static geometry. Only `uFeatures` and `uEnvMode` change per-frame. If the scene were dynamic, you would re-upload the changed uniforms each frame
5. **Epsilon consistency matters** -- the GPU and CPU must use the same intersection thresholds (0.001) and origin offset strategy, otherwise visual artifacts appear only in one mode
