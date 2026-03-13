# Lesson 23 — GPU Rendering Mode

> **What you'll build:** By the end of this lesson, pressing **N** twice cycles to "GPU" mode — an in-process OpenGL 3.3 fragment shader that runs the full raytracer directly on the GPU. The same ray tracing algorithm that ran on CPU pthreads now runs on every GPU core in parallel. CPU usage drops to near zero for rendering, the fan goes silent, and the scene renders at full resolution at 60fps.

## Observable outcome

Press **N** twice to cycle from CPU-BASIC → CPU-OPT → GPU. The HUD line 5 changes to:

```
GPU: Mesa Intel(R) Graphics | #330 core | full scene: 4sph 1box 1vox 1mesh
```

The scene shows the complete scene — 4 spheres (ivory, rubber, mirror, glass), checkerboard floor, box, voxel duck, and triangle mesh — all rendered entirely by the GPU. The camera responds to WASD, arrows, and mouse orbit/pan/zoom identically to the CPU modes. Feature toggles (V/F/B/M/R/T/H/E/X) and environment map cycling (C) work in GPU mode. CPU rendering is skipped completely. The fan goes quiet.

GPU mode renders the full scene with all geometry types, feature toggles, and environment maps. The backbuffer is cleared to transparent black each frame so the HUD overlay doesn't obscure the GPU scene.

## New concepts

- **gpu_shader.h** — provides TWO build functions: `gpu_build_fragment_source()` assembles the L21 `GLSL_RAYTRACER_SOURCE` with a `#version 330 core` preamble, Shadertoy-compatible uniforms, camera uniforms, and `main()` epilogue for Shadertoy export; `gpu_build_scene_fragment_source()` assembles the data-driven `gpu_scene_glsl.h` body with scene-data uniforms for full in-process rendering.
- **gpu_scene_glsl.h** — a complete data-driven GLSL raytracer body: sphere/box/voxel/mesh intersection, environment map sampling, and feature toggles driven entirely by uniform values. This replaces the hardcoded L21 scene for in-process rendering.
- **gpu_upload.h** — defines `GpuSceneData` struct and `gpu_pack_scene()` which flattens the CPU-side scene (materials, spheres, lights, boxes, voxel bitfield, mesh triangles) into flat arrays suitable for uploading as GL uniforms and textures.
- **X11 backend GPU path** — runtime OpenGL function pointer loading via `glXGetProcAddressARB`, a `GpuState` struct holding 20 GL function pointers, shader compilation/linking, fullscreen triangle rendering via `gl_VertexID` (no vertex buffer needed), and HUD overlay with alpha blending.
- **Raylib backend GPU path** — `LoadShaderFromMemory`, `BeginShaderMode`/`EndShaderMode`, `SetShaderValue` for uniforms. Much simpler than raw GL — Raylib handles all the function pointer loading, VAO creation, and uniform binding internally.

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/gpu_shader.h` | Created | `GPU_VERT_SHADER`, `GPU_FRAG_PREAMBLE`, `GPU_FRAG_EPILOGUE` macros; `gpu_build_fragment_source()` for Shadertoy export, `gpu_build_scene_fragment_source()` for in-process rendering |
| `game/gpu_scene_glsl.h` | Created | Complete data-driven GLSL raytracer: sphere/box/voxel/mesh intersection, envmap sampling, feature toggles via uniforms |
| `game/gpu_upload.h` | Created | `GpuSceneData` struct + `gpu_pack_scene()` for flat array packing of scene data into uniform-friendly format |
| `platforms/x11/main.c` | Modified | `GpuState` struct with 20 GL function pointers; `gpu_init()`, `gpu_render_scene()`, `gpu_overlay_hud()`, `gpu_shutdown()` |
| `platforms/raylib/main.c` | Modified | `RaylibGpuState` struct; `rl_gpu_init()`, `rl_gpu_render()`, `rl_gpu_overlay_hud()`, `rl_gpu_shutdown()` |

## Background — from export to in-process

### L21 exported text; L23 compiles and runs it

In Lesson 21, we stored the complete GLSL raytracer as a C string (`GLSL_RAYTRACER_SOURCE`) and exported it to a file for pasting into Shadertoy. The GPU was already there — we use it every frame to blit the backbuffer texture via `glTexSubImage2D`. Lesson 23 takes the obvious next step: compile that same GLSL string into an OpenGL shader program and render directly to the screen.

### The OpenGL shader pipeline

```
Vertex Shader  →  Rasterizer  →  Fragment Shader  →  Framebuffer
(positions)       (triangles     (per-pixel          (screen)
                   to pixels)     computation)
