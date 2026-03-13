# Lesson 25 — GPU Voxels & Triangle Meshes

> **What you'll build:** By the end of this lesson, the GPU rendering mode shows the full scene including voxel models (the bunny) and triangle meshes (the icosahedron) -- the last two geometry types that were missing from GPU mode. Voxel bitfields are uploaded as uniform uint arrays and tested per-bit in GLSL. Mesh triangle data is packed into an RGBA32F 1D texture and read via `texelFetch`-style sampling. Pressing V and M toggles voxels and meshes in GPU mode, matching the CPU behavior exactly.

## Observable outcome

Press **N** twice to enter GPU mode. The voxel bunny and icosahedron mesh now appear alongside the spheres, floor, and box. Press **V** to toggle voxels off/on -- the bunny disappears and reappears. Press **M** to toggle meshes -- the icosahedron disappears and reappears. All geometry types render with correct lighting, shadows, and reflections in GPU mode. The scene visually matches the CPU modes.

## New concepts

- **Voxel bitfield as uniform uint array** -- 5 uint32 values per voxel model encode which of the 144 cells (6x6x4 grid) are solid, uploaded via `glUniform1uiv`
- **On-the-fly cell center computation** in GLSL -- the GPU computes each cell's world position from the model position, scale, and grid index, avoiding pre-computed arrays
- **Integer hash function for voxel color** -- a deterministic hash (0x45d9f3b multiplier) maps cell IDs to RGB colors in the [0.2, 0.8] range, ported from C to GLSL
- **RGBA32F 1D texture for mesh data** -- triangle vertices and normals packed as 6 texels per triangle, sampled by index in the fragment shader
- **Moller-Trumbore intersection** in GLSL -- the same algorithm from L19, with barycentric normal interpolation for smooth shading
- **Texture unit management** -- Raylib internally binds textures to units 0-2, so custom textures must use units 3+ to avoid conflicts

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/gpu_scene_glsl.h` | Modified | Added `voxel_solid()`, `voxel_color_from_id()`, `voxel_model_hit()`, `read_tri_vec3()`, `tri_hit()`, `mesh_hit()` GLSL functions; voxel/mesh branches in `scene_hit()` |
| `game/gpu_upload.h` | Modified | Voxel fields (`vox_pos`, `vox_scale`, `vox_bitfield`, etc.) and mesh fields (`mesh_pos`, `mesh_tri_count`, `mesh_data_offset`, `tri_tex_data`) in `GpuSceneData`; packing logic in `gpu_pack_scene()` |
| `platforms/x11/main.c` | Modified | Voxel + mesh uniform locations, RGBA32F texture creation for mesh triangles, texture unit binding (unit 0 = mesh tri tex) |
| `platforms/raylib/main.c` | Modified | Same uniform + texture upload; textures bound to units 3-5 to avoid Raylib's internal bindings |

## Background -- why these geometry types need special handling

### Voxels: bitfield, not geometry

On the CPU, each voxel model stores a flat array of `VoxelCell` structs with pre-computed centers. Uploading 60+ cell positions as uniforms would be wasteful and would hit uniform limits with multiple models. Instead, we upload only the bitfield (which cells are solid) and the model's position/scale. The GPU recomputes cell centers on-the-fly from grid indices -- GPUs are fast at arithmetic, and recomputation is cheaper than the memory transfer.

The bunny's 144 cells (6x6x4) fit in 5 uint32 values (144 bits / 32 = 4.5, rounded up to 5). With 4 models maximum, that is 20 uniforms total.

### Meshes: texture, not uniforms

Triangle meshes have variable amounts of data per model. The icosahedron has 20 triangles, each with 3 vertices + 3 normals = 6 vec3 values = 18 floats per triangle. Uniform arrays could work for small meshes, but texture sampling scales better and avoids the uniform count limit.

We pack triangle data into an RGBA32F 1D texture: 6 texels per triangle (v0, v1, v2, n0, n1, n2), each texel storing xyz in the RGB channels (A unused). The shader reads texels by index using a computed UV coordinate.

### JS analogy

In WebGL2, `gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, width, 1, 0, gl.RGBA, gl.FLOAT, data)` creates a 1D data texture. Three.js provides `DataTexture(data, width, 1, RGBAFormat, FloatType)` for the same purpose. Both approaches use textures as general-purpose data storage -- the same trick we use here for mesh triangles.

For voxel bitfields, WebGL2's `gl.uniform1uiv(location, uint32Array)` maps directly to our `glUniform1uiv` call.

## Code walkthrough

### 1. Voxel bitfield upload

In `gpu_pack_scene()`, the bunny bitfield is copied into the flat array:

```c
d->voxel_count = s->voxel_model_count;
for (int i = 0; i < s->voxel_model_count && i < 4; i++) {
  const VoxelModel *vm = &s->voxel_models[i];
  d->vox_pos[i * 3 + 0] = vm->position.x;
  d->vox_pos[i * 3 + 1] = vm->position.y;
  d->vox_pos[i * 3 + 2] = vm->position.z;
  d->vox_scale[i] = vm->scale;
  d->vox_bbox_center[i * 3 + 0] = vm->bbox_center.x;
  /* ... bbox half-size, material index ... */
  for (int w = 0; w < 5; w++)
    d->vox_bitfield[i * 5 + w] = BUNNY_BITFIELD[w];
}
```

The X11 backend uploads with `glUniform1uiv`:

```c
g_gpu.Uniform1uiv(g_gpu.u_vox_bitfield, d.voxel_count * 5, d.vox_bitfield);
```

### 2. GLSL voxel_solid -- bit testing

The GLSL function tests a single bit in the uniform uint array:

```glsl
bool voxel_solid(int model_idx, int cell_id) {
    uint word = uVoxBitfield[model_idx * 5 + cell_id / 32];
    return (word & (1u << uint(cell_id & 31))) != 0u;
}
```

`cell_id / 32` selects the uint32 word, `cell_id & 31` selects the bit within that word. The `1u` and `uint()` casts are required in GLSL -- mixing signed and unsigned in bitwise operations is a compile error on some drivers.

### 3. GLSL voxel_color_from_id -- deterministic color hash

The C voxel renderer assigns each cell a deterministic color based on its grid index. The GLSL version ports the same integer hash:

```glsl
vec3 voxel_color_from_id(int cell_id) {
    uint h = uint(cell_id);
    h = ((h >> 16u) ^ h) * 0x45d9f3bu;
    h = ((h >> 16u) ^ h) * 0x45d9f3bu;
    h = (h >> 16u) ^ h;
    float r = float((h >> 0u) & 0xFFu) / 255.0 * 0.6 + 0.2;
    float g = float((h >> 8u) & 0xFFu) / 255.0 * 0.6 + 0.2;
    float b = float((h >> 16u) & 0xFFu) / 255.0 * 0.6 + 0.2;
    return vec3(r, g, b);
}
```

The `* 0.6 + 0.2` maps each channel to the [0.2, 0.8] range, avoiding pure black or pure white cells. The 0x45d9f3b multiplier is a well-known integer hash constant that distributes bits well.

### 4. GLSL voxel_model_hit -- on-the-fly cell iteration

The key insight: instead of uploading pre-computed cell centers, the GPU computes each cell's world position from a formula:

```glsl
bool voxel_model_hit(vec3 orig, vec3 dir, int mi,
                     out float best_t, out vec3 best_N, out vec3 voxColor) {
    best_t = 1e10;
    if (!aabb_test(orig, dir, uVoxBBoxCenter[mi], uVoxBBoxHalf[mi]))
        return false;

    float vox_size = VOXEL_SIZE * uVoxScale[mi];
    float half_cell = vox_size * 0.5;
    vec3 hs = vec3(half_cell);
    bool found = false;

    for (int k = 0; k < VOX_D; k++) {
        for (int j = 0; j < VOX_H; j++) {
            for (int i = 0; i < VOX_W; i++) {
                int cell_id = i + j * VOX_W + k * VOX_W * VOX_H;
                if (!voxel_solid(mi, cell_id)) continue;

                vec3 cell_center = uVoxPos[mi] + vec3(
                    (float(i) - 3.0 + 0.5) * vox_size,
                    (float(j) - 3.0 + 0.5) * vox_size,
                    (-float(k) + 2.0 - 0.5) * vox_size);

                vec3 cn;
                float ct = box_hit(orig, dir, cell_center, hs, cn);
                if (ct > 0.0 && ct < best_t) {
                    best_t = ct;
                    best_N = cn;
                    voxColor = voxel_color_from_id(cell_id);
                    found = true;
                }
            }
        }
    }
    return found;
}
```

The cell center formula mirrors the CPU code: `(i - 3 + 0.5) * vox_size` centers the grid at the origin (offsets -2.5 to +2.5 for a 6-wide grid). The Z axis is flipped (`-k + 2 - 0.5`) to match the CPU's front-to-back ordering.

The AABB early-out (`aabb_test`) is critical -- without it, every ray would iterate all 144 cells even when the model is nowhere near the ray.

### 5. Mesh triangle texture creation

`gpu_pack_scene()` allocates and fills an RGBA32F pixel array:

```c
d->tri_tex_total = total_tris * 6;  /* 6 texels per triangle */
d->tri_tex_data = malloc(d->tri_tex_total * 4 * sizeof(float));

