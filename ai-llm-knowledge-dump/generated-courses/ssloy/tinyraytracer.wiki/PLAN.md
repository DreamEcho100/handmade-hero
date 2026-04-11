# Building a Raytracer in C — PLAN.md

> **Course-builder principles:** `ai-llm-knowledge-dump/prompt/course-builder.md`
> **Platform template source:** `ai-llm-knowledge-dump/generated-courses/platform-backend/course/src/`
> **Reference algorithms:** `ai-llm-knowledge-dump/generated-courses/ssloy/tinyraytracer.wiki/sources/Part-1:-understandable-raytracing.md` (ssloy's 10-step tutorial)
> **Stereo rendering reference:** `ai-llm-knowledge-dump/generated-courses/ssloy/tinyraytracer.wiki/sources/Part-2:-low-budget-stereo-rendering.md` (anaglyph, stereoscope, autostereogram)
> **GPU raytracing reference:** `ai-llm-knowledge-dump/generated-courses/ssloy/tinyraytracer.wiki/sources/Part-3:-shadertoy.md` (ray-box intersection, voxel rendering)
> **C code standards:** `ai-llm-knowledge-dump/modern-c-programming-safe,-performant,-and-practical-practices.md`
> **Purpose:** Teach CPU raytracing fundamentals by porting ssloy's "Understandable Raytracing in 256 lines of bare C++" to modern C, rendered in real time via the platform backbuffer. Extended with ray-box intersection (Part 3), anaglyph stereo, and autostereogram generation (Part 2).

---

## What This Course Builds

A real-time CPU raytracer rendered to the platform backbuffer at 60fps. The final scene contains 4 spheres (ivory, red rubber, mirror, glass), a cube, a checkerboard floor, and 3 point lights. The renderer implements the full Phong reflection model (ambient + diffuse + specular), hard shadows, recursive reflections (depth 4), and refractions (Snell's law). Arrow keys orbit the camera around the scene. Export keys: P exports a standard PPM snapshot, A exports an anaglyph stereo image (red/cyan 3D glasses), G exports a random-dot autostereogram. Both X11 and Raylib backends are supported.

**What this course is NOT:**

- Not an offline renderer — the raytracer writes to the platform backbuffer each frame, not to a file
- Not a rasterizer — every pixel is computed by tracing rays, not by projecting triangles
- Not a platform course — platform infrastructure is copied from the Platform Foundation Course and adapted; it is not re-taught

**Source material:** ssloy's wiki covers three parts:

- **Part 1** — 10 algorithmic steps (gradient → sphere intersection → multiple spheres → diffuse → specular → shadows → reflections → refractions → checkerboard → assignments). This course covers Steps 1–9 across Lessons 01–12, adding interactive camera and HUD/PPM export. Step 10 assignments are covered in Lessons 18 (environment maps) and 19 (triangle meshes).
- **Part 2** — Stereo rendering: anaglyph (red/cyan), stereoscope (side-by-side), autostereogram (random-dot SIRDS). This course covers anaglyph and side-by-side in Lesson 14, and autostereogram (wall-eyed + cross-eyed) in Lesson 15. Barrel distortion correction is out of scope.
- **Part 3** — GPU raytracing via GLSL/Shadertoy. The **ray-box intersection algorithm** is ported to C in Lesson 13 and voxel rendering in Lesson 16. The complete GLSL Shadertoy shader is provided in Lesson 21 as a C string export (Shift+L writes `raytracer.glsl`). Cube map textures are covered in Lesson 20.

**C++ to Modern C upgrades:**

| C++ feature | Modern C replacement | Why |
|---|---|---|
| Operator overloading (`+`, `-`, `*`) | Explicit `vec3_add`/`vec3_sub`/`vec3_scale`/`vec3_dot` | C has no operator overloading; explicit names are clearer |
| `Vec3f` class with constructors | `typedef struct { float x, y, z; } Vec3` + `vec3_make(x,y,z)` | C structs have no constructors; compound literals or factory functions |
| `std::vector<Sphere>` | Fixed-size array `Sphere spheres[MAX_SPHERES]` + `int sphere_count` | No dynamic allocation needed; scene is static |
| `std::ofstream` | `fopen`/`fwrite`/`fclose` | C standard I/O; no C++ streams |
| `std::numeric_limits<float>::max()` | `FLT_MAX` from `<float.h>` | C standard constant |
| `std::max`/`std::min` | `fmaxf`/`fminf` or `MAX`/`MIN` macros | C standard math or platform macros |
| Offline render → save to file | Real-time backbuffer + optional PPM export | Interactive experience; platform integration |
| Fixed camera at origin | Interactive arrow-key camera orbit | Demonstrates GameInput integration |
| No stereo output | Anaglyph PPM export + autostereogram PPM export | Extends beyond ssloy's file-only stereo to interactive scene |
| GLSL fragment shader | Per-face ray-box intersection in C | Same algorithm, different language; box intersection is geometry, not GPU-specific |

---

## Final Observable State

At the end of Lesson 23, the student has:

- A window rendering a raytraced scene at interactive frame rates
- 4 spheres: ivory (diffuse+specular), red rubber (diffuse), mirror (reflective), glass (refractive)
- 1 cube: axis-aligned box with Phong shading, shadows, and reflections (Part 3 geometry)
- 1 voxel bunny: 6×6×4 bitfield-packed model with per-voxel palette colors (Part 3 Steps 8-9)
- 1 icosahedron: procedural triangle mesh with smooth-shaded normals (or any .obj file)
- 3 point lights creating realistic Phong shading with hard shadows
- A checkerboard floor plane with procedural pattern
- Environment mapping: procedural sky, equirectangular panorama (stb_image), or cube map (6 faces)
- Recursive reflections (mirror sphere reflects the scene) up to depth 3
- Refractions through the glass sphere (Snell's law)
- Mouse orbit/pan/scroll + arrow keys + WASD camera controls
- Multi-threaded rendering via pthreads (row-parallel)
- Adaptive quality: AA disabled during camera movement, full quality when still
- 2×2 jittered supersampling anti-aliasing
- **Three render modes** (N key cycles): CPU-BASIC (baseline L01–L21), CPU-OPT (frame cap + bilinear upscale + octant BVH), GPU (OpenGL fragment shader)
- CPU-OPT: 30fps frame rate cap via nanosleep, bilinear interpolation upscale (smooth edges at scale>1), octant-BVH voxel acceleration (~50% fewer box tests)
- GPU: in-process OpenGL 3.3 fragment shader running the L21 GLSL raytracer — full resolution at 60fps with near-zero CPU usage
- Feature toggles: V(voxels) F(floor) B(boxes) M(meshes) R(reflections) T(refractions) H(shadows) E(envmap) X(AA) Tab(scale) C(envmap mode) **N(render mode)**
- **P** key exports the current frame as `out.ppm`
- **A** key exports an anaglyph stereo image as `anaglyph.ppm` (viewable with red/cyan 3D glasses)
- **S** key exports a side-by-side stereoscope image as `sidebyside.ppm` (viewable in VR headsets or cross-eyed)
- **G** key exports a random-dot autostereogram as `stereogram.ppm` (viewable wall-eyed or cross-eyed)
- **Shift+L** exports a complete GLSL Shadertoy raytracer as `raytracer.glsl`
- HUD overlay showing FPS, frame time, ray count, scale, AA, feature toggles, envmap mode, render mode, per-mode implementation details, GPU renderer string
- Both X11 and Raylib backends working identically

---

## Final File Structure (Stage B)

```
tinyraytracer.wiki/
├── PLAN.md                            ← this file
├── README.md
├── PLAN-TRACKER.md
└── course/
    ├── build-dev.sh                   ← --backend=x11|raylib, -r, -d/--debug, -lm for math
    └── src/
        ├── utils/                          [PLATFORM — copied from template]
        │   ├── math.h                 ← CLAMP, MIN, MAX, ABS macros
        │   ├── backbuffer.h           ← Backbuffer struct + GAME_RGB/RGBA + color constants
        │   ├── draw-shapes.h          ← draw_rect / draw_rect_blend declarations
        │   ├── draw-shapes.c          ← clipped fill + alpha blend implementation
        │   ├── draw-text.h            ← draw_char / draw_text declarations
        │   ├── draw-text.c            ← FONT_8X8[128][8] + renderers
        │   └── audio.h               ← present but unused (raytracer is visual-only)
        ├── game/
        │   ├── base.h                 [PLATFORM — adapted buttons for raytracer]
        │   ├── main.h                 ← RaytracerState, Camera structs
        │   ├── main.c                 ← game_init, game_update, game_render
        │   ├── vec3.h                 ← Vec3 struct + all vector operations (header-only, static inline)
        │   ├── ray.h                  ← Ray, HitRecord structs (header-only)
        │   ├── scene.h                ← Material, Sphere, Light, Scene structs + scene_init (header-only)
        │   ├── intersect.h            ← intersection function declarations
        │   ├── intersect.c            ← sphere/plane intersection, nearest-hit traversal
        │   ├── lighting.h             ← lighting function declarations
        │   ├── lighting.c             ← Phong diffuse + specular + shadow rays
        │   ├── refract.h              ← Snell's law refract() function (header-only, static inline)
        │   ├── raytracer.h            ← cast_ray declaration
        │   ├── raytracer.c            ← recursive cast_ray + reflect/refract material blending
        │   ├── render.h               ← render_scene declaration
        │   ├── render.c               ← per-pixel loop, camera ray generation, Camera struct usage
        │   ├── ppm.h                  ← PPM P6 writer (header-only, static inline)
        │   ├── stereo.h               ← anaglyph + side-by-side stereo declarations
        │   ├── stereo.c               ← dual-camera render, anaglyph channel merge
        │   ├── stereogram.h           ← autostereogram declarations
        │   ├── stereogram.c           ← depth buffer extraction, union-find, SIRDS rendering
        │   ├── voxel.h                ← VoxelModel, bunny bitfield, voxel_is_solid, palette, VoxelOctant
        │   ├── voxel.c                ← voxel_model_intersect (brute-force) + voxel_model_intersect_bvh (octant BVH)
        │   ├── envmap.h               ← EnvMap, EnvMapMode, CubeMapFace, envmap_sample
        │   ├── envmap.c               ← equirect/cubemap/procedural sky sampling + stb_image
        │   ├── mesh.h                 ← Triangle, TriMesh, Moller-Trumbore ray-triangle
        │   ├── mesh.c                 ← fast_obj .obj loading, icosahedron, AABB early-out
        │   ├── settings.h             ← RenderSettings struct + RenderMode enum + render_settings_init
        │   ├── shader_glsl.h          ← GLSL Shadertoy raytracer as C string + export
        │   └── gpu_shader.h           ← GLSL wrapping for native OpenGL (#version 330 preamble, uniforms, main() epilogue)
        ├── utils/                          [PLATFORM — copied from template]
        │   ├── math.h, backbuffer.h, draw-shapes.h/.c, draw-text.h/.c, audio.h
        │   ├── stb_image.h            ← stb_image v2.30 (declarations only)
        │   ├── stb_image_impl.c       ← STB_IMAGE_IMPLEMENTATION (X11 only)
        │   └── fast_obj.h             ← fast_obj v1.3 single-header .obj loader
        ├── platforms/
        │   ├── x11/                        [PLATFORM — key bindings adapted]
        │   │   ├── base.h
        │   │   ├── base.c
        │   │   ├── main.c
        │   │   └── audio.c           ← present but unused
        │   └── raylib/
        │       └── main.c                 [PLATFORM — key bindings adapted]
        └── platform.h                      [PLATFORM — title/dimensions adapted]
```

**Why header-only files in `game/`?** Mathematical types (`Vec3`, `Ray`, `HitRecord`) and small utility functions (`vec3_add`, `vec3_dot`, `refract`) are best as `static inline` in headers. This avoids function call overhead for operations called millions of times per frame (once per pixel × multiple bounces). The compiler can inline and vectorize these hot-path operations. Larger algorithmic functions (`scene_intersect`, `cast_ray`, `render_scene`) live in `.c` files for clean compilation units and readable lesson diffs.

**Why `vec3.h` in `game/` not `utils/`?** The parameter test (course-builder.md §Step 2) asks: "can this code be dropped unchanged into the next game?" `Vec3` is raytracer-specific — a different game might use `Vec2` for 2D, or the platform math macros. It carries domain-specific operations (`vec3_reflect`, `vec3_refract`) that would pollute `utils/`. Game-specific math stays in `game/`.

**Why multiple `.c` files in `game/`?** Each `.c` file corresponds to a distinct algorithmic subsystem (intersection, lighting, raytracing, rendering). This produces clean lesson diffs — when Lesson 07 adds shadows, only `lighting.c` changes. A single `game/main.c` with all ray-tracing logic would make lesson diffs unreadable.

**Why no `game/audio.c` or `game/audio_demo.c`?** A raytracer has no game audio. The `utils/audio.h` and platform audio files are present (inherited from the template) but unused. The `game/base.h` audio-related buttons are omitted. The build script's SOURCES list does not include audio demo files.

---

## Topic Inventory

Every item below must appear in exactly one lesson's "New concepts" column. Items marked `[PLATFORM]` are copied from the Platform Foundation Course and not re-taught.

### `build-dev.sh`

- `[PLATFORM]` `clang` invocation, include paths, linker flags
- `[PLATFORM]` `SOURCES` variable, `--backend` flag, `-r`/`-d` flags
- `-lm` linker flag for math library (added for raytracer)
- Game title = `"TinyRaytracer"` in build output message

### `src/utils/` (all `[PLATFORM — copied from template]`)

- `math.h`: `MIN`, `MAX`, `CLAMP`, `ABS` macros
- `backbuffer.h`: `Backbuffer` struct, `GAME_RGB/RGBA`, color constants, pixel addressing
- `draw-shapes.h/.c`: `draw_rect`, `draw_rect_blend`
- `draw-text.h/.c`: `FONT_8X8[128][8]`, `draw_char`, `draw_text`
- `audio.h`: `AudioOutputBuffer`, `PlatformAudioConfig` (present but unused)

### `src/game/vec3.h` (header-only)

- `Vec3` struct: `float x, y, z`
- `vec3_make(x, y, z)` — compound literal constructor
- `vec3_add(a, b)` — component-wise addition
- `vec3_sub(a, b)` — component-wise subtraction
- `vec3_scale(v, s)` — scalar multiplication
- `vec3_negate(v)` — negate all components
- `vec3_dot(a, b)` — dot product (scalar result)
- `vec3_length(v)` — Euclidean length via `sqrtf`
- `vec3_normalize(v)` — unit vector (divide by length)
- `vec3_reflect(incident, normal)` — reflection formula: `I - 2*dot(I,N)*N`
- `vec3_cross(a, b)` — cross product (used for camera right vector)
- `vec3_lerp(a, b, t)` — linear interpolation

### `src/game/ray.h` (header-only)

- `Ray` struct: `Vec3 origin`, `Vec3 direction`
- `HitRecord` struct: `float t` (distance), `Vec3 point`, `Vec3 normal`, `int material_index`
- `ray_at(ray, t)` — point along ray: `origin + t * direction`

### `src/game/scene.h` (header-only)

- `Material` struct: `Vec3 diffuse_color`, `float specular_exponent`, `Vec3 albedo` (4 components: diffuse, specular, reflect, refract), `float refractive_index`
- `Sphere` struct: `Vec3 center`, `float radius`, `int material_index`
- `Light` struct: `Vec3 position`, `float intensity`
- `MAX_SPHERES`, `MAX_LIGHTS`, `MAX_MATERIALS` constants
- `Scene` struct: fixed arrays of `Sphere`, `Light`, `Material` with counts
- `scene_init(Scene *scene)` — populate 4 spheres, 3 lights, 4 materials (ivory, red rubber, mirror, glass)

### `src/game/intersect.h/.c`

- `sphere_intersect(ray, sphere, hit)` — geometric ray-sphere intersection algorithm
- `plane_intersect(ray, hit)` — ray-plane intersection for y=0 checkerboard floor
- `scene_intersect(ray, scene, hit)` — nearest-hit traversal across all spheres + floor plane

### `src/game/lighting.h/.c`

- `compute_lighting(point, normal, view_dir, material, scene, lights)` — returns `Vec3` color
- Diffuse component: `max(0, dot(light_dir, N)) * intensity`
- Specular component: `pow(max(0, dot(reflect(-light_dir, N), view_dir)), specular_exponent) * intensity`
- Shadow ray: cast toward light, offset by `point + N * 1e-3`, skip light if occluded
- Shadow acne prevention: epsilon offset along normal

### `src/game/refract.h` (header-only)

- `refract(incident, normal, eta_ratio)` — Snell's law implementation
- Inside/outside detection: `dot(I, N) < 0` means outside; flip normal and swap eta
- Total internal reflection check: `1 - eta² * (1 - cos²θ) < 0`

### `src/game/raytracer.h/.c`

- `MAX_RECURSION_DEPTH = 4`
- `cast_ray(ray, scene, depth)` — recursive ray casting
- Background color return when depth exceeded or no hit
- Reflect direction + origin offset for reflection ray
- Refract direction + origin offset for refraction ray
- Material blending: `diffuse * albedo[0] + specular * albedo[1] + reflect_color * albedo[2] + refract_color * albedo[3]`
- Checkerboard pattern integration: `((int)floor(x) + (int)floor(z)) & 1` procedural color

### `src/game/render.h/.c`

- `Camera` struct: `Vec3 position`, `float yaw`, `float pitch`, `float fov`, `float orbit_radius`
- `camera_init(camera)` — default position, fov = M_PI/3
- `camera_update(camera, input, delta_time)` — arrow key orbit (sin/cos yaw/pitch)
- `render_scene(backbuffer, scene, camera)` — per-pixel loop: NDC → ray direction → `cast_ray` → pixel color
- Pixel-to-ray direction: NDC conversion with FOV and aspect ratio (ssloy's formula)
- Camera rotation matrix from yaw — forward/right/up basis vectors

### `src/game/ppm.h` (header-only)

- `ppm_export(backbuffer, filename)` — PPM P6 binary writer
- `fopen`/`fwrite`/`fclose` — C standard file I/O
- Float-to-byte clamping: `(uint8_t)(255 * fminf(1.0f, fmaxf(0.0f, value)))`

### `src/game/intersect.h/.c` — Box extension (L13)

- `Box` struct: `Vec3 center`, `Vec3 half_size`
- `box_intersect(ray, box, hit)` — per-face ray-AABB intersection (check each of 6 axis-aligned faces)
- Face normal computation: axis-aligned unit normal on the hit dimension (`normal[d] = side`)
- `scene_intersect` extended to traverse boxes after spheres, before floor

### `src/game/scene.h` — Box extension (L13)

- `MAX_BOXES` constant
- `Box` struct added to `Scene` (fixed array + count)
- `scene_init` extended: add 1 box (positioned between spheres)
- Box `material_index` for Phong shading

### `src/game/stereo.h/.c` (L14)

- `EYE_SEPARATION` constant (interpupillary distance in world units, ~0.065)
- `STEREO_OVERSIZE_FRAC` constant (10% wider render for frustum correction)
- Parallax concepts: zero parallax (at screen), positive parallax (behind screen), negative parallax (in front)
- `render_eye_cropped(out, w, h, scene, eye_cam, settings)` — render at wider resolution (110% width), crop center portion to compensate off-axis parallax shift (ssloy's "render wider then crop" technique for asymmetric frustum)
- `render_anaglyph(...)` — render left+right eye via `render_eye_cropped`, merge left→red channel, right→cyan (green+blue) channel
- `render_side_by_side(...)` — render left+right eye via `render_eye_cropped`, place side by side (stereoscope/VR format)
- `ppm_export_rgb(pixels, width, height, filename)` — write raw RGB pixel buffer to PPM

### `src/game/stereogram.h/.c` (L15)

- `render_depth_buffer(width, height, scene, camera, zbuffer)` — per-pixel depth extraction (store normalized `t` instead of color, 0=far, 1=near)
- `parallax(z)` — pixel offset for given depth: `e * (1 - mu*z) / (2 - mu*z)` (ssloy's formula where distance `d` cancels out)
- `STEREO_MU` constant (`0.33f`) — near plane as fraction of far plane distance
- `STEREOGRAM_EYE_PX` constant (`400`) — interpupillary distance in pixels
- `uf_find(same, x)` — union-find with path compression (recursive, 3 lines)
- `uf_union(same, x, y)` — union two pixel indices (3 lines)
- `render_sirds_internal(...)` — shared core: per-row constraint propagation with `cross_eyed` flag that swaps union direction
- `render_autostereogram(...)` — wall-eyed variant (default, G key): near objects pop out toward viewer
- `render_autostereogram_crosseyed(...)` — cross-eyed variant (Shift+G key): parallax reversed for cross-eyed viewing (ssloy Part 2: "remember positive and negative parallax?")
- Source image preparation: periodic random pattern with `sinf` stripe hints (aids eye convergence, stripes spaced at max parallax width)

### `src/game/voxel.h/.c` (L16)

- `VOXEL_W`, `VOXEL_H`, `VOXEL_D` — grid dimensions (6×6×4 for bunny)
- `VOXEL_SIZE` — world-unit size of each voxel cube (0.10f)
- `BUNNY_BITFIELD[5]` — `uint32_t` array packing 144 boolean cells (ssloy Part 3 Step 8)
- `voxel_is_solid(id)` — bit test: `BUNNY_BITFIELD[id/32] & (1u << (id & 31))`
- `VoxelModel` struct: `position`, `scale`, `material_index`
- `voxel_color_from_id(cell_id)` — integer hash → deterministic RGB palette color (ssloy Part 3 Step 9)
- `voxel_model_intersect(ray, model, hit, voxel_color_out)` — triple nested loop over `(i,j,k)`, construct Box per solid cell, `box_intersect` each, keep nearest

### `src/game/envmap.h/.c` (L18 + L20)

- `EnvMapMode` enum: `ENVMAP_PROCEDURAL`, `ENVMAP_EQUIRECT`, `ENVMAP_CUBEMAP` — three modes of environment mapping
- `EnvMap` struct: mode, equirectangular pixels/width/height/channels, cube map `CubeMapFace faces[6]`
- `CubeMapFace` struct: pixels, width, height, channels (per-face image data)
- `CUBEMAP_FACE_POS_X` .. `CUBEMAP_FACE_NEG_Z` — face index constants (OpenGL convention)
- `envmap_init_procedural(e)` — initialize to procedural sky (no file needed)
- `envmap_load(e, filename)` — load equirectangular image via stb_image (L18)
- `envmap_load_cubemap(e, face_files[6])` — load 6 cube map faces with rollback on failure (L20)
- `envmap_free(e)` — free all loaded image data
- `envmap_sample(e, direction)` — dispatch to equirect, cubemap, or procedural sky based on mode
- `direction_to_uv(d, &u, &v)` — spherical UV: `u = 0.5 + atan2(z,x)/(2π)`, `v = 0.5 - asin(y)/π` (L18)
- `sample_cubemap(e, d)` — major-axis face selection with OpenGL UV convention (L20)
- `procedural_sky(dir)` — ground→horizon→zenith gradient (no file needed)

### `src/game/mesh.h/.c` (L19)

- `Triangle` struct: 3 vertices + 3 normals
- `TriMesh` struct: triangles array + count + AABB bounding box
- `triangle_intersect(ray, tri, t_out, normal_out)` — Moller-Trumbore algorithm with barycentric normal interpolation
- `mesh_intersect(ray, mesh, hit)` — AABB early-out + per-triangle traversal
- `mesh_load_obj(mesh, filename, material_index)` — fast_obj loading with fan triangulation
- `mesh_create_icosahedron(mesh, center, radius, material_index)` — procedural 20-triangle mesh
- `mesh_free(mesh)` — free triangle array

### `src/game/settings.h` (L12)

- `RenderSettings` struct: 8 show toggles (voxels, floor, boxes, meshes, reflections, refractions, shadows, envmap) + `render_scale` (1/2/3) + `aa_samples` (1/4) + `render_mode` (CPU_BASIC/CPU_OPT/GPU)
- `RenderMode` enum: `RENDER_CPU_BASIC`, `RENDER_CPU_OPT`, `RENDER_GPU`, `RENDER_MODE_COUNT`
- `render_settings_init(s)` — defaults: all on, scale=2, aa=4, render_mode=CPU_BASIC

### `src/game/shader_glsl.h` (L21)

- `GLSL_RAYTRACER_SOURCE[]` — complete GLSL Shadertoy fragment shader as C string constant
- `shader_export_glsl(filename)` — write shader string to file
- Shader replicates CPU raytracer: 4 spheres, 3 lights, Phong shading, checkerboard, shadows, iterative reflections (no GLSL recursion)

### `src/game/gpu_shader.h` (L23)

- `GPU_VERT_SHADER` — fullscreen triangle vertex shader via `gl_VertexID` (3 vertices: `(-1,-1)`, `(3,-1)`, `(-1,3)`)
- `GPU_FRAG_PREAMBLE` — `#version 330 core`, `precision highp float`, uniform declarations (`iResolution`, `iTime`, `iMouse`), `out vec4 FragColor`
- `GPU_FRAG_EPILOGUE` — `void main() { mainImage(FragColor, gl_FragCoord.xy); }` — bridge from Shadertoy entry point to OpenGL
- `gpu_build_fragment_source(buf, bufsize)` — concatenates preamble + `GLSL_RAYTRACER_SOURCE` + epilogue into caller buffer

### `src/game/voxel.h/.c` — Octant BVH extension (L22)

- `VoxelOctant` struct: `center`, `half_size`, `cell_indices[]`, `cell_count` — one sub-AABB of the voxel grid
- `VOXEL_OCTANTS` constant (8)
- `octants[VOXEL_OCTANTS]` in `VoxelModel` — 8 octants partitioned at bbox center
- `voxel_model_intersect_bvh(ray, model, hit, voxel_color_out)` — two-level hierarchy: model AABB → octant sub-AABBs → per-cell tests

### `src/game/render.h/.c` — Bilinear upscale extension (L22)

- `render_scene_to_float_mt(framebuffer, width, height, scene, cam, settings)` — multi-threaded float-buffer rendering
- `bilinear_upscale(src, sw, sh, bb)` — 4-pixel bilinear interpolation from float buffer to backbuffer
- CPU-OPT branch in `render_scene()`: at `scale > 1`, use float buffer + bilinear upscale instead of block-fill

### `src/utils/stb_image.h` (L18)

- stb_image v2.30 single-header image loader (declarations only)
- `stbi_load(filename, &w, &h, &ch, desired_channels)` — load JPG/PNG/HDR
- `stbi_image_free(data)` — free loaded image data
- X11 backend: implementation compiled via `stb_image_impl.c`; Raylib: uses internal copy

### `src/utils/fast_obj.h` (L19)

- fast_obj v1.3 single-header .obj loader
- `fastObjMesh *fast_obj_read(filename)` — parse .obj file
- `fast_obj_destroy(mesh)` — free parsed mesh
- Implementation compiled via `FAST_OBJ_IMPLEMENTATION` define in `mesh.c`

### `src/game/main.h`

- `RaytracerState` struct: `Scene scene`, `RtCamera camera`, `RenderSettings settings`, `int still_frames`, `int active_scale`, `float frame_time_ms`, `float fps_smoothed`, export flags (ppm, anaglyph, sidebyside, stereogram, stereogram_cross, glsl), GPU fields (`gpu_available`, `gpu_compile_ms`, `gpu_renderer[128]`), enhanced metrics (`ray_count_k`, `rays_per_sec_m`, `cpu_util_pct`, `thread_count`)
- `game_init(state)`, `game_update(state, input, delta_time)`, `game_render(state, backbuffer)` declarations

### `src/game/main.c`

- `game_init(state)` — calls `scene_init`, `camera_init`, `render_settings_init`; initializes `active_scale`
- `game_update(state, input, delta_time)` — camera update, adaptive quality (still_frames tracking), feature toggles (V/F/B/M/R/T/H/E/X), envmap mode cycling (C), scale cycling (Tab), render mode cycling (N, skip GPU if unavailable), export triggers (P/A/S/G/Shift+G/Shift+L)
- `game_render(state, backbuffer)` — adaptive AA (disabled during movement), timed `render_scene`, frame rate cap for CPU-OPT (nanosleep), GPU mode skips render_scene, FPS smoothing, HUD overlay (5 lines: metrics, controls, toggles+envmap mode, camera state+exports, per-mode implementation details), all export handlers
- HUD: FPS (color-coded green/yellow/red), frame time ms, ray count K, scale, AA, `[moving]` indicator, feature toggle status, envmap mode (sky/eq/cube), render mode (CPU-BASIC/CPU-OPT/GPU), camera orbit/yaw/pitch, per-mode details (threads, bilinear/block-fill, brute-force/octant-BVH, GPU renderer)

### `src/game/base.h` (adapted from template)

- `GameButtonState`, `UPDATE_BUTTON`, `prepare_input_frame`, `platform_swap_input_buffers` — `[PLATFORM]`
- `MouseState` struct: `x`, `y`, `dx`, `dy`, `scroll`, `left_down`, `right_down`, `middle_down` (Three.js OrbitControls-style input)
- 24 named button fields: quit, camera (6: left/right/up/down/forward/backward), exports (6: ppm/anaglyph/sidebyside/stereogram/stereogram_cross/glsl), toggles (9: voxels/floor/boxes/meshes/reflections/refractions/shadows/envmap/aa), cycle_envmap_mode, scale_cycle, cycle_render_mode
- `all[27]` union array for bulk iteration

### `src/platform.h` (adapted from template)

- `[PLATFORM]` `PlatformGameProps`, 4-function platform API, `STRINGIFY`/`TOSTRING`/`TITLE`
- `GAME_TITLE "TinyRaytracer"`, `GAME_W 800`, `GAME_H 600`

### `src/platforms/x11/` (adapted from template)

- `[PLATFORM]` X11 + GLX windowing, backbuffer upload, frame timing, letterbox
- Arrow keys (`XK_Left`, `XK_Right`, `XK_Up`, `XK_Down`) mapped to camera buttons
- `XK_p` mapped to `export_ppm`, `XK_a` mapped to `export_anaglyph` (L14), `XK_g` mapped to `export_stereogram` (L15)
- `XK_n` / `XK_N` mapped to `cycle_render_mode` (L22/L23)
- `XK_Escape` mapped to `quit`
- Audio init/write calls present but produce silence (no game audio state)
- L23 GPU: `GpuState` struct (20 GL function pointers via `glXGetProcAddressARB`), `gpu_init()` (compile/link shader), `gpu_render_scene()` (fullscreen triangle), `gpu_overlay_hud()` (alpha-blended HUD), `gpu_shutdown()`

### `src/platforms/raylib/` (adapted from template)

- `[PLATFORM]` Raylib windowing, backbuffer upload, frame timing, letterbox
- `KEY_LEFT`/`KEY_RIGHT`/`KEY_UP`/`KEY_DOWN` mapped to camera buttons
- `KEY_P` mapped to `export_ppm`, `KEY_A` mapped to `export_anaglyph` (L14), `KEY_G` mapped to `export_stereogram` (L15)
- `KEY_N` mapped to `cycle_render_mode` (L22/L23)
- `KEY_ESCAPE` mapped to `quit`
- Audio init calls present but produce silence (no game audio state)
- L23 GPU: `RaylibGpuState` struct, `rl_gpu_init()` (`LoadShaderFromMemory`), `rl_gpu_render()` (`BeginShaderMode`/`DrawRectangle`/`EndShaderMode`), `rl_gpu_overlay_hud()`, `rl_gpu_shutdown()`

---

## Proposed Lesson Sequence

| # | Title | What gets built | Observable outcome | New concepts (≤ 3 groups) | Files created/modified |
|---|-------|-----------------|-------------------|--------------------------|------------------------|
| 01 | Vec3 Math + Gradient Canvas | `vec3.h` with basic ops; gradient rendered to backbuffer | Color gradient fills the window (green→red vertical, blue→red horizontal) | `Vec3` struct + `vec3_make` compound literal constructor; `vec3_add`/`vec3_sub`/`vec3_scale` component-wise arithmetic; float-to-pixel conversion (`GAME_RGB(r*255, g*255, b*255)`) | `game/vec3.h` (created), `game/main.h` (created), `game/main.c` (created), `build-dev.sh` (adapted), `platform.h` (adapted), `game/base.h` (adapted), both backends (adapted) |
| 02 | Vec3 Advanced + PPM Export | `vec3_dot`/`vec3_length`/`vec3_normalize`; PPM writer | Gradient saved as `out.ppm`; viewable in image viewer | `vec3_dot`/`vec3_length`/`vec3_normalize` (dot product, Euclidean norm, unit vector); PPM P6 binary format (header + raw RGB bytes); `fopen`/`fwrite`/`fclose` (C file I/O) | `game/vec3.h` (modified), `game/ppm.h` (created), `game/main.c` (modified) |
| 03 | Ray-Sphere Intersection | Ray struct; single sphere on solid background | Single ivory sphere visible against blue background | `Ray`/`HitRecord` structs (origin+direction, distance+point+normal); geometric ray-sphere intersection algorithm (project center, compute discriminant); pixel-to-ray direction (FOV, aspect ratio, NDC mapping — ssloy's formula) | `game/ray.h` (created), `game/intersect.h` (created), `game/intersect.c` (created), `game/main.c` (modified) |
| 04 | Multiple Spheres + Materials | 4 spheres with distinct colors; `Scene` struct | Four colored spheres visible (ivory, red, mirror-gray, glass-blue tint) | `Material` struct (diffuse color, specular exponent, albedo weights, refractive index); `Scene` struct (fixed arrays of spheres, lights, materials + counts); nearest-hit traversal (`scene_intersect` — loop spheres, keep closest `t`) | `game/scene.h` (created), `game/intersect.h/.c` (modified), `game/main.h` (modified), `game/main.c` (modified) |
| 05 | Diffuse Lighting | 3 point lights; Lambertian shading | Spheres show light/dark shading (3D depth perception) | `Light` struct (position + intensity); Lambertian diffuse: `max(0, dot(light_dir, N)) * intensity`; surface normal computation at hit point (`(hit.point - sphere.center) / radius`) | `game/lighting.h` (created), `game/lighting.c` (created), `game/scene.h` (modified — lights added to scene_init), `game/main.c` (modified) |
| 06 | Specular Highlights | Phong specular on ivory; rubber stays matte | Shiny highlights on ivory sphere; red rubber stays flat | `vec3_reflect(I, N)` formula: `I - 2*dot(I,N)*N`; Phong specular: `pow(max(0, dot(R, V)), exp) * intensity`; `specular_exponent` in Material (ivory=50 shiny, rubber=10 matte) | `game/vec3.h` (modified — add `vec3_reflect`), `game/lighting.h/.c` (modified — add specular), `game/main.c` (modified) |
| 07 | Hard Shadows | Spheres cast shadows on each other | Dark shadow regions visible where light is blocked | Shadow ray: cast from hit point toward each light source; epsilon offset `point + N * 1e-3` (shadow acne prevention); occlusion test: if `scene_intersect` hits something closer than the light, skip that light | `game/lighting.c` (modified — add shadow test) |
| 08 | Reflections | Mirror sphere reflects the entire scene | Third sphere shows reflected image of other spheres and background | Recursive `cast_ray(depth+1)` with `MAX_RECURSION_DEPTH=3`; reflect direction + origin offset (reuse `vec3_reflect`); material blending: `diffuse*albedo[0] + specular*albedo[1] + reflect_color*albedo[2]` | `game/raytracer.h` (created), `game/raytracer.c` (created), `game/lighting.c` (modified — extracted from cast_ray), `game/main.c` (modified) |
| 09 | Refractions | Glass sphere bends light through it | Fourth sphere shows distorted refracted view of scene behind it | Snell's law `refract()`: `eta * I + (eta * cos_i - cos_t) * N`; inside/outside detection: `dot(I,N) < 0` = outside, flip normal + swap eta; material blending extended: `+ refract_color * albedo[3]` | `game/refract.h` (created), `game/raytracer.c` (modified), `game/scene.h` (modified — glass material refractive_index=1.5) |
| 10 | Checkerboard Floor | Floor plane with alternating pattern | Checkerboard pattern extends to horizon beneath the spheres | Ray-plane intersection: `t = -origin.y / dir.y` (y=0 plane); procedural pattern: `((int)floor(x) + (int)floor(z)) & 1` alternating colors; plane integration into `scene_intersect` (check floor after spheres, keep nearest) | `game/intersect.c` (modified — add `plane_intersect`), `game/raytracer.c` (modified — checkerboard color in cast_ray) |
| 11 | Interactive Camera + Mouse + Multi-threading | Mouse orbit/pan/scroll + arrow keys + WASD; pthreads parallel rendering; camera basis hoisted out of per-pixel loop | Mouse drag orbits camera; scroll zooms; arrow keys orbit; W/S zoom; smooth real-time interaction | `RtCamera` struct (position, target, yaw, pitch, fov, orbit_radius); `MouseState` in GameInput; `CameraBasis` (pre-computed once per frame); pthreads row-parallel rendering (`pthread_create`/`pthread_join` across CPU cores); orbit math with look-at target | `game/render.h` (modified — RtCamera, CameraBasis, mouse constants), `game/render.c` (modified — camera_update with mouse/keyboard, multi-threaded render_scene, camera_compute_basis), `game/base.h` (modified — add MouseState, camera_forward/backward buttons, 7 buttons total), both backends (modified — mouse events + WASD keys), `build-dev.sh` (modified — add `-lpthread` for X11) |
| 12 | Settings + Toggles + AA + HUD + Performance | `settings.h` created; feature toggles (V/F/B/R/T/H/X/Tab); 2×2 jittered SSAA; adaptive quality; HUD with FPS/ms/rays; PPM export | HUD shows FPS, frame time, ray count; press V/F/B/R/T/H to toggle features; X for AA; Tab for scale; P for PPM | `RenderSettings` struct + `render_settings_init`; 2×2 jittered supersampling (`trace_pixel_aa4` with rotated grid offsets); adaptive quality (AA disabled while camera moving, re-enabled when still); feature toggle buttons; frame timing via `clock_gettime`; `vec3_cross`/`vec3_lerp` added | `game/settings.h` (created), `game/vec3.h` (modified — `vec3_cross`, `vec3_lerp`), `game/base.h` (modified — add 15 toggle/export/scale buttons, 22 total), `game/intersect.h/.c` (modified — add `RenderSettings *` parameter to `scene_intersect`), `game/lighting.h/.c` (modified — add `RenderSettings *` for shadow toggle), `game/raytracer.h/.c` (modified — add `RenderSettings *` for reflection/refraction toggles), `game/render.h/.c` (modified — add `render_scene_to_float`, AA logic in thread function), `game/main.h` (modified — add `still_frames`, `active_scale`, `frame_time_ms`, `fps_smoothed`), `game/main.c` (rewritten — adaptive quality, toggle handling, timed render, HUD with metrics, PPM export), both backends (modified — all toggle/export key bindings) |
| 13 | Ray-Box Intersection | `Box` struct; per-face AABB intersection; cube added to scene | A lit, shadowed cube appears in the scene alongside the 4 spheres | `Box` struct (center + half_size); per-face ray-AABB intersection algorithm (iterate 3 axes, check each face pair, keep nearest); axis-aligned face normal (`normal[d] = ±1` on hit dimension) | `game/scene.h` (modified — add `Box`, `MAX_BOXES`, box in Scene), `game/intersect.h/.c` (modified — add `box_intersect`, extend `scene_intersect`), `game/main.c` (modified — scene_init adds cube) |
| 14 | Anaglyph + Side-by-Side Stereo | Dual-camera render with wider-FOV-then-crop (ssloy's asymmetric frustum technique); red/cyan anaglyph + side-by-side stereoscope | Press A → `anaglyph.ppm`; Press S → `sidebyside.ppm` | Positive/negative/zero parallax (binocular depth); off-axis stereo via "render wider then crop" (`render_eye_cropped`: render at 110% width, crop center to compensate parallax shift); anaglyph channel merge (left→red, right→cyan) + side-by-side layout | `game/stereo.h/.c` (created — `render_anaglyph`, `render_side_by_side`, `render_eye_cropped`), `game/base.h` (modified), both backends (modified — A/S keys), `game/main.c` (modified) |
| 15 | Autostereogram (Wall-eyed + Cross-eyed) | Depth buffer; union-find; SIRDS with wall-eyed/cross-eyed variants; G/Shift+G keys | Press G → `stereogram.ppm` (wall-eyed); Shift+G → `stereogram_cross.ppm` (cross-eyed) | Depth buffer extraction (normalized `t` per pixel); union-find (`uf_find` path compression + `uf_union`); parallax constraint propagation; wall-eyed vs cross-eyed (swap union direction inverts depth — ssloy Part 2: "remember positive and negative parallax?") | `game/stereogram.h/.c` (created — `render_autostereogram` + `render_autostereogram_crosseyed`, shared `render_sirds_internal`), `game/base.h` (modified — `export_stereogram` + `export_stereogram_cross`), both backends (modified — g/G keys), `game/main.c` (modified) |
| 16 | Voxel Rendering | `VoxelModel` struct; bunny bitfield; per-cell box intersection; palette coloring | Voxelized bunny appears in scene with per-voxel colors, fully lit with shadows and reflections | Bitfield packing (`uint32_t[]` → `bool voxel_is_solid(id)` via bit test); triple nested voxel grid traversal (`i,j,k` → cell_id → Box); per-voxel palette color (hash-based color from cell ID overrides material diffuse) | `game/voxel.h` (created — bunny bitfield, VoxelModel struct, voxel_is_solid, voxel_color_from_id), `game/voxel.c` (created — voxel_model_intersect), `game/scene.h` (modified — VoxelModel in Scene, scene_init adds bunny), `game/intersect.c` (modified — scene_intersect traverses voxel models), `game/raytracer.c` (modified — voxel palette color override) |
| 17 | Beyond — Modern Rendering Techniques | Capstone roadmap (no new code); 10 modern techniques contextualized against what was built + GLSL shader reference | Student has a clear roadmap for extending the raytracer toward production rendering; complete GLSL fragment shader provided for Shadertoy | Path tracing; BVH; GPU raytracing (complete GLSL shader code for Shadertoy with explanation); denoising; PBR; importance sampling; ambient occlusion; texture mapping; normal mapping; depth of field | `course/lessons/lesson-17-modern-rendering-techniques.md` (lesson file only — includes GLSL shader source as reference) |
| 18 | Environment Mapping | `EnvMap` struct; stb_image loading; spherical UV mapping; procedural sky fallback | Background shows spherical image (envmap.jpg) or procedural sky; reflected objects show environment | Spherical environment map (equirectangular projection: `u = 0.5 + atan2(z,x)/(2*PI)`, `v = 0.5 - asin(y)/PI`); stb_image loading (`stbi_load`/`stbi_image_free`); procedural sky gradient (zenith→horizon→ground); E key toggle | `game/envmap.h/.c` (created), `game/scene.h` (modified — `EnvMap` in Scene, loaded in scene_init), `game/raytracer.c` (modified — `envmap_sample` replaces background_color) |
| 19 | Triangle Meshes | `TriMesh` struct; Moller-Trumbore ray-triangle; fast_obj .obj loading; procedural icosahedron | Icosahedron appears in scene (smooth-shaded); students can load any .obj file | Moller-Trumbore ray-triangle intersection (barycentric coordinates, smooth-shaded normals); fast_obj .obj loading (triangulation of quads/n-gons); AABB bounding box for early-out; M key toggle | `game/mesh.h/.c` (created), `game/scene.h` (modified — TriMesh in Scene), `game/intersect.c` (modified — mesh_intersect in scene traversal) |
| 20 | Cube Map Textures | `CubeMapFace` struct; `EnvMapMode` enum; 6-face loading; direction-to-face UV mapping | Cube map background replaces equirect; C key cycles: sky/eq/cube; graceful fallback chain | Cube map direction-to-face (dominant axis selection, OpenGL UV convention); `envmap_load_cubemap` (6 stbi_load calls with rollback); multi-mode dispatch in `envmap_sample`; C key cycles available modes | `game/envmap.h` (modified — EnvMapMode, CubeMapFace, CUBEMAP_FACE_* defines), `game/envmap.c` (modified — load_cubemap, sample_cubemap, sample_face), `game/scene.h` (modified — fallback chain), `game/main.c` (modified — C key + HUD), `game/base.h` (modified — cycle_envmap_mode) |
| 21 | GLSL Fragment Shaders | Complete Shadertoy GLSL raytracer as C string; Shift+L export; C-to-GLSL mapping | Shift+L writes `raytracer.glsl`; paste into shadertoy.com for GPU rendering | GLSL string constant in `shader_glsl.h`; iterative reflection (no GLSL recursion); C-to-GLSL operation mapping (vec3_add→+, vec3_dot→dot, vec3_lerp→mix, HitRecord→out params); `mainImage` entry point; GPU parallelism vs CPU pthreads | `game/shader_glsl.h` (created — GLSL_RAYTRACER_SOURCE + shader_export_glsl), `game/main.c` (modified — Shift+L export + HUD), `game/main.h` (modified — export_glsl_requested), `game/base.h` (modified — export_glsl button) |
| 22 | CPU Optimization Techniques | Frame rate cap (nanosleep), bilinear upscale (float buffer), octant-BVH voxel acceleration; N key cycles to CPU-OPT mode | Press N → CPU-OPT: fan noise drops, bilinear smooth edges at scale>1, HUD shows "voxel:octant-BVH \| cap:30fps" | Frame rate capping with `nanosleep` (sleep unused frame budget, target 30fps); bilinear interpolation upscale (render at reduced resolution into Vec3 float buffer, 4-pixel blend to full resolution); octant-BVH acceleration (partition voxel grid into 8 sub-AABBs, skip empty octants before per-cell tests) | `game/settings.h` (modified — RenderMode enum, render_mode), `game/base.h` (modified — cycle_render_mode, all[27]), `game/main.h` (modified — GPU+metrics fields), `game/main.c` (modified — mode cycling, frame cap, 5-line HUD), `game/render.h/.c` (modified — float rendering, bilinear_upscale), `game/voxel.h/.c` (modified — VoxelOctant, octant partitioning, BVH intersect), `game/intersect.c` (modified — BVH branch), both backends (modified — N key) |
| 23 | GPU Rendering Mode | In-process OpenGL 3.3 fragment shader running L21 GLSL raytracer; gpu_shader.h wraps existing source; N key cycles to GPU mode | Press N×2 → GPU: same 4-sphere scene at 60fps, near-zero CPU, HUD shows "GPU: [renderer]" | `gpu_shader.h` wrapping (preamble + GLSL_RAYTRACER_SOURCE + epilogue, zero duplication); X11 GL function pointer loading (glXGetProcAddressARB, GpuState struct, fullscreen triangle via gl_VertexID); Raylib shader API (LoadShaderFromMemory, BeginShaderMode/EndShaderMode) | `game/gpu_shader.h` (created — GPU_VERT_SHADER, GPU_FRAG_PREAMBLE, GPU_FRAG_EPILOGUE, gpu_build_fragment_source), `platforms/x11/main.c` (modified — GpuState, gpu_init, gpu_render_scene, gpu_overlay_hud), `platforms/raylib/main.c` (modified — RaylibGpuState, rl_gpu_init, rl_gpu_render, rl_gpu_overlay_hud) |

---

## Concept Dependency Map

```
Vec3 struct → vec3_make → vec3_add/vec3_sub/vec3_scale
vec3_add/vec3_sub/vec3_scale → float-to-pixel conversion (GAME_RGB)

vec3_dot → vec3_length → vec3_normalize
vec3_normalize → pixel-to-ray direction (NDC + FOV + aspect ratio)

PPM P6 format → fopen/fwrite/fclose

Ray/HitRecord structs → ray_at
ray_at → sphere_intersect (geometric algorithm)
sphere_intersect → scene_intersect (nearest-hit traversal)

Material struct → Scene struct → scene_init
Scene struct → scene_intersect (material_index lookup)

Light struct → diffuse lighting (Lambertian dot product)
surface normal computation → diffuse lighting
diffuse lighting → specular lighting (Phong model)

vec3_reflect → specular highlights (reflect view dir, compute dot with light dir)
vec3_reflect → reflection ray (recursive cast_ray)

specular_exponent → Phong specular (pow(dot, exp))

shadow ray → epsilon offset (shadow acne prevention)
shadow ray → occlusion test (scene_intersect toward light)
diffuse lighting + specular lighting → shadow integration

MAX_RECURSION_DEPTH → recursive cast_ray(depth+1)
reflect direction + origin offset → reflection color
material blending (albedo weights) → diffuse + specular + reflect color

refract() (Snell's law) → inside/outside detection → refraction ray
refraction ray → refract_color → material blending extended (albedo[3])

plane_intersect (t = -origin.y/dir.y) → checkerboard pattern ((floor(x)+floor(z))&1)
plane_intersect → scene_intersect integration (check floor after spheres)

Camera struct → orbit math (sin/cos yaw/pitch)
orbit math → camera ray generation (forward/right/up basis)
camera ray generation → render_scene per-pixel loop
GameInput camera buttons → camera_update

draw_text + draw_rect_blend → HUD rendering
snprintf → camera state display
vec3_cross → camera right vector
vec3_lerp → (available for smooth transitions)
PPM export → export_ppm button wiring

Box struct (center + half_size) → per-face ray-AABB intersection
per-face ray-AABB intersection → axis-aligned face normal
Box in Scene → scene_intersect extended (traverse boxes after spheres)
scene_intersect (boxes) → lighting + shadows + reflections apply to box faces

EYE_SEPARATION (interpupillary distance) → off-axis stereo projection
Camera right vector (vec3_cross) → stereo camera offset (±EYE_SEPARATION/2 along right)
off-axis stereo projection → wider FOV render + crop
render left + render right → anaglyph channel merge (left=red, right=cyan)
anaglyph merge → ppm_export_rgb → export_anaglyph button

render_scene (all geometry) → render_depth_buffer (store t instead of color)
render_depth_buffer → parallax function (depth → pixel offset)
parallax function → union-find constraint propagation (union left+right pixel)
uf_find (path compression) + uf_union → constraint resolution
periodic random source image → cluster resolution (copy root pixel color)
constraint resolution → render_autostereogram → export_stereogram button

render_side_by_side → side-by-side layout (left+right horizontally) → export_sidebyside button

BUNNY_BITFIELD (uint32_t[5]) → voxel_is_solid (bit test: bitfield[id/32] & (1<<(id&31)))
VoxelModel struct (position, scale, material_index) → triple nested loop (i,j,k)
triple nested loop → cell_id → voxel_is_solid check → Box construction per cell
Box construction → box_intersect (reuse L13) → nearest voxel hit
voxel_color_from_id (integer hash → RGB) → palette color override in cast_ray

stb_image (stbi_load) → envmap_load (equirectangular .jpg/.png/.hdr)
direction_to_uv (atan2f, asinf) → sample_equirect (spherical UV → pixel lookup)
envmap_sample (dispatch by mode) → cast_ray background (replaces solid color)
procedural_sky (ground→horizon→zenith) → fallback when no image loaded
E key toggle → RenderSettings.show_envmap → envmap_sample vs procedural

fast_obj .obj loading (fast_obj_read) → mesh_load_obj (triangulation + normals)
mesh_create_icosahedron → procedural 20-triangle mesh
Moller-Trumbore (cross products, barycentric) → triangle_intersect
TriMesh AABB → mesh_intersect early-out → per-triangle traversal
M key toggle → RenderSettings.show_meshes → scene_intersect mesh traversal

CubeMapFace struct (per-face pixels) → envmap_load_cubemap (6 stbi_load calls)
direction → dominant axis → face index + UV → sample_face → sample_cubemap
EnvMapMode enum → envmap_sample dispatch (procedural/equirect/cubemap)
C key → cycle_envmap_mode → skip unavailable modes

GLSL_RAYTRACER_SOURCE (C string constant) → shader_export_glsl → fopen/fputs/fclose
C-to-GLSL mapping (vec3_add→+, cast_ray→iterative loop)
Shift+L → export_glsl_requested → shader_export_glsl("raytracer.glsl")

RenderMode enum (CPU_BASIC/CPU_OPT/GPU) → N key mode cycling → game_update
nanosleep → frame rate cap (CPU-OPT: sleep unused frame budget, target 30fps)
render_scene_to_float_mt (Vec3 float buffer) → bilinear_upscale (4-pixel blend → backbuffer)
VoxelOctant (8 sub-AABBs) → voxel_model_intersect_bvh (model AABB → octant AABB → per-cell)
RenderMode → intersect.c branch (CPU-OPT → BVH, CPU-BASIC → brute-force)

GLSL_RAYTRACER_SOURCE → gpu_build_fragment_source (preamble + source + epilogue)
gpu_build_fragment_source → gpu_init (compile vertex + fragment, link program)
gpu_init → gpu_render_scene (bind program, set uniforms, draw fullscreen triangle)
gpu_render_scene → gpu_overlay_hud (alpha-blend HUD texture on top)
RenderMode == RENDER_GPU → main loop branch (skip render_scene, call gpu_render_scene)
```

---

## Per-Lesson Skill Inventory Table

| Lesson | New concepts | Concepts re-used from prior lessons | Cognitive level |
|--------|-------------|-------------------------------------|-----------------|
| 01 | `Vec3` struct + `vec3_make` compound literal; `vec3_add`/`vec3_sub`/`vec3_scale`; float-to-pixel conversion (`GAME_RGB(r*255, g*255, b*255)`) | `[PLATFORM]` Backbuffer, `GAME_RGB`, `game_render`, build-dev.sh | Apply |
| 02 | `vec3_dot`/`vec3_length`/`vec3_normalize`; PPM P6 binary format; `fopen`/`fwrite`/`fclose` | `Vec3`, `vec3_make`, `vec3_scale`, `GAME_RGB` | Apply |
| 03 | `Ray`/`HitRecord` structs; geometric ray-sphere intersection (project, discriminant, solve t); pixel-to-ray direction (FOV, aspect ratio, NDC) | `Vec3`, `vec3_sub`, `vec3_dot`, `vec3_normalize`, `vec3_scale`, Backbuffer | Apply |
| 04 | `Material` struct (diffuse_color, specular_exponent, albedo, refractive_index); `Scene` struct (fixed arrays + counts); nearest-hit traversal (`scene_intersect`) | `Ray`, `HitRecord`, `sphere_intersect`, `Vec3`, pixel-to-ray | Apply |
| 05 | `Light` struct (position, intensity); Lambertian diffuse (`max(0, dot(L,N)) * intensity`); surface normal computation (`(point - center) / radius`) | `Scene`, `scene_intersect`, `HitRecord`, `vec3_dot`, `vec3_sub`, `vec3_normalize`, `Material` | Apply |
| 06 | `vec3_reflect(I, N)` formula; Phong specular (`pow(max(0, dot(R,V)), exp) * intensity`); `specular_exponent` (ivory=50 vs rubber=10) | `Light`, diffuse lighting, `vec3_dot`, `vec3_normalize`, `Material`, surface normal | Apply |
| 07 | Shadow ray toward light + occlusion test; epsilon offset `point + N*1e-3`; shadow acne prevention | `scene_intersect`, `Light`, diffuse, specular, `vec3_scale`, `vec3_add` | Analyze |
| 08 | Recursive `cast_ray(depth+1)` + `MAX_RECURSION_DEPTH`; reflect direction + origin offset; material blending (`diffuse*albedo[0] + specular*albedo[1] + reflect*albedo[2]`) | `vec3_reflect`, shadow rays, `scene_intersect`, `Material.albedo`, all lighting | Apply |
| 09 | Snell's law `refract()` function; inside/outside detection (flip normal, swap eta); material blending extended (`+ refract_color * albedo[3]`) | Recursive `cast_ray`, `vec3_dot`, `vec3_reflect` (contrast), `Material.refractive_index`, all prior raytracing | Apply |
| 10 | Ray-plane intersection (`t = -origin.y / dir.y`); procedural checkerboard (`(floor(x)+floor(z)) & 1`); plane integration into `scene_intersect` | `cast_ray`, `scene_intersect`, `HitRecord`, `Vec3`, all lighting + shadows + reflections + refractions | Apply |
| 11 | `Camera` struct (position, yaw, pitch, fov, orbit_radius); orbit math (`sin`/`cos` yaw/pitch); camera ray generation (forward/right/up basis vectors) | `GameInput` buttons, `render_scene`, pixel-to-ray, `vec3_normalize`, `vec3_sub`, `vec3_cross` | Apply |
| 12 | HUD via `draw_text` + `draw_rect_blend`; `snprintf` for camera state; `vec3_cross`/`vec3_lerp` finalization + PPM export wiring | `[PLATFORM]` `draw_text`, `draw_rect_blend`; `Camera`, `ppm_export`, `game_update`, `GameInput.export_ppm` | Apply |
| 13 | `Box` struct (center + half_size); per-face ray-AABB intersection (iterate axes, check face pair, nearest hit); axis-aligned face normal (`normal[d] = ±1`) | `scene_intersect`, `HitRecord`, `Ray`, `Material`, all lighting + shadows + reflections, `Scene` | Apply |
| 14 | Positive/negative/zero parallax (binocular depth perception); off-axis stereo via "render wider then crop" (ssloy's asymmetric frustum technique); anaglyph (left→red, right→cyan) + side-by-side stereoscope layout | `RtCamera`, `render_scene_to_float`, `vec3_cross` (right vector), `ppm_export_rgb`, `render_eye_cropped` (wider FOV + center crop), all raytracing | Apply |
| 15 | Depth buffer extraction (store normalized `t` per pixel); union-find (`uf_find` path compression + `uf_union`); parallax constraint propagation + wall-eyed vs cross-eyed viewing (swap union direction inverts depth) | `render_scene`, `scene_intersect`, `HitRecord.t`, `ppm_export_rgb`, `render_autostereogram` + `render_autostereogram_crosseyed` | Analyze |
| 16 | Bitfield packing (`uint32_t[]` → `voxel_is_solid()` bit test); triple nested voxel grid traversal (`i,j,k` → cell_id → Box per cell); per-voxel palette color (hash cell_id → RGB, override material diffuse in cast_ray) | `Box`, `box_intersect` (L13), `scene_intersect`, `cast_ray`, `RtMaterial.diffuse_color`, all lighting + shadows + reflections | Apply |
| 17 | 10 modern techniques: path tracing, BVH, GPU raytracing, denoising, PBR, importance sampling, ambient occlusion, texture mapping, normal mapping, depth of field | All L01–L16 concepts (roadmap lesson contextualizes what was built) | Understand |
| 18 | `EnvMap` struct; stb_image (`stbi_load`/`stbi_image_free`); spherical UV mapping (`atan2f`/`asinf`); procedural sky gradient (zenith→horizon→ground); E key toggle | `Scene`, `cast_ray`, `Vec3`, `vec3_normalize`, pixel-to-ray direction, `RenderSettings` toggle pattern (L12) | Apply |
| 19 | Moller-Trumbore ray-triangle intersection (barycentric coordinates); `TriMesh` struct; fast_obj .obj loading (`fastObjMesh`); AABB bounding box early-out; M key toggle | `Scene`, `scene_intersect`, `HitRecord`, `Ray`, `Vec3`, `vec3_cross`, `vec3_dot`, `vec3_sub`, `box_intersect` (L13 AABB), `RenderSettings` toggle | Apply |
| 20 | `CubeMapFace` struct; `EnvMapMode` enum; 6-face cube map loading; direction-to-face UV (dominant axis + OpenGL convention); C key mode cycling | `EnvMap` (L18), `stbi_load`, `envmap_sample`, `vec3_make`, `fabsf` comparisons, `RenderSettings` toggle pattern | Apply |
| 21 | GLSL string constant (`shader_glsl.h`); iterative reflection (GLSL forbids recursion); C-to-GLSL operation mapping; `mainImage` entry point; Shift+L export | All L01–L16 concepts (GLSL version mirrors CPU raytracer); `fopen`/`fputs` (L02 I/O pattern) | Analyze |
| 22 | Frame rate cap (`nanosleep`, sleep unused budget, target 30fps); bilinear interpolation upscale (Vec3 float buffer, 4-pixel blend); octant-BVH acceleration (`VoxelOctant`, 8 sub-AABBs, skip empty octants) | `RenderSettings` (L12), `render_scene` (L11), `voxel_model_intersect` (L16), `box_intersect` (L13), `pthreads` (L11), `scene_intersect` (L04), `nanosleep` timing, `N` key mode cycling | Apply |
| 23 | `gpu_shader.h` wrapping (preamble + `GLSL_RAYTRACER_SOURCE` + epilogue); X11 GL function pointer loading (`glXGetProcAddressARB`, `GpuState`, fullscreen triangle via `gl_VertexID`, HUD alpha blend overlay); Raylib shader API (`LoadShaderFromMemory`, `BeginShaderMode`/`EndShaderMode`) | `GLSL_RAYTRACER_SOURCE` (L21), `RenderMode` enum (L22), OpenGL texture upload (platform layer), `glXGetProcAddressARB` (VSync setup), HUD overlay, mode cycling | Apply |

**Coverage check:** Every item in the topic inventory maps to exactly one "New concepts" cell above. Platform items marked `[PLATFORM]` appear only in the "re-used" column. The `vec3_reflect` formula is taught in L06 (specular) and reused in L08 (reflections). The PPM writer is created in L02, wired to the P key in L12, and extended for RGB buffers in L14. The `Box` struct from L13 integrates into the existing `scene_intersect` and inherits all lighting/shadow/reflection behavior. L16 reuses `box_intersect` from L13 to compose many small boxes into a voxel object — demonstrating how a simple primitive becomes a building block for complex shapes. L22 optimizes the CPU path introduced in L01–L21 with frame capping, bilinear upscale, and octant-BVH acceleration. L23 bridges L21's GLSL export to an in-process GPU shader, demonstrating the CPU→GPU transition with zero code duplication. Cognitive levels progress from Apply (L01–L06) through Analyze (L07 — shadow acne debugging) back to Apply (L08–L14), Analyze (L15 — parallax/depth constraints), Apply (L16 — bitfield + voxel composition), Apply (L22 — optimization techniques), and Apply (L23 — GPU shader integration).

**Raylib name conflicts:** Source code uses `RtMaterial`, `RtCamera`, `RtRay` to avoid collisions with Raylib's built-in `Material`, `Camera`, and `Ray` types. PLAN.md topic inventory uses the conceptual names for clarity; the `Rt` prefix is a platform adaptation detail, not a new concept.

---

## JS → C Concept Mapping

| JS / Web concept | C equivalent in this course | Key difference |
|---|---|---|
| `class Vec3 { constructor(x,y,z) { this.x=x; ... } }` | `typedef struct { float x, y, z; } Vec3` + `vec3_make(x,y,z)` | No constructors; use compound literals or factory functions |
| `vec.add(other)` / `vec + other` | `vec3_add(a, b)` — explicit function call | No operator overloading in C; every operation is an explicit named function |
| `Math.sqrt(x)` | `sqrtf(x)` from `<math.h>` + `-lm` | Must link math library; `sqrtf` for `float` (not `sqrt` which is `double`) |
| `Math.max(a, b)` | `fmaxf(a, b)` or `MAX(a, b)` macro | C has no generic `max`; use type-specific or macro |
| `Math.pow(base, exp)` | `powf(base, exp)` from `<math.h>` | Float version; `-lm` required |
| `Math.floor(x)` | `floorf(x)` from `<math.h>` | Float version for checkerboard pattern |
| `Math.sin(x)` / `Math.cos(x)` | `sinf(x)` / `cosf(x)` | Float trigonometry for camera orbit |
| `Math.PI` | `M_PI` from `<math.h>` (or define manually) | Not always defined; may need `#define _USE_MATH_DEFINES` |
| `new Sphere(center, radius)` | `(Sphere){ .center = c, .radius = r, .material_index = i }` | C designated initializer — no `new`, no heap; stack or static |
| `spheres.push(s)` / `spheres[i]` | `scene.spheres[count] = s; scene.sphere_count++` | Fixed-size array; no dynamic resize; bounds-check manually |
| `for (const sphere of spheres)` | `for (int i = 0; i < scene.sphere_count; i++)` | Manual index loop; no iterators |
| `Infinity` | `FLT_MAX` from `<float.h>` | Largest representable float; used to initialize nearest-hit distance |
| `fs.writeFileSync('out.ppm', data)` | `fopen("out.ppm","wb")` + `fwrite(buf,1,n,f)` + `fclose(f)` | Three-step open/write/close; must check for errors; `"wb"` for binary |
| `if (depth > MAX) return bg` | Same pattern — recursion works the same way | C recursion is identical; stack depth is the concern (4 levels is fine) |
| `{ ...material, reflectivity: 0.8 }` | `material.albedo[2] = 0.8f` — explicit field access | No spread operator; set each field individually |
| `event.key === 'ArrowLeft'` | `case XK_Left: UPDATE_BUTTON(...)` | Platform keysym mapping; no event objects |
| `console.log(\`yaw: ${yaw}\`)` | `snprintf(buf, sizeof(buf), "yaw: %.1f", yaw)` + `draw_text(...)` | No template literals; format string + buffer; render to backbuffer |
| `new Box({ min, max })` | `(Box){ .center = c, .half_size = hs }` | Center+half_size representation; no min/max corners |
| `ctx.fillStyle = 'red'; ctx.drawImage(left)` | Channel extraction via bitwise ops: `red = (pixel >> 16) & 0xFF` | Manual channel merge for anaglyph; no canvas compositing |
| `depthBuffer[i] = distance` | `zbuffer[i] = normalized_t` (float array, 0.0–1.0) | Explicit depth buffer allocation; no GPU z-buffer hardware |
| `class UnionFind { find(x) { ... } }` | `int uf_find(int *same, int x)` — recursive with path compression | No classes; array-based union-find with `same[x] = x` identity init |
| `Math.random()` | `rand() / (float)RAND_MAX` | Seed with `srand(42)` for reproducible stereograms; no `crypto.getRandomValues` |

---

## Deviations from `course-builder.md` Standard Layout

| Deviation | Reason |
|-----------|--------|
| No audio (`game/audio.c`, `game/audio_demo.c`) | Raytracer is visual-only. Audio template files exist in `utils/audio.h` and platform backends but produce silence. No `SOUND_DEFS`, no `game_get_audio_samples`, no `game_audio_update`. |
| Many header-only files in `game/` (`vec3.h`, `ray.h`, `scene.h`, `refract.h`, `ppm.h`) | Mathematical types and small utility functions are best as `static inline` for performance — these are called millions of times per frame. Larger algorithms remain in `.c` files. |
| Multiple `.c` files in `game/` (`intersect.c`, `lighting.c`, `raytracer.c`, `render.c`, `main.c`) | Each file is a distinct algorithmic subsystem. This produces clean per-lesson diffs (L07 only changes `lighting.c`, L08 only creates `raytracer.c`). A monolithic `main.c` would make diffs unreadable. |
| `vec3.h` in `game/` not `utils/` | Fails the parameter test — `vec3_reflect` and `vec3_refract` are raytracer-specific operations. A generic `utils/` math library would not include reflection/refraction. The full `Vec3` type is course-specific. |
| No elaborate `GAME_PHASE` state machine | Raytracer has one mode: continuous rendering. No menu, no game-over, no transitions. A state machine would be over-engineering. |
| `render.c` separate from `main.c` | The per-pixel rendering loop with camera math is substantial enough (~60 lines) to warrant its own file. It changes significantly in L11 (camera) and should not clutter `main.c`. |
| `ppm.h` is header-only (not `.h/.c` split) | The PPM writer is ~15 lines; a separate `.c` file would be more boilerplate than content. `static inline` in a header is appropriate for this size. |
| `stereo.c` and `stereogram.c` are separate files (not merged into `render.c`) | Stereo and stereogram rendering are distinct algorithmic subsystems with no shared state. Merging them into `render.c` would make that file too large and conflate camera rendering with stereo post-processing. |
| Stereo/stereogram exports are offline (not real-time) | Anaglyph and autostereogram require rendering the scene twice (or extracting a depth buffer), which halves frame rate. Exporting to PPM on keypress avoids this cost during interactive use. |
| `stereogram.c` allocates temporary buffers with `malloc` | The autostereogram needs `width`-sized `int` arrays per row (union-find `same[]`) and a full `width×height` float depth buffer and RGB output buffer. Stack allocation would overflow; `malloc`+`free` is appropriate for these temporary per-export buffers. |

---

## Platform Template Adaptations

These changes are applied when copying the Platform Foundation Course template to `course/src/`:

### `build-dev.sh`

- `-lm -lpthread` in both backend link flags
- `SOURCES` list: `src/utils/draw-shapes.c src/utils/draw-text.c src/game/main.c src/game/intersect.c src/game/lighting.c src/game/raytracer.c src/game/render.c src/game/stereo.c src/game/stereogram.c src/game/voxel.c src/game/envmap.c src/game/mesh.c`
- X11 backend additionally compiles `src/utils/stb_image_impl.c` (Raylib uses internal stb_image)
- Remove `src/game/demo.c` and `src/game/audio_demo.c` from SOURCES
- Build output message: `"Building TinyRaytracer course (backend=$BACKEND)..."`

### `platform.h`

- `GAME_TITLE "TinyRaytracer"`
- `GAME_W 800`
- `GAME_H 600` (same as default, but explicit)

### `game/base.h`

- Buttons: `quit`, `export_ppm`, `export_anaglyph`, `export_stereogram`, `camera_left`, `camera_right`, `camera_up`, `camera_down`
- `BUTTON_COUNT` adjusted to match (8 buttons)
- Remove `play_tone` button (no audio demo)

### `platforms/x11/main.c`

- Key bindings: `XK_Escape` → quit, `XK_p` → export_ppm, `XK_a` → export_anaglyph, `XK_g` → export_stereogram, `XK_Left` → camera_left, `XK_Right` → camera_right, `XK_Up` → camera_up, `XK_Down` → camera_down
- Audio init calls remain but `game_get_audio_samples` produces silence (no game audio state)
- Remove `demo_render` call; replace with `game_update` + `game_render` pattern

### `platforms/raylib/main.c`

- Key bindings: `KEY_ESCAPE` → quit, `KEY_P` → export_ppm, `KEY_A` → export_anaglyph, `KEY_G` → export_stereogram, `KEY_LEFT` → camera_left, `KEY_RIGHT` → camera_right, `KEY_UP` → camera_up, `KEY_DOWN` → camera_down
- Same audio adaptation as X11
- Remove `demo_render` call; replace with `game_update` + `game_render` pattern

---

## Key Constants

| Constant | Value | Where defined | Lesson |
|----------|-------|---------------|--------|
| `GAME_W` | 800 | `platform.h` | L01 |
| `GAME_H` | 600 | `platform.h` | L01 |
| `MAX_SPHERES` | 16 | `scene.h` | L04 |
| `MAX_LIGHTS` | 8 | `scene.h` | L05 |
| `MAX_MATERIALS` | 16 | `scene.h` | L04 |
| `MAX_RECURSION_DEPTH` | 4 | `raytracer.h` | L08 |
| `CAMERA_ORBIT_SPEED` | 1.5f | `render.c` | L11 |
| `DEFAULT_FOV` | `M_PI / 3.0f` | `render.c` | L11 |
| `DEFAULT_ORBIT_RADIUS` | 5.0f | `render.c` | L11 |
| `SHADOW_EPSILON` | 1e-3f | `lighting.c` | L07 |
| `MAX_BOXES` | 8 | `scene.h` | L13 |
| `EYE_SEPARATION` | 0.065f | `stereo.h` | L14 |
| `STEREO_MU` | 0.33f | `stereogram.h` | L15 |
| `STEREOGRAM_EYE_PX` | 400 | `stereogram.h` | L15 |
| `VOXEL_W` / `VOXEL_H` / `VOXEL_D` | 6 / 6 / 4 | `voxel.h` | L16 |
| `VOXEL_SIZE` | 0.10f | `voxel.h` | L16 |
| `MAX_VOXEL_MODELS` | 4 | `scene.h` | L16 |
| `MAX_MESHES` | 4 | `scene.h` | L19 |
| `CUBEMAP_FACE_POS_X` .. `NEG_Z` | 0–5 | `envmap.h` | L20 |
| `RENDER_CPU_BASIC` / `RENDER_CPU_OPT` / `RENDER_GPU` | 0 / 1 / 2 | `settings.h` | L22 |
| `RENDER_MODE_COUNT` | 3 | `settings.h` | L22 |
| `VOXEL_OCTANTS` | 8 | `voxel.h` | L22 |