```

We provide:
- **Vertex shader**: a fullscreen triangle via `gl_VertexID` — 3 hardcoded vertices that cover the entire viewport after clipping
- **Fragment shader**: our `GLSL_RAYTRACER_SOURCE` wrapped with uniforms and a `main()` that calls `mainImage`

The GPU runs the fragment shader for every pixel in parallel — no pthreads needed.

### JS analogy

In WebGL2, you'd call `gl.createShader(gl.FRAGMENT_SHADER)`, `gl.shaderSource(shader, source)`, `gl.compileShader(shader)`, then link into a program. Three.js abstracts this with `ShaderMaterial({ fragmentShader: source })`. Our X11 path is the raw GL equivalent; our Raylib path is closer to the Three.js abstraction.

## Code walkthrough

### 1. gpu_shader.h — zero-duplication wrapping

The header assembles a complete OpenGL fragment shader from three pieces:

```c
#define GPU_FRAG_PREAMBLE \
  "#version 330 core\n" \
  "precision highp float;\n" \
  "uniform vec3  iResolution;\n" \
  "uniform float iTime;\n" \
  "uniform vec4  iMouse;\n" \
  "uniform vec3  iCamOrigin;\n" \
  "uniform vec3  iCamForward;\n" \
  "uniform vec3  iCamRight;\n" \
  "uniform vec3  iCamUp;\n" \
  "uniform float iCamFov;\n" \
  "out vec4 FragColor;\n\n"

/* GLSL_RAYTRACER_SOURCE goes here — unchanged from L21 */

#define GPU_FRAG_EPILOGUE \
  "\nvoid main() {\n" \
  "    vec2  ndc     = (2.0 * gl_FragCoord.xy - iResolution.xy)\n" \
  "                    / iResolution.y;\n" \
  "    float cam_fov = (iCamFov > 0.0) ? tan(iCamFov / 2.0)\n" \
  "                    : tan(FOV / 2.0);\n" \
  "    vec3  dir     = normalize(\n" \
  "        iCamRight   * (ndc.x * cam_fov) +\n" \
  "        iCamUp      * (ndc.y * cam_fov) +\n" \
  "        iCamForward);\n" \
  "    FragColor = vec4(cast_ray(iCamOrigin, dir), 1.0);\n" \
  "}\n"
```

Shadertoy provides `iResolution`, `iTime`, `iMouse` as built-in globals and calls `mainImage` automatically. Our preamble declares these as uniforms (set by the host each frame), plus camera uniforms for interactive orbit controls.

**Key design decision:** instead of calling `mainImage()` in the epilogue, we build the camera ray directly in `main()` and call `cast_ray()`. This avoids relying on GLSL preprocessor `#ifdef` directives (which can behave inconsistently across GPU drivers). The original `mainImage()` from `GLSL_RAYTRACER_SOURCE` remains as dead code — the GLSL compiler silently drops it. For Shadertoy export, `mainImage()` is still the entry point (Shadertoy calls it automatically).

The camera uniforms (`iCamOrigin/Forward/Right/Up/Fov`) mirror the CPU `CameraBasis` struct from `render.c`. The host computes the camera basis each frame via `camera_compute_basis()` and uploads it — the GPU shader uses the same orbit camera as the CPU modes.

The vertex shader uses the "fullscreen triangle" trick — no vertex buffer needed:

```c
#define GPU_VERT_SHADER \
  "#version 330 core\n" \
  "void main() {\n" \
  "    vec2 pos[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));\n" \
  "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n" \
  "}\n"
```

Three vertices at `(-1,-1)`, `(3,-1)`, `(-1,3)` form a triangle that covers the entire `[-1,1]×[-1,1]` clip space. The GPU's clip hardware trims to the viewport.

