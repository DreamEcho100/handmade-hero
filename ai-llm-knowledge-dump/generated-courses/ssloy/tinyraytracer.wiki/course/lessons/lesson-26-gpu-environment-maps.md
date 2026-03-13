# Lesson 26 — GPU Environment Maps

> **What you'll build:** By the end of this lesson, the GPU rendering mode supports all three environment map modes -- procedural sky gradient, equirectangular HDR image, and cubemap. Pressing **E** toggles envmaps on/off and pressing **C** cycles between modes, all in GPU mode. Reflective surfaces (mirror sphere, glass refractions) sample the environment map for background color, matching the CPU rendering exactly. Two new GL textures are uploaded: a 2D texture for the equirectangular map and a cubemap texture for the 6-face cubemap.

## Observable outcome

In GPU mode, press **E** to toggle the environment map on/off. When on, press **C** to cycle through:

1. **Procedural** (mode 0) -- 3-color vertical gradient (brown ground, grey horizon, blue zenith)
2. **Equirectangular** (mode 1) -- the HDR panorama wrapped as a sphere around the scene
3. **Cubemap** (mode 2) -- the 6-face cubemap with GL hardware face selection

The mirror sphere reflects the environment. The glass sphere refracts through to the environment behind it. The checkerboard floor shows environment-tinted reflections. All three modes match the CPU rendering, and the mode switches are instant (no shader recompilation -- just a uniform change).

## New concepts

