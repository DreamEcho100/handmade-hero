# Building a Raytracer in C — PLAN-TRACKER.md

Progress log. Updated after every file is created or lesson is written.

---

## Phase 1 — Planning files

| Task | Status | Notes |
|------|--------|-------|
| `PLAN.md` | ✅ Done | 16-lesson table (L01–L12 core raytracing + L13 box + L14 stereo + L15 autostereogram + L16 voxel), topic inventory, dependency map, skill table, file structure, deviation notes, JS→C mapping |
| `README.md` | ✅ Done | Course overview, prerequisites, lesson list, build instructions |
| `PLAN-TRACKER.md` | ✅ Done | This file |

---

## Phase 2 — Source files

Acceptance criteria for all source files: compiles clean with `-Wall -Wextra` (both backends); no sanitizer errors under `-fsanitize=address,undefined`.

**Note:** `Material` and `Camera` renamed to `RtMaterial` and `RtCamera` to avoid Raylib header conflicts. PLAN.md topic inventory uses original names for clarity; source code uses prefixed names.

| File | Created in | Status | Acceptance criteria |
|------|-----------|--------|---------------------|
| `course/build-dev.sh` | L01 | ✅ Done | Both backends produce `./build/game` with 0 errors; `-r` runs; `-lm` linked; audio.c stripped from SOURCES |
| `course/src/utils/math.h` | Template | ✅ Done | Copied from platform template; `CLAMP`, `MIN`, `MAX`, `ABS` macros present |
| `course/src/utils/backbuffer.h` | Template | ✅ Done | Copied from platform template; `Backbuffer` struct, `GAME_RGB`/`GAME_RGBA`, color constants |
| `course/src/utils/draw-shapes.h` | Template | ✅ Done | Copied from platform template |
| `course/src/utils/draw-shapes.c` | Template | ✅ Done | Copied from platform template |
| `course/src/utils/draw-text.h` | Template | ✅ Done | Copied from platform template |
| `course/src/utils/draw-text.c` | Template | ✅ Done | Copied from platform template |
| `course/src/utils/audio.h` | Template | ✅ Done | Copied from platform template; present but unused |
| `course/src/utils/audio-helpers.h` | Template | ✅ Done | Copied from platform template; present but unused |
| `course/src/game/base.h` | L01 | ✅ Done | 8 buttons (quit, export_ppm, export_anaglyph, export_stereogram, camera_left/right/up/down); `play_tone` removed |
| `course/src/game/main.h` | L01 | ✅ Done | `RaytracerState` struct with `Scene`, `RtCamera`, export flags; function declarations |
| `course/src/game/main.c` | L01 | ✅ Done | `game_init`/`game_update`/`game_render`; HUD overlay; PPM/anaglyph/stereogram export triggers |
| `course/src/game/vec3.h` | L01 | ✅ Done | `Vec3` + all ops: make, add, sub, scale, negate, mul, dot, length, normalize, reflect, cross, lerp |
| `course/src/game/ppm.h` | L02 | ✅ Done | PPM P6 writer (`ppm_export` from backbuffer, `ppm_export_rgb` from raw bytes); `float_to_byte` helper |
| `course/src/game/ray.h` | L03 | ✅ Done | `Ray`, `HitRecord` structs; `ray_at` inline |
| `course/src/game/intersect.h` | L03 | ✅ Done | `sphere_intersect`, `plane_intersect`, `box_intersect`, `scene_intersect` declarations |
| `course/src/game/intersect.c` | L03 | ✅ Done | Geometric ray-sphere; ray-plane (y=0); per-face ray-AABB; nearest-hit traversal (spheres + boxes + floor) |
| `course/src/game/scene.h` | L04 | ✅ Done | `RtMaterial`, `Sphere`, `Light`, `Box`, `Scene` structs; `scene_init` (4 spheres, 3 lights, 1 box, 5 materials) |
| `course/src/game/lighting.h` | L05 | ✅ Done | `LightingResult` struct; `compute_lighting` declaration |
| `course/src/game/lighting.c` | L05 | ✅ Done | Diffuse + specular + shadow rays with epsilon offset |
| `course/src/game/refract.h` | L09 | ✅ Done | Snell's law `refract()` function; inside/outside detection; total internal reflection check |
| `course/src/game/raytracer.h` | L08 | ✅ Done | `cast_ray` declaration; `MAX_RECURSION_DEPTH = 4` |
| `course/src/game/raytracer.c` | L08 | ✅ Done | Recursive cast_ray: background color, lighting, reflections, refractions, checkerboard floor, material blending |
| `course/src/game/render.h` | L11 | ✅ Done | `RtCamera` struct; `render_scene`, `render_scene_to_float`, `camera_init`, `camera_update` declarations |
| `course/src/game/render.c` | L11 | ✅ Done | Per-pixel ray generation with camera rotation; camera orbit math (sin/cos yaw/pitch) |
| `course/src/platform.h` | L01 | ✅ Done | `GAME_TITLE "TinyRaytracer"`, `GAME_W 800`, `GAME_H 600` |
| `course/src/platforms/x11/base.h` | Template | ✅ Done | Copied from platform template |
| `course/src/platforms/x11/base.c` | Template | ✅ Done | Copied from platform template |
| `course/src/platforms/x11/main.c` | L01 | ✅ Done | Audio stripped; key bindings: arrows/P/A/G/Esc; `game_update`/`game_render` calls; frame timing + letterbox |
| `course/src/platforms/x11/audio.c` | Template | ✅ Done | Present in tree but NOT compiled (stripped from build-dev.sh SOURCES) |
| `course/src/platforms/x11/audio.h` | Template | ✅ Done | Present in tree but not used |
| `course/src/platforms/raylib/main.c` | L01 | ✅ Done | Audio stripped; key bindings: arrows/P/A/G/Esc/Q; `game_update`/`game_render` calls; letterbox |
| `course/src/game/stereo.h` | L14 | ✅ Done | `render_anaglyph` + `render_side_by_side` + `render_eye_cropped`; parallax concepts; `EYE_SEPARATION`, `STEREO_OVERSIZE_FRAC` |
| `course/src/game/stereo.c` | L14 | ✅ Done | Wider-then-crop off-axis stereo (ssloy technique); grayscale anaglyph (left=red, right=cyan); side-by-side stereoscope |
| `course/src/game/stereogram.h` | L15 | ✅ Done | `render_autostereogram` (wall-eyed) + `render_autostereogram_crosseyed`; `STEREO_MU`, `STEREOGRAM_EYE_PX` |
| `course/src/game/stereogram.c` | L15 | ✅ Done | Depth buffer; `parallax()`; `uf_find`/`uf_union`; wall-eyed + cross-eyed variants via `render_sirds_internal(cross_eyed)` |
| `course/src/game/voxel.h` | L16 | ✅ Done | `VoxelModel`; `BUNNY_BITFIELD[5]` with ASCII art diagram; `voxel_is_solid()`; `voxel_color_from_id()` palette hash |
| `course/src/game/voxel.c` | L16 | ✅ Done | `voxel_model_intersect()` — triple nested loop, per-cell Box construction, nearest voxel hit |
| `course/src/game/envmap.h` | L18 | ✅ Done | `EnvMap` struct, `envmap_load/free/sample`, procedural sky fallback, spherical UV mapping |
| `course/src/game/envmap.c` | L18 | ✅ Done | stb_image integration, procedural sky gradient, equirectangular direction→UV |
| `course/src/game/mesh.h` | L19 | ✅ Done | `Triangle`, `TriMesh` structs, `triangle_intersect`, `mesh_intersect`, `mesh_load_obj`, `mesh_create_icosahedron` |
| `course/src/game/mesh.c` | L19 | ✅ Done | Moller-Trumbore algorithm, fast_obj .obj loading with triangulation, AABB early-out, procedural icosahedron |
| `course/src/utils/stb_image.h` | L18 | ✅ Done | stb_image v2.30 single-header library; declarations included by envmap.c |
| `course/src/utils/stb_image_impl.c` | L18 | ✅ Done | `STB_IMAGE_IMPLEMENTATION` compile unit; compiled only for X11 backend (Raylib uses its internal copy) |
| `course/src/utils/fast_obj.h` | L19 | ✅ Done | fast_obj v1.3; single-header .obj loader; compiled via FAST_OBJ_IMPLEMENTATION in mesh.c |
| `course/src/game/shader_glsl.h` | L21 | ✅ Done | Complete GLSL Shadertoy raytracer as C string constant; `shader_export_glsl()` writes to file |
| **Build verification (Phase 2)** | — | ✅ Done | `--backend=raylib`, `--backend=x11`, `--backend=raylib -d`, `--backend=x11 -d` — all 4 pass with 0 errors |