### 2. X11 backend — raw OpenGL function pointers

The X11 backend uses a legacy compat context (from `glXCreateContext`), but most drivers expose GL 2.0+ shader functions. We load them at runtime:

```c
#define LOAD_GL(name) \
  g_gpu.name = (PFN_gl##name)glXGetProcAddressARB((const GLubyte*)"gl" #name)

LOAD_GL(CreateShader);   LOAD_GL(ShaderSource);
LOAD_GL(CompileShader);  LOAD_GL(GetShaderiv);
/* ... 20 function pointers total ... */
```

The `GpuState` struct holds all pointers plus the compiled program, VAO, and uniform locations. `gpu_init()` compiles both shaders, links them, queries uniform locations, and stores the GL renderer string for the HUD.

### 3. GPU rendering — fullscreen triangle

```c
static void gpu_render_scene(float time_sec, float mouse_x, float mouse_y,
                             const RtCamera *cam, const RtScene *scene,
                             const RtSettings *settings) {
  glDisable(GL_TEXTURE_2D);          /* switch from legacy to modern */
  g_gpu.UseProgram(g_gpu.program);

  /* Set Shadertoy-compatible uniforms */
  g_gpu.Uniform3f(g_gpu.u_resolution, w, h, 1.0f);
  g_gpu.Uniform1f(g_gpu.u_time, time_sec);
  g_gpu.Uniform4f(g_gpu.u_mouse, mouse_x, mouse_y, 0.0f, 0.0f);

  /* Set camera uniforms from CPU orbit camera state */
  CameraBasis cb = camera_compute_basis(cam, (int)w, (int)h);
  g_gpu.Uniform3f(g_gpu.u_cam_origin,  cb.origin.x,  cb.origin.y,  cb.origin.z);
  g_gpu.Uniform3f(g_gpu.u_cam_forward, cb.forward.x, cb.forward.y, cb.forward.z);
  g_gpu.Uniform3f(g_gpu.u_cam_right,   cb.right.x,   cb.right.y,   cb.right.z);
  g_gpu.Uniform3f(g_gpu.u_cam_up,      cb.up.x,      cb.up.y,      cb.up.z);
  g_gpu.Uniform1f(g_gpu.u_cam_fov,     cam->fov);

  /* Draw fullscreen triangle — fragment shader runs for every pixel */
  g_gpu.BindVertexArray(g_gpu.vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  g_gpu.BindVertexArray(0);

  g_gpu.UseProgram(0);               /* restore legacy state */
  glEnable(GL_TEXTURE_2D);
}
```

### 4. HUD overlay with alpha blending

After GPU rendering, the HUD (drawn into the CPU backbuffer by `game_render`) is overlaid via alpha-blended texture:

```c
static void gpu_overlay_hud(Backbuffer *bb) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  /* Upload HUD pixels, draw textured quad on top of GPU scene */
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bb->width, bb->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);
  /* ... draw quad ... */
  glDisable(GL_BLEND);
}
```

### 5. Raylib backend — much simpler

Raylib abstracts all the GL complexity:

```c
g_rl_gpu.shader = LoadShaderFromMemory(GPU_VERT_SHADER, frag_src);
g_rl_gpu.u_resolution = GetShaderLocation(g_rl_gpu.shader, "iResolution");

/* Each frame: */
SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_resolution, res, SHADER_UNIFORM_VEC3);
BeginShaderMode(g_rl_gpu.shader);
DrawRectangle(0, 0, w, h, WHITE);  /* Raylib applies shader to this */
EndShaderMode();
```

No function pointer loading, no VAO management, no compile/link error handling — Raylib does it all internally.

### 6. Interactive camera via uniforms

The L21 GLSL shader hardcoded the camera at the origin looking down -Z. For in-process rendering, we need the GPU to use the same interactive orbit camera as the CPU modes. The solution: upload the CPU `CameraBasis` as uniforms each frame, and construct the ray in the epilogue's `main()`.

The epilogue bypasses `mainImage()` entirely and calls `cast_ray()` directly:

```glsl
void main() {
    vec2  ndc     = (2.0 * gl_FragCoord.xy - iResolution.xy)
                    / iResolution.y;
    float cam_fov = (iCamFov > 0.0) ? tan(iCamFov / 2.0)
                    : tan(FOV / 2.0);
    vec3  dir     = normalize(
        iCamRight   * (ndc.x * cam_fov) +
        iCamUp      * (ndc.y * cam_fov) +
        iCamForward);
    FragColor = vec4(cast_ray(iCamOrigin, dir), 1.0);
}
```

Why not use `#ifdef` inside `mainImage()`? GLSL preprocessor behavior for `#define`/`#ifdef` can be inconsistent across GPU drivers (Mesa, NVIDIA, AMD). By putting the camera code directly in `main()`, we avoid preprocessor issues entirely. `mainImage()` from `GLSL_RAYTRACER_SOURCE` becomes dead code in the assembled shader — the GLSL compiler silently drops it. For Shadertoy export, `mainImage()` remains the entry point with the original fixed camera.

The host calls `camera_compute_basis()` (the same function used by CPU rendering) and uploads the result as 5 uniforms (`iCamOrigin`, `iCamForward`, `iCamRight`, `iCamUp`, `iCamFov`). WASD, arrow keys, mouse orbit/pan/zoom all work identically in GPU mode.

### 7. HUD toggle (F1)

The HUD overlay can now be toggled with F1 via `state->show_hud`. In GPU mode, `game_render()` still draws the HUD into the CPU backbuffer, but the platform layer only overlays it if the HUD is visible. This lets you get a clean fullscreen view of the GPU-rendered scene.

### 8. Main loop branching

Both backends branch in the main loop:

```c
if (state.settings.render_mode == RENDER_GPU && g_gpu.ready) {
  gpu_render_scene(elapsed_time, curr_input->mouse.x, curr_input->mouse.y,
                   &state.camera, &state.scene, &state.settings);
  gpu_overlay_hud(&bb);
} else {
  display_backbuffer(&bb);
}
```

In GPU mode, `game_render` still runs (it draws the HUD into the backbuffer) but `render_scene` is skipped — no CPU raytracing at all.

## GPU mode limitations

The GPU shader now renders the full scene. Camera controls, feature toggles (V/F/B/M/R/T/H/E/X), envmap modes (C), and all geometry types work in GPU mode.

**Backbuffer clearing:** In GPU mode, the backbuffer is cleared to transparent black (`rgba(0,0,0,0)`) each frame before drawing the HUD. Without this, the stale CPU-rendered pixels (with `alpha=255` from `GAME_RGB`) would completely cover the GPU scene during the HUD overlay's alpha-blend pass.

**Iterative vs recursive cast_ray:** The GPU uses an iterative `cast_ray` loop (`MAX_DEPTH=4`) instead of recursion (GLSL does not support true recursion). After exhausting all bounce iterations (e.g., inside a mirror sphere), remaining weight is added as sky/envmap color to match the CPU's recursive termination behavior.

## Key takeaways

1. **Two shader paths, one codebase** — `gpu_build_fragment_source()` wraps the L21 `GLSL_RAYTRACER_SOURCE` for Shadertoy export; `gpu_build_scene_fragment_source()` assembles `gpu_scene_glsl.h` (the data-driven replacement) for full in-process rendering with all geometry types
2. **Uniform-driven scene data** — materials, spheres, lights, and boxes are uploaded as uniform arrays; the voxel bitfield is packed into `uint` uniforms; mesh triangles are uploaded as an RGBA32F texture. This lets the GPU shader render the exact same scene as the CPU without hardcoding any geometry
3. **The GPU was always there** — we already used it for texture upload (`glTexSubImage2D`). Adding shader compilation is a natural extension
4. **Camera uniforms bridge CPU and GPU** — the same `camera_compute_basis()` drives both CPU ray generation and GPU uniform uploads, keeping the interactive experience identical across all three modes
5. **X11 vs Raylib** highlights the abstraction gap — raw GL needs 20 function pointers and manual error handling; Raylib wraps it in 3 function calls
6. **The three-mode toggle** (CPU-BASIC → CPU-OPT → GPU) provides a complete educational comparison: baseline vs optimized CPU vs GPU, with the HUD (F1 toggleable) showing implementation details for each
