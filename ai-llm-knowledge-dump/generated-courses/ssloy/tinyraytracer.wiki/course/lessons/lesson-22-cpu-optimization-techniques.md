# Lesson 22 — CPU Optimization Techniques

> **What you'll build:** By the end of this lesson, pressing **N** cycles to "CPU-OPT" mode — a suite of three CPU-side optimizations that dramatically reduce CPU usage and fan noise: frame rate capping (nanosleep), bilinear interpolation upscale (smooth edges at reduced resolution), and octant-BVH voxel acceleration (skip empty space before testing individual cells). The HUD updates to show per-mode implementation details so you can compare approaches side by side.

## Observable outcome

Press **N** to cycle from CPU-BASIC to CPU-OPT. Three changes are immediately visible:

1. **CPU usage drops** — the frame rate cap sleeps away unused time instead of spinning the CPU at 100%
2. **Smoother edges at scale>1** — bilinear interpolation replaces blocky pixel duplication with smooth 4-pixel blending
3. **HUD line 5** changes from `voxel:brute-force` to `voxel:octant-BVH | cap:30fps`

Press **N** again to cycle back to CPU-BASIC (or forward to GPU in L23). The three modes are designed for A/B comparison — you can see and hear the difference.

The HUD now displays all available controls across 7 lines: metrics, feature toggle states, camera info, mode-specific details, keyboard controls, toggle key reference, and mouse/export controls. Press **F1** to toggle the entire HUD on/off for an unobstructed view.

## New concepts

- **Frame rate capping** with `nanosleep` — sleep unused frame budget to reduce CPU utilization from ~100% to ~50% (target 30fps for CPU-OPT)
- **Bilinear interpolation upscale** — render at reduced resolution into a `Vec3` float buffer, then smooth 4-pixel blend to full resolution (vs the block-fill approach in CPU-BASIC)
- **Octant-BVH acceleration** — partition the voxel grid into 8 sub-AABBs at the model center, skip empty octants before per-cell box tests (~50% fewer intersection tests)

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/settings.h` | Modified | `RenderMode` enum (`CPU_BASIC`, `CPU_OPT`, `GPU`), `render_mode` field in `RenderSettings` |
| `game/base.h` | Modified | `cycle_render_mode` + `toggle_hud` buttons added, `all[28]` |
| `game/main.h` | Modified | GPU availability fields, `show_hud` flag, enhanced metrics (`ray_count_k`, `rays_per_sec_m`, `cpu_util_pct`, `thread_count`) |
| `game/main.c` | Modified | Mode cycling (N key), F1 HUD toggle, frame rate cap in CPU-OPT, 7-line HUD with per-mode details and full controls listing |
| `game/render.h` | Modified | `render_scene_to_float_mt()` and `bilinear_upscale()` declarations |
| `game/render.c` | Modified | Float buffer rendering, bilinear upscale function, CPU-OPT branch in `render_scene` |
| `game/voxel.h` | Modified | `VoxelOctant` struct, `octants[8]` in `VoxelModel`, `voxel_model_intersect_bvh()` declaration |
| `game/voxel.c` | Modified | Octant partitioning in `voxel_model_init()`, `voxel_model_intersect_bvh()` function |
| `game/intersect.c` | Modified | Branch: CPU-OPT uses octant-BVH, CPU-BASIC uses brute-force |
| Both backends | Modified | N key binding for `cycle_render_mode`, F1 for `toggle_hud` |

## Background — why CPU raytracing is expensive

### The problem

Our raytracer does `width × height × aa_samples` ray computations per frame. At 800×600 with 4× AA, that's 1.92 million rays — each one doing sphere/box/plane intersections, shadow rays, reflection bounces, and Phong lighting. Even with pthreads, the CPU runs at 100% utilization and the fan spins up.

### JS analogy

In JavaScript, `requestAnimationFrame` naturally throttles to the display refresh rate. A CPU raytracer in WebGL or Web Workers would have the same problem — you'd use `setTimeout` or `requestAnimationFrame` to cap the frame rate and yield CPU time back to the OS, just like our `nanosleep` approach.

For bilinear upscale, WebGL's `gl.TEXTURE_MAG_FILTER = gl.LINEAR` does this in hardware. Our float-buffer approach is the CPU equivalent.

For BVH, Three.js uses `MeshBVH` to accelerate raycasting — same idea of spatial partitioning, just more sophisticated (full binary tree vs our 8-way octant split).

## Code walkthrough

### 1. Render mode plumbing

The `RenderMode` enum in `settings.h` defines three modes:

```c
typedef enum {
  RENDER_CPU_BASIC = 0,   /* L01-L21: original CPU raytracer (baseline)    */
  RENDER_CPU_OPT,         /* L22: frame cap + bilinear upscale + octant BVH */
  RENDER_GPU,             /* L23: OpenGL fragment shader (Shadertoy GLSL)   */
  RENDER_MODE_COUNT
} RenderMode;
```

The N key cycles through modes in `game_update()`, skipping GPU if the shader failed to compile:

```c
if (button_just_pressed(input->buttons.cycle_render_mode)) {
  int next = ((int)state->settings.render_mode + 1) % RENDER_MODE_COUNT;
  if (next == RENDER_GPU && !state->gpu_available)
    next = (next + 1) % RENDER_MODE_COUNT;
  state->settings.render_mode = (RenderMode)next;
}
```

### 2. HUD toggle (F1)

A new `toggle_hud` button in `base.h` (bumping `all[28]`) lets the user press F1 to hide the entire HUD overlay for an unobstructed view:

```c
/* In game_update(): */
if (button_just_pressed(input->buttons.toggle_hud))
  state->show_hud ^= 1;