- **Equirectangular texture upload** -- a single 2D texture (RGB8, GL_LINEAR, GL_REPEAT for S, GL_CLAMP_TO_EDGE for T) storing the full 360-degree panorama
- **Cubemap texture upload** -- a `GL_TEXTURE_CUBE_MAP` with 6 faces (RGB8, GL_LINEAR, GL_CLAMP_TO_EDGE for all axes), uploaded in the order +X, -X, +Y, -Y, +Z, -Z
- **GLSL equirectangular sampling** -- `atan(dz, dx)` and `asin(dy)` convert a ray direction to UV coordinates for 2D texture lookup
- **GLSL cubemap sampling** -- `texture(uEnvCube, dir)` lets the GPU hardware select the correct face and interpolate
- **Per-frame envmap mode uniform** -- `uEnvMode` (0/1/2) is uploaded every frame since the C key cycles it at runtime
- **Envmap in refraction miss** -- when a refracted ray exits the scene, it samples the environment map instead of returning black

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/gpu_scene_glsl.h` | Modified | Added `envmap_procedural()`, `envmap_equirect()`, `envmap_cubemap()`, `envmap_sample()` GLSL functions; envmap sampling in `cast_ray()` for background + refraction miss |
| `game/gpu_upload.h` | Modified | `env_mode` field in `GpuSceneData`; `gpu_pack_scene()` copies `scene->envmap.mode` |
| `platforms/x11/main.c` | Modified | `tex_env_equirect` and `tex_env_cube` texture creation in `gpu_upload_scene()`; texture unit binding (unit 1 = equirect, unit 2 = cubemap); per-frame `uEnvMode` upload |
| `platforms/raylib/main.c` | Modified | Same texture creation + binding on units 4 and 5; per-frame envmap mode upload |

## Background -- environment maps on the GPU

### How the CPU does it

The CPU envmap system (L18/L20) converts a ray direction to a texture coordinate and samples the image. For equirectangular maps, this uses `atan2(dz, dx)` and `asin(dy)`. For cubemaps, it finds the dominant axis, selects the face, and computes 2D coordinates within that face.

### Why the GPU version is simpler

GLSL has built-in `atan(y, x)` and `asin(x)` functions that compile to single GPU instructions. For cubemaps, `texture(samplerCube, dir)` handles face selection, coordinate mapping, and bilinear interpolation entirely in hardware -- the 30+ lines of CPU cubemap sampling code reduce to a single GLSL function call.

### JS analogy

In Three.js, `scene.background = new THREE.CubeTextureLoader().load(urls)` sets a cubemap background, and `material.envMap = cubeTexture` enables reflections. Internally, Three.js creates a `GL_TEXTURE_CUBE_MAP` and samples it with `textureCube(envMap, reflectDir)` -- exactly what our GLSL does. For equirectangular maps, Three.js uses `PMREMGenerator` to convert to cubemap first; our shader samples the equirect texture directly via `atan`/`asin` math.

## Code walkthrough

### 1. Equirectangular texture upload

In `gpu_upload_scene()`, the equirect panorama is uploaded as a standard 2D texture:

```c
const EnvMap *em = &scene->envmap;
if (em->pixels && em->width > 0 && em->height > 0) {
  glGenTextures(1, &g_gpu.tex_env_equirect);
  glBindTexture(GL_TEXTURE_2D, g_gpu.tex_env_equirect);
  GLenum fmt = (em->channels == 4) ? GL_RGBA : GL_RGB;
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, em->width, em->height,
               0, fmt, GL_UNSIGNED_BYTE, em->pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
```

**GL_REPEAT for S (horizontal):** The equirectangular map wraps around 360 degrees horizontally. When the `atan` calculation produces `u = 0.0` or `u = 1.0`, both should map to the same column. GL_REPEAT handles this seamlessly.

**GL_CLAMP_TO_EDGE for T (vertical):** The map does not wrap vertically -- the north pole (top) and south pole (bottom) are singular points. Clamping prevents sampling artifacts at the poles.

**GL_LINEAR:** Bilinear filtering smooths the environment when viewed at oblique angles. Without it, reflections on curved surfaces would show visible texel boundaries.

### 2. Cubemap texture upload

The 6 cubemap faces are uploaded to a single `GL_TEXTURE_CUBE_MAP` object:

```c
if (em->faces[0].pixels) {
  glGenTextures(1, &g_gpu.tex_env_cube);
  glBindTexture(GL_TEXTURE_CUBE_MAP, g_gpu.tex_env_cube);
  for (int f = 0; f < 6; f++) {
    const CubeMapFace *face = &em->faces[f];
    if (!face->pixels) continue;
    GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)f;
    GLenum cfmt = (face->channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(target, 0, GL_RGB, face->width, face->height,
                 0, cfmt, GL_UNSIGNED_BYTE, face->pixels);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}
```

The face order is defined by the GL spec: `GL_TEXTURE_CUBE_MAP_POSITIVE_X + i` for `i = 0..5` gives +X, -X, +Y, -Y, +Z, -Z. Our `em->faces[]` array must match this order. All three wrap modes (S, T, R) are clamped -- cubemap edges should blend smoothly between adjacent faces, which GL_LINEAR handles via cross-face interpolation.

### 3. GLSL envmap_equirect -- direction to UV

```glsl
vec3 envmap_equirect(vec3 dir) {
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * PI);
    float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI;
    return texture(uEnvEquirect, vec2(u, v)).rgb;
}
```

The math: `atan(z, x)` returns the azimuthal angle in [-pi, pi]. Dividing by 2pi and adding 0.5 maps to [0, 1]. `asin(y)` returns the elevation in [-pi/2, pi/2]. Dividing by pi and subtracting from 0.5 maps to [0, 1] with the top of the image at the north pole.

The `clamp(dir.y, -1.0, 1.0)` guard prevents `asin` from receiving values outside its domain due to floating-point imprecision -- `asin(1.0001)` is undefined and produces NaN on some GPU drivers.

### 4. GLSL envmap_cubemap -- hardware face selection

```glsl
vec3 envmap_cubemap(vec3 dir) {
    return texture(uEnvCube, dir).rgb;
}
```

This single line replaces the CPU's 30+ lines of face selection code. The GPU hardware determines which face to sample based on the largest component of `dir`, computes the 2D coordinate within that face, and performs bilinear interpolation -- all in one instruction.

### 5. GLSL envmap_procedural -- gradient fallback

```glsl
vec3 envmap_procedural(vec3 dir) {
    float t = 0.5 * (dir.y + 1.0);
    vec3 ground  = vec3(0.35, 0.25, 0.15);
    vec3 horizon = vec3(0.85, 0.85, 0.80);
    vec3 zenith  = vec3(0.15, 0.35, 0.65);
    if (t < 0.5) return mix(ground, horizon, t * 2.0);
    return mix(horizon, zenith, (t - 0.5) * 2.0);
}
```

This 3-color gradient maps `dir.y` (vertical component, range [-1, 1]) to a ground-horizon-zenith blend. It is used when no image envmap is loaded or when `uEnvMode == 0`.

### 6. GLSL envmap_sample -- mode dispatch

```glsl
vec3 envmap_sample(vec3 dir) {
    if (uEnvMode == 1) return envmap_equirect(dir);
    if (uEnvMode == 2) return envmap_cubemap(dir);
    return envmap_procedural(dir);
}
```

The `uEnvMode` uniform is set per-frame by the host, allowing the C key to cycle modes without shader recompilation. The branches introduce minor divergence on the GPU (all pixels execute all paths, masking results), but the cost is negligible since environment sampling only happens for background rays and refraction misses.

### 7. Envmap in cast_ray -- background and refraction miss

The environment map is sampled in two places within `cast_ray`:

**Background (ray misses all geometry):**

```glsl
if (!scene_hit(orig, dir, t, N, mat, voxColor)) {
    if (feat_envmap())
        color += weight * envmap_sample(dir);
    else {
        float sky = 0.5 * (dir.y + 1.0);
        color += weight * mix(BG_TOP, BG_BOT, sky * 0.3);
    }
    exhausted = false;
    break;
}
```

**Refraction miss (refracted ray exits the scene):**

```glsl
if (feat_refract() && mat.albedo.w > 0.0) {
    /* ... compute refracted ray ... */
    if (!scene_hit(rOrig, rdir, rt, rN, rM, rVC)) {
        if (feat_envmap())
            color += weight * mat.albedo.w * envmap_sample(rdir);
        else {
            float sky = 0.5 * (rdir.y + 1.0);
            color += weight * mat.albedo.w * mix(BG_TOP, BG_BOT, sky * 0.3);
        }
    }
}
```

Without the refraction-miss sampling, the glass sphere would show black when looking through it at the sky -- the refracted ray would exit the scene and contribute nothing. With envmap sampling, the glass sphere acts as a lens that bends the environment image, matching the CPU behavior.

### 8. Per-frame uniform upload

Both backends upload `uEnvMode` every frame since the C key can change it at any time:

```c
/* In gpu_render_scene(), called every frame: */
g_gpu.Uniform1i(g_gpu.u_env_mode, (int)scene->envmap.mode);
```

The `uFeatures` bitfield (which includes `GPU_FEAT_ENVMAP` for the E key toggle) is also uploaded per-frame. Together, these two uniforms give the GPU shader full runtime control over environment mapping without shader recompilation.

### 9. Texture unit binding in the render loop

The X11 backend binds all three textures before drawing:

```c
g_gpu.ActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, g_gpu.tex_mesh_tri);
g_gpu.Uniform1i(g_gpu.u_mesh_tri_tex, 0);