---

## Phase 3 — Lesson files

| Task | Status | Notes |
|------|--------|-------|
| `lessons/lesson-01-vec3-math-gradient-canvas.md` | ✅ Done | Vec3 struct, basic ops, gradient render |
| `lessons/lesson-02-vec3-advanced-ppm-export.md` | ✅ Done | dot/length/normalize, PPM P6 writer |
| `lessons/lesson-03-ray-sphere-intersection.md` | ✅ Done | Ray/HitRecord, geometric intersection, NDC |
| `lessons/lesson-04-multiple-spheres-materials.md` | ✅ Done | RtMaterial, Scene, nearest-hit traversal |
| `lessons/lesson-05-diffuse-lighting.md` | ✅ Done | Light struct, Lambertian diffuse |
| `lessons/lesson-06-specular-highlights.md` | ✅ Done | vec3_reflect, Phong specular |
| `lessons/lesson-07-hard-shadows.md` | ✅ Done | Shadow ray, epsilon offset, occlusion |
| `lessons/lesson-08-reflections.md` | ✅ Done | Recursive cast_ray, MAX_RECURSION_DEPTH=3, material blending |
| `lessons/lesson-09-refractions.md` | ✅ Done | Snell's law, inside/outside, albedo[3] |
| `lessons/lesson-10-checkerboard-floor.md` | ✅ Done | Plane intersection, procedural pattern |
| `lessons/lesson-11-interactive-camera-mouse-threading.md` | ✅ Done | RtCamera, MouseState, CameraBasis, pthreads multi-threaded rendering |
| `lessons/lesson-12-settings-toggles-aa-hud.md` | ✅ Done | RenderSettings, feature toggles, 2×2 SSAA, adaptive quality, HUD with FPS/ms/rays, signature refactor |
| `lessons/lesson-13-ray-box-intersection.md` | ✅ Done | Box struct, per-face AABB intersection, cube in scene |
| `lessons/lesson-14-anaglyph-sidebyside-stereo.md` | ✅ Done | Parallax concepts, wider-then-crop, anaglyph red/cyan, side-by-side |
| `lessons/lesson-15-autostereogram.md` | ✅ Done | Depth buffer, union-find, SIRDS, wall-eyed + cross-eyed |
| `lessons/lesson-16-voxel-rendering.md` | ✅ Done | Bunny bitfield, VoxelModel, per-cell box intersection, palette colors |
| `lessons/lesson-17-modern-rendering-techniques.md` | ✅ Done | 10 techniques + GLSL Shadertoy shader reference |
| `lessons/lesson-18-environment-mapping.md` | ✅ Done | stb_image, spherical UV, procedural sky, EnvMap |
| `lessons/lesson-19-triangle-meshes.md` | ✅ Done | Moller-Trumbore, fast_obj .obj loading, icosahedron |
| `lessons/lesson-20-cube-map-textures.md` | ✅ Done | CubeMapFace, direction-to-face UV, 6-face loading, C key mode cycling |
| `lessons/lesson-21-glsl-fragment-shaders.md` | ✅ Done | GLSL string constant, Shift+L export, C-to-GLSL mapping, iterative reflection |