int texel = 0;
for (int ti = 0; ti < m->tri_count; ti++) {
  const Triangle *tr = &m->triangles[ti];
  float *p = d->tri_tex_data + texel * 4;
  /* Texel 0-2: vertex positions (xyz in RGB, A=0) */
  p[0] = tr->v0.x; p[1] = tr->v0.y; p[2] = tr->v0.z; p[3] = 0.0f;
  p += 4;
  p[0] = tr->v1.x; p[1] = tr->v1.y; p[2] = tr->v1.z; p[3] = 0.0f;
  /* ... v2, n0, n1, n2 ... */
  texel += 6;
}
```

The X11 backend uploads this as a GL texture:

```c
glGenTextures(1, &g_gpu.tex_mesh_tri);
glBindTexture(GL_TEXTURE_2D, g_gpu.tex_mesh_tri);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
             d.tri_tex_width, 1, 0, GL_RGBA, GL_FLOAT, d.tri_tex_data);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
```

NEAREST filtering is essential -- we are reading exact data values, not interpolating between pixels. GL_LINEAR would blend adjacent triangle data, producing garbage.

For the icosahedron: 20 triangles x 6 texels = 120 texels, each RGBA32F = 16 bytes. Total: 1920 bytes -- trivial.

### 6. GLSL read_tri_vec3 -- sampling the data texture

```glsl
vec3 read_tri_vec3(int texel_idx) {
    float u = (float(texel_idx) + 0.5) / float(textureSize(uMeshTriTex, 0).x);
    return texture(uMeshTriTex, vec2(u, 0.5)).xyz;
}
```

The `+ 0.5` centers the sample within the texel (avoids sampling on the boundary between adjacent texels). `textureSize` returns the texture width at mip level 0.

### 7. GLSL tri_hit -- Moller-Trumbore with smooth normals

```glsl
float tri_hit(vec3 orig, vec3 dir,
              vec3 v0, vec3 v1, vec3 v2,
              vec3 n0, vec3 n1, vec3 n2, out vec3 N) {
    vec3 e1 = v1 - v0, e2 = v2 - v0;
    vec3 h = cross(dir, e2);
    float a = dot(e1, h);
    if (abs(a) < 1e-7) return -1.0;
    float f = 1.0 / a;
    vec3 s = orig - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return -1.0;
    vec3 q = cross(s, e1);
    float v = f * dot(dir, q);
    if (v < 0.0 || u + v > 1.0) return -1.0;
    float t = f * dot(e2, q);
    if (t < 0.001) return -1.0;
    float w = 1.0 - u - v;
    N = normalize(n0 * w + n1 * u + n2 * v);  /* barycentric interpolation */
    return t;
}
```

This is identical to the CPU implementation from L19. The barycentric coordinates `(w, u, v)` weight the per-vertex normals to produce smooth shading across the triangle face.

### 8. GLSL mesh_hit -- AABB early-out + triangle loop

```glsl
bool mesh_hit(vec3 orig, vec3 dir, int mi,
              out float best_t, out vec3 best_N) {
    best_t = 1e10;
    if (!aabb_test(orig, dir, uMeshBBoxCenter[mi], uMeshBBoxHalf[mi]))
        return false;
    bool found = false;
    int off = uMeshDataOff[mi];
    for (int ti = 0; ti < uMeshTriCount[mi]; ti++) {
        int base = off + ti * 6;
        vec3 v0 = read_tri_vec3(base + 0);
        vec3 v1 = read_tri_vec3(base + 1);
        vec3 v2 = read_tri_vec3(base + 2);
        vec3 n0 = read_tri_vec3(base + 3);
        vec3 n1 = read_tri_vec3(base + 4);
        vec3 n2 = read_tri_vec3(base + 5);
        vec3 tn;
        float tt = tri_hit(orig, dir, v0,v1,v2, n0,n1,n2, tn);
        if (tt > 0.0 && tt < best_t) {
            best_t = tt;
            best_N = tn;
            found = true;
        }
    }
    return found;
}
```

The `uMeshDataOff[mi]` offset lets multiple meshes share a single texture -- each mesh's triangles start at a different texel offset.

### 9. Raylib texture unit workaround

Raylib's `DrawRectangle` (used with `BeginShaderMode`) internally calls `rlSetTexture` which binds a white 1x1 texture to unit 0. If our mesh triangle texture is also on unit 0, Raylib overwrites it every frame. The solution: bind custom textures to units 3-5:

```c
g_gl.ActiveTexture(0x84C3); /* GL_TEXTURE3 */
glBindTexture(GL_TEXTURE_2D, g_rl_gpu.tex_mesh_tri);
/* Tell the shader that uMeshTriTex is on unit 3 */
SetShaderValue(shader, u_mesh_tri_tex, &(int){3}, SHADER_UNIFORM_INT);
```

The X11 backend does not have this problem because it uses a raw `glDrawArrays` call with no framework interference.

## Key takeaways

1. **Recomputation beats data transfer for regular grids** -- the GPU computes 144 cell centers from position + scale + grid index faster than uploading 144 vec3 uniforms. This principle applies broadly: if data has a formula, compute it on the GPU instead of transferring it
2. **Textures are general-purpose GPU memory** -- RGBA32F textures store arbitrary float data, not just images. The mesh triangle texture is a lookup table indexed by integer, with NEAREST filtering to avoid interpolation artifacts
3. **Bitfield packing is highly uniform-efficient** -- 144 cells in 5 uint32 values (20 bytes) instead of 144 material indices (576 bytes). The GPU's bitwise operators (`&`, `>>`, `<<`) test individual bits at negligible cost
4. **Texture unit conflicts are a common Raylib pitfall** -- any framework that internally binds textures can silently overwrite your custom bindings. Always use higher-numbered units for custom data textures when mixing with framework drawing calls
5. **AABB early-out is essential for brute-force loops** -- without the bounding box test, every ray would iterate all 144 voxel cells and all 20 mesh triangles, making the GPU shader significantly slower