g_gpu.ActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, g_gpu.tex_env_equirect);
g_gpu.Uniform1i(g_gpu.u_env_equirect, 1);

g_gpu.ActiveTexture(GL_TEXTURE2);
glBindTexture(GL_TEXTURE_CUBE_MAP, g_gpu.tex_env_cube);
g_gpu.Uniform1i(g_gpu.u_env_cube, 2);
```

Each texture gets a dedicated unit (0, 1, 2). The `Uniform1i` calls tell the shader which sampler maps to which unit. The Raylib backend uses units 3, 4, and 5 instead (to avoid conflicts with Raylib's internal texture bindings on units 0-2, as discussed in L25).

## Key takeaways

1. **Cubemap sampling is trivially simple on the GPU** -- `texture(samplerCube, dir)` replaces 30+ lines of CPU face-selection code. This is one of the strongest examples of GPU hardware doing complex work in a single instruction
2. **Equirectangular sampling requires careful wrap modes** -- GL_REPEAT horizontally for seamless 360-degree wrap, GL_CLAMP_TO_EDGE vertically to avoid pole artifacts. Getting these wrong produces visible seams in reflections
3. **Per-frame uniforms enable runtime mode switching** -- `uEnvMode` changes instantly via the C key without shader recompilation. This is a general pattern: use uniforms for anything that changes at runtime, recompile only for structural shader changes
4. **Envmap must be sampled at both background hits and refraction misses** -- omitting either case produces black areas where the CPU version shows the environment. The glass sphere is the most visible test case
5. **Three texture types coexist** in one shader: RGBA32F data texture (mesh triangles, NEAREST), RGB8 2D texture (equirect envmap, LINEAR), and RGB8 cubemap (6-face envmap, LINEAR). Each serves a different purpose and requires different filter/wrap settings
