# Building a Raytracer in C

Build a real-time CPU raytracer from scratch, rendered to a platform backbuffer at 60fps. Based on ssloy's ["Understandable Raytracing in 256 lines of bare C++"](https://github.com/ssloy/tinyraytracer/wiki), ported to modern C with interactive camera controls, ray-box intersection, anaglyph stereo rendering, and autostereogram generation.

---

## What You'll Build

A complete raytracer that renders a scene with 4 spheres (ivory, red rubber, mirror, glass), a cube, a voxel bunny, a checkerboard floor, and 3 point lights. The renderer implements:

- **Phong reflection model** — diffuse + specular lighting
- **Hard shadows** — shadow rays with epsilon offset
- **Recursive reflections** — mirror sphere reflects the entire scene (depth 4)
- **Refractions** — glass sphere bends light via Snell's law
- **Checkerboard floor** — procedural pattern via ray-plane intersection
- **Ray-box intersection** — axis-aligned cube with per-face intersection (Part 3)
- **Voxel rendering** — bitfield-packed bunny model with per-voxel palette colors (Part 3)
- **Interactive camera** — arrow keys orbit around the scene
- **PPM export** — P key saves the current frame to disk
- **Anaglyph stereo** — A key exports red/cyan 3D image (Part 2)
- **Side-by-side stereo** — S key exports stereoscope/VR image (Part 2)
- **Autostereogram** — G key exports random-dot SIRDS with union-find (Part 2)

Both **X11/GLX** and **Raylib** backends are supported.

---

## Prerequisites

**Required — complete before starting:**

- **Platform Foundation Course** (`ai-llm-knowledge-dump/generated-courses/platform-backend/`) — this course copies the platform template and assumes familiarity with the backbuffer, input system, and build scripts
- C syntax: variables, loops, functions, structs, pointers, typedefs
- Basic linear algebra intuition: vectors, dot product, what "perpendicular" and "reflection" mean geometrically

**Helpful but not required:**

- Trigonometry basics (`sin`, `cos`) — used for camera orbit; explained when introduced
- Familiarity with PPM image format — explained in Lesson 02
- Having read ssloy's original tutorial — not required but provides good context

No prior graphics programming or raytracing experience required. Every algorithm is derived from first principles.

---

## Final File Tree

```
course/
├── build-dev.sh                   ← --backend=x11|raylib, -r, -d/--debug
└── src/
    ├── utils/                     [copied from Platform Foundation Course]
    │   ├── math.h, backbuffer.h
    │   ├── draw-shapes.h/.c, draw-text.h/.c
    │   └── audio.h               (present but unused)
    ├── game/
    │   ├── base.h                 ← GameInput with camera + export buttons
    │   ├── main.h                 ← RaytracerState, Camera
    │   ├── main.c                 ← game_init, game_update, game_render
    │   ├── vec3.h                 ← Vec3 + all vector operations
    │   ├── ray.h                  ← Ray, HitRecord
    │   ├── scene.h                ← Material, Sphere, Light, Scene
    │   ├── intersect.h/.c         ← sphere/plane/box intersection, nearest-hit
    │   ├── lighting.h/.c          ← Phong + shadows
    │   ├── refract.h              ← Snell's law
    │   ├── raytracer.h/.c         ← recursive cast_ray
    │   ├── render.h/.c            ← per-pixel loop + Camera
    │   ├── ppm.h                  ← PPM P6 writer
    │   ├── envmap.h/.c             ← environment mapping (equirect + cubemap + procedural)
    │   ├── mesh.h/.c               ← triangle mesh loading + intersection
    │   ├── stereo.h/.c            ← anaglyph + side-by-side stereo
    │   ├── stereogram.h/.c        ← depth buffer + union-find + SIRDS
    │   ├── voxel.h/.c             ← bunny bitfield + voxel model intersection
    │   └── shader_glsl.h          ← GLSL Shadertoy shader as C string
    ├── platforms/
    │   ├── x11/                   [adapted key bindings]
    │   └── raylib/                [adapted key bindings]
    └── platform.h                 ← GAME_TITLE "TinyRaytracer", 800×600
```

---

## Lesson List