---

## Phase 4 — Integration verification

| Verification gate | Status | Acceptance criteria |
|------------------|--------|---------------------|
| Visual output — both backends | [ ] | Builds pass; 4 spheres + 1 cube + checkerboard + shadows + reflections + refractions visible |
| Input — both backends | [ ] | Arrow keys orbit camera; P/A/G export PPMs; Esc quits |
| PPM export | [ ] | `out.ppm` opens in image viewer; contains raytraced scene |
| Box intersection | [ ] | Cube visible in scene; properly lit, shadowed, reflected |
| Anaglyph export | [ ] | `anaglyph.ppm` shows red/cyan stereo image; depth visible with 3D glasses |
| Side-by-side export | [ ] | `sidebyside.ppm` shows left+right eye views side by side; viewable in VR or cross-eyed |
| Autostereogram export | [ ] | `stereogram.ppm` shows random-dot pattern; wall-eyed viewing reveals 3D geometry |
| Voxel bunny | [ ] | Voxelized bunny visible in scene with per-voxel colors; lit, shadowed, reflected |
| Reflections | [ ] | Mirror sphere shows reflected image of other spheres |
| Refractions | [ ] | Glass sphere bends light; scene visible through it distorted |
| Shadows | [ ] | Dark regions visible where spheres block light |
| Camera orbit | [ ] | Arrow keys smoothly rotate view; scene re-renders from new angle |
| HUD overlay | [ ] | Text overlay visible with controls and camera state |
| Clean build — `-Wall -Wextra` | ✅ Done | All 4 builds (`raylib`, `x11`, `raylib -d`, `x11 -d`) pass with 0 errors/warnings |
| Frame rate | [ ] | Interactive frame rate (>10fps at 800×600) on both backends |