/* In game_render(): */
if (state->show_hud) {
  /* draw 7-line HUD ... */
}
```

The HUD now shows all available controls in 7 lines: performance metrics (line 1), feature toggle states (line 2), camera + export info (line 3), mode-specific details (line 4), keyboard navigation controls (line 5), toggle key reference (line 6), and mouse/export controls (line 7).

### 3. Frame rate capping with nanosleep

After `render_scene` completes, CPU-OPT mode sleeps unused time to target ~30fps:

```c
if (mode == RENDER_CPU_OPT) {
  float target_ms = 33.3f;
  float sleep_ms = target_ms - work_ms;
  if (sleep_ms > 1.0f) {
    struct timespec ts = { 0, (long)(sleep_ms * 1e6f) };
    nanosleep(&ts, NULL);
  }
}
```

**Why 30fps?** At scale=2 with AA, rendering takes ~15ms. Without the cap, the CPU immediately starts the next frame, spinning at 100%. With the cap, it sleeps ~18ms per frame, cutting CPU utilization roughly in half. 30fps is still smooth enough for interactive exploration.

**Why nanosleep, not busy-wait?** `nanosleep` yields the CPU to other processes. A busy-wait loop (`while (elapsed < target) {}`) would still consume 100% CPU — defeating the purpose entirely.

### 4. Bilinear interpolation upscale

At `render_scale > 1`, CPU-BASIC duplicates each pixel into an NxN block ("block-fill"). This is fast but produces visible blocky edges. CPU-OPT instead renders into a `Vec3` float buffer at reduced resolution, then bilinear-interpolates to full resolution:

```c
void bilinear_upscale(const Vec3 *src, int sw, int sh, Backbuffer *bb) {
  int dw = bb->width, dh = bb->height;
  for (int y = 0; y < dh; y++) {
    float sy = ((float)y + 0.5f) * (float)sh / (float)dh - 0.5f;
    int y0 = (int)sy;            /* floor */
    int y1 = y0 + 1;             /* ceil  */
    float fy = sy - (float)y0;   /* fractional part */
    /* clamp */
    if (y0 < 0) y0 = 0;
    if (y1 >= sh) y1 = sh - 1;

    for (int x = 0; x < dw; x++) {
      float sx = ((float)x + 0.5f) * (float)sw / (float)dw - 0.5f;
      int x0 = (int)sx, x1 = x0 + 1;
      float fx = sx - (float)x0;
      if (x0 < 0) x0 = 0;
      if (x1 >= sw) x1 = sw - 1;

      /* 4-pixel bilinear blend */
      Vec3 p00 = src[y0 * sw + x0], p01 = src[y0 * sw + x1];
      Vec3 p10 = src[y1 * sw + x0], p11 = src[y1 * sw + x1];

      Vec3 top = vec3_add(vec3_scale(p00, 1.0f - fx), vec3_scale(p01, fx));
      Vec3 bot = vec3_add(vec3_scale(p10, 1.0f - fx), vec3_scale(p11, fx));
      Vec3 c   = vec3_add(vec3_scale(top, 1.0f - fy), vec3_scale(bot, fy));

      /* write to backbuffer */
      bb->pixels[y * dw + x] = GAME_RGB(
        (int)(c.x * 255), (int)(c.y * 255), (int)(c.z * 255));
    }
  }
}
```

The math: for each output pixel, map back to float source coordinates `(sx, sy)`. Sample the 4 nearest source pixels (p00, p01, p10, p11) and blend: `result = (1-fx)(1-fy)·p00 + fx(1-fy)·p01 + (1-fx)fy·p10 + fx·fy·p11`. This is what GPU texture filtering does in hardware.

### 5. Octant-BVH voxel acceleration

The brute-force voxel intersection tests every solid cell in the grid (~60 cells for the bunny). The octant BVH splits the grid into 8 sub-volumes at the model's bounding box center:

```c
/* In voxel_model_init(): assign each solid cell to an octant */
Vec3 mid = model->bbox_center;
for (int s = 0; s < model->solid_count; s++) {
  Vec3 c = model->cells[s].center;
  int octant = ((c.x >= mid.x) ? 1 : 0)
             | ((c.y >= mid.y) ? 2 : 0)
             | ((c.z >= mid.z) ? 4 : 0);
  VoxelOctant *oct = &model->octants[octant];
  oct->cell_indices[oct->cell_count++] = s;
}
```

Each octant gets a tight sub-AABB computed from its cells. During intersection, rays that miss an octant's AABB skip all cells inside it:

```c
int voxel_model_intersect_bvh(RtRay ray, const VoxelModel *model,
                              HitRecord *hit, Vec3 *voxel_color_out) {
  /* Level 0: model bounding box */
  if (!aabb_test(ray, model->bbox_center, model->bbox_half_size))
    return 0;

  /* Level 1: test each octant sub-AABB */
  for (int o = 0; o < VOXEL_OCTANTS; o++) {
    const VoxelOctant *oct = &model->octants[o];
    if (oct->cell_count == 0) continue;
    if (!aabb_test(ray, oct->center, oct->half_size)) continue;

    /* Level 2: per-cell tests within this octant */
    for (int ci = 0; ci < oct->cell_count; ci++) {
      /* ... box_intersect per cell ... */
    }
  }
}
```

For the 6×6×4 bunny, a typical ray hits 2-3 octants out of 8, testing ~30 cells instead of ~60. This cuts intersection tests by roughly 50%.

### 6. Mode branching in intersect.c

`scene_intersect` checks the render mode to choose the intersection strategy:

```c
if (settings && settings->render_mode == RENDER_CPU_OPT)
  vhit = voxel_model_intersect_bvh(ray, &scene->voxel_models[i], &tmp, &vc);
else
  vhit = voxel_model_intersect(ray, &scene->voxel_models[i], &tmp, &vc);
```

## Key takeaways

1. **Frame rate capping** is the single biggest win for CPU usage — `nanosleep` yields unused time instead of busy-spinning
2. **Bilinear interpolation** is a quality improvement, not a speed improvement — it trades a small amount of upscale CPU time for much smoother edges at `scale > 1`
3. **Octant BVH** demonstrates the principle of spatial acceleration structures — even a simple 8-way split cuts intersection tests significantly. Production raytracers use full BVH trees (binary, SAH-optimized) for much larger gains
4. The **three-mode toggle** (N key) lets you A/B compare: see the blocky vs smooth edges, hear the fan quiet down, watch the HUD metrics change