| #   | Title                        | What you build                                    | What you see                                                          |
| --- | ---------------------------- | ------------------------------------------------- | --------------------------------------------------------------------- |
| 01  | Vec3 Math + Gradient Canvas  | `Vec3` struct + basic ops; gradient render         | Color gradient fills the window                                       |
| 02  | Vec3 Advanced + PPM Export   | `vec3_dot`/`length`/`normalize`; PPM writer        | Gradient saved as `out.ppm`                                           |
| 03  | Ray-Sphere Intersection      | `Ray`, `HitRecord`; geometric intersection         | Single sphere on solid blue background                                |
| 04  | Multiple Spheres + Materials | `Material`, `Scene`; nearest-hit traversal         | Four colored spheres visible                                          |
| 05  | Diffuse Lighting             | `Light` struct; Lambertian shading                 | Spheres with realistic light/dark shading                             |
| 06  | Specular Highlights          | `vec3_reflect`; Phong specular                     | Shiny highlights on ivory; rubber stays matte                         |
| 07  | Hard Shadows                 | Shadow rays; epsilon offset                        | Dark shadow regions where light is blocked                            |
| 08  | Reflections                  | Recursive `cast_ray`; material blending            | Mirror sphere reflects the entire scene                               |
| 09  | Refractions                  | Snell's law; inside/outside detection              | Glass sphere bends light; distorted view through it                   |
| 10  | Checkerboard Floor           | Ray-plane intersection; procedural pattern         | Alternating checkerboard extends to horizon                           |
| 11  | Interactive Camera           | `Camera` struct; orbit math; ray generation        | Arrow keys orbit the camera around the scene                          |
| 12  | Polish + HUD + PPM Snapshot  | HUD overlay; PPM export on P key                   | Text overlay with controls; P key saves `out.ppm`                     |
| 13  | Ray-Box Intersection         | `Box` struct; per-face AABB intersection           | Lit, shadowed cube appears alongside spheres                          |
| 14  | Anaglyph + Side-by-Side Stereo | Dual-camera render; red/cyan + side-by-side       | A/S keys export `anaglyph.ppm` + `sidebyside.ppm`                    |
| 15  | Autostereogram               | Depth buffer; union-find; random-dot SIRDS         | G key exports `stereogram.ppm`; wall-eyed 3D                         |
| 16  | Voxel Rendering              | Bunny bitfield; per-cell box intersection          | Colorful voxel bunny appears in scene with full lighting              |
| 17  | Beyond — Modern Techniques   | Capstone roadmap + GLSL shader reference            | 10 modern techniques + complete Shadertoy shader code                 |
| 18  | Environment Mapping          | stb_image; spherical UV; procedural sky             | Background shows envmap image or gradient sky                         |
| 19  | Triangle Meshes              | Moller-Trumbore; fast_obj .obj loading              | Icosahedron in scene; load any .obj file                              |
| 20  | Cube Map Textures            | 6-face cube map; direction-to-face UV; C key cycle  | Cube map background; C key cycles sky/eq/cube modes                   |
| 21  | GLSL Fragment Shaders        | Shadertoy GLSL raytracer; Shift+L export            | Shift+L writes `raytracer.glsl`; paste into shadertoy.com             |

---

## Build Instructions

### Raylib backend

```bash
sudo apt install libraylib-dev   # Ubuntu 22.04+
cd course/
chmod +x build-dev.sh
./build-dev.sh --backend=raylib -r
```

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

### Export PPM snapshots

While the program is running:

- **P** — save standard raytraced image to `out.ppm`
- **Shift+A** — save anaglyph stereo image to `anaglyph.ppm` (view with red/cyan 3D glasses)
- **Shift+S** — save side-by-side stereo image to `sidebyside.ppm` (view in VR headset or cross-eyed)
- **G** — save wall-eyed autostereogram to `stereogram.ppm` (diverge eyes past screen)
- **Shift+G** — save cross-eyed autostereogram to `stereogram_cross.ppm` (cross eyes in front of screen)
- **Shift+L** — export GLSL Shadertoy shader to `raytracer.glsl` (paste into shadertoy.com)
- **C** — cycle environment map mode: procedural sky → equirectangular → cube map

Open exported files with:

```bash
feh out.ppm        # or any image viewer
display out.ppm    # ImageMagick
```

---

## Architecture Overview

```
Platform layer (window, input, timing, GPU upload)
        ↓ GameInput (arrow keys, P/A/G, Esc), delta_time
   game_update()   →   Camera orbit, export triggers (PPM/anaglyph/stereogram)
        ↓
   game_render()   →   render_scene() per-pixel raytracing → Backbuffer
        ↓
Platform layer (upload backbuffer to GPU, swap buffers)
```

The game layer (`game/`) traces rays and writes pixels. It never calls X11 or Raylib directly. The platform layer (`platforms/*/`) handles windowing, input, and display. `platform.h` defines the boundary.

---

## Raytracing Pipeline (per pixel)

```
pixel (i, j)
    → compute ray direction (NDC + FOV + camera rotation)
    → cast_ray(ray, scene, depth=0)
        → scene_intersect(ray, scene) → find nearest sphere/plane hit
        → if no hit → return background color
        → compute_lighting(hit, scene) → diffuse + specular + shadows
        → if reflective → cast_ray(reflect_ray, depth+1) → reflect_color
        → if refractive → cast_ray(refract_ray, depth+1) → refract_color
        → blend: diffuse*a[0] + specular*a[1] + reflect*a[2] + refract*a[3]
    → GAME_RGB(r, g, b) → write to backbuffer
```

---

## Source Material

This course draws from all three parts of ssloy's wiki tutorial:

- [Part 1: Understandable Raytracing](https://github.com/ssloy/tinyraytracer/wiki/Part-1:-understandable-raytracing) — 10 algorithmic steps from gradient to checkerboard (Lessons 01–12)
- [Part 2: Low Budget Stereo Rendering](https://github.com/ssloy/tinyraytracer/wiki/Part-2:-low-budget-stereo-rendering) — anaglyph, stereoscope, autostereogram (Lessons 14–15)
- [Part 3: Shadertoy](https://github.com/ssloy/tinyraytracer/wiki/Part-3:-shadertoy) — ray-box intersection, voxel rendering ported to C (Lessons 13, 16); complete GLSL shader (Lesson 21)
- [Source repository](https://github.com/ssloy/tinyraytracer) — original C++ implementation

**Out of scope:** stereoscope barrel distortion (Part 2).

---

## Reference Implementations

- `ai-llm-knowledge-dump/generated-courses/platform-backend/` — Platform Foundation Course (template source)
- `ai-llm-knowledge-dump/prompt/course-builder.md` — course-building guidelines
- `ai-llm-knowledge-dump/modern-c-programming-safe,-performant,-and-practical-practices.md` — C code standards
