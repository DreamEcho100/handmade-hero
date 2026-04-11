# Lesson 12 — Settings + Toggles + AA + HUD + Performance

> **What you'll build:** By the end of this lesson, a HUD displays FPS, frame time, and ray count. Feature toggle keys (V/F/B/R/T/H/X/Tab) turn scene elements on/off in real time. Anti-aliasing smooths sphere edges. Adaptive quality drops AA while the camera is moving. P key exports a PPM snapshot.

## Observable outcome

A semi-transparent HUD overlay in the top-left corner shows performance metrics: FPS (color-coded green/yellow/red), render time in milliseconds, ray count, render scale, and AA status. A second line shows controls. A third line shows toggle states (V:on F:on B:on R:on T:on H:on X:AA). A fourth line shows camera distance, yaw, and pitch. Pressing V/F/B/R/T/H toggles voxels, floor, boxes, reflections, refractions, and shadows. X toggles anti-aliasing (sphere edges become noticeably smoother). Tab cycles render scale (2x, 1x, 3x). P exports the current frame as `out.ppm`. While dragging the camera, AA is automatically disabled for speed, then re-enabled after 3 still frames.

## New concepts

- `RenderSettings` struct — centralizes all toggle flags and quality settings in one passable struct
- Feature toggle buttons (V/F/B/R/T/H/X/Tab) — `button_just_pressed` detects edge-triggered keypress
- 2x2 jittered supersampling (SSAA) — 4 sub-pixel rays per pixel, averaged, for anti-aliased edges
- Adaptive quality — disable AA while camera is moving, re-enable when still
- HUD overlay — `snprintf` + `draw_text` + `draw_rect` (with alpha blending) + two-context pattern: `hud_cfg` derived from the platform-supplied `world_config` (inheriting `coord_origin`), camera and zoom overridden to lock HUD to screen space; `HUD_TOP_Y(offset)` macro converts top-relative offsets to bottom-left y-coordinates
- Frame timing — `clock_gettime(CLOCK_MONOTONIC)` for high-resolution render timing
- PPM export wiring — P key triggers `ppm_export` on the backbuffer

## Files changed

| File                      | Change type | Summary                                                                                                            |
| ------------------------- | ----------- | ------------------------------------------------------------------------------------------------------------------ |
| `game/settings.h`         | Created     | `RenderSettings` struct + `render_settings_init`                                                                   |
| `game/vec3.h`             | Modified    | Add `vec3_cross` and `vec3_lerp` (used by camera basis and background gradient)                                    |
| `game/base.h`             | Modified    | Expand to 22 buttons: add all toggle/export/scale buttons                                                          |
| `game/intersect.h`        | Modified    | `scene_intersect` gains `Vec3 *voxel_color_out` and `const RenderSettings *settings` parameters                    |
| `game/intersect.c`        | Modified    | `scene_intersect` uses settings to conditionally skip floor/boxes/voxels                                           |
| `game/lighting.h`         | Modified    | `compute_lighting` gains `const RenderSettings *settings` parameter                                                |
| `game/lighting.c`         | Modified    | Shadow ray gated by `settings->show_shadows`                                                                       |
| `game/raytracer.h`        | Modified    | `cast_ray` gains `const RenderSettings *settings` parameter                                                        |
| `game/raytracer.c`        | Modified    | Reflections/refractions gated by `settings->show_reflections`/`show_refractions`                                   |
| `game/render.h`           | Modified    | `render_scene` gains `const RenderSettings *settings`; add `render_scene_to_float`                                 |
| `game/render.c`           | Modified    | AA logic (`trace_pixel_aa4`), `RenderJob` carries settings, scale support                                          |
| `game/main.h`             | Modified    | `RaytracerState` gains `settings`, `still_frames`, `active_scale`, `frame_time_ms`, `fps_smoothed`                 |
| `game/main.c`             | Rewritten   | Toggle handling, adaptive quality, timed render, HUD with metrics, PPM export; adds `#include "../utils/render.h"` |
| `platforms/x11/main.c`    | Modified    | All toggle/export key bindings added                                                                               |
| `platforms/raylib/main.c` | Modified    | Same key bindings                                                                                                  |

## Background — why this works

### The big refactor: adding RenderSettings to function signatures

This lesson introduces the single largest API change in the course. The `RenderSettings` struct is threaded through the entire rendering pipeline so every subsystem can check which features are enabled.

**BEFORE (L03-L10):** Simple signatures with no settings parameter.

```c
/* scene_intersect — 3 args */
int scene_intersect(RtRay ray, const Scene *scene, HitRecord *hit);

/* compute_lighting — 5 args */
LightingResult compute_lighting(Vec3 point, Vec3 normal, Vec3 view_dir,
                                const RtMaterial *material,
                                const Scene *scene);

/* cast_ray — 3 args */
Vec3 cast_ray(RtRay ray, const Scene *scene, int depth);
```

**AFTER (L12):** Each function gains a `const RenderSettings *settings` parameter.

```c
/* scene_intersect — 5 args */
int scene_intersect(RtRay ray, const Scene *scene, HitRecord *hit,
                    Vec3 *voxel_color_out, const RenderSettings *settings);

/* compute_lighting — 6 args */
LightingResult compute_lighting(Vec3 point, Vec3 normal, Vec3 view_dir,
                                const RtMaterial *material,
                                const Scene *scene,
                                const RenderSettings *settings);

/* cast_ray — 4 args */
Vec3 cast_ray(RtRay ray, const Scene *scene, int depth,
              const RenderSettings *settings);
```

The `voxel_color_out` parameter on `scene_intersect` is also new — it carries per-voxel palette color data for L16. Passing `NULL` is valid and means "I don't need voxel color info."

All callers of these functions must be updated. This is the cost of adding toggles retroactively. The `const RenderSettings *settings` convention allows passing `NULL` to mean "all features on" — each function checks `!settings || settings->show_X` before toggling.

### RenderSettings struct

```c
typedef struct {
  int show_voxels;       /* V key */
  int show_floor;        /* F key */
  int show_boxes;        /* B key */
  int show_meshes;       /* M key (future) */
  int show_reflections;  /* R key */
  int show_refractions;  /* T key */
  int show_shadows;      /* H key */
  int show_envmap;       /* E key (future) */
  int render_scale;      /* 1=full, 2=half, 3=quarter — Tab cycles */
  int aa_samples;        /* 1=off, 4=2x2 jittered SSAA — X toggles */
} RenderSettings;
```

Each `show_*` field is a boolean toggle (0 or 1). `render_scale` controls resolution: at scale 2, we render at 400x300 and upscale each pixel to 2x2. `aa_samples` is 1 (off) or 4 (2x2 jittered supersampling).

### 2x2 Jittered Supersampling (SSAA)

Without anti-aliasing, sphere edges show staircase-like jaggies. SSAA fixes this by shooting multiple rays per pixel at slightly different positions, then averaging the results.

A regular 2x2 grid (corners of each pixel) works, but a **jittered** grid produces better results for the same cost. The four sample positions within a pixel:

```
+---+---+
| x |   |   (0.25, 0.25)  (0.75, 0.25)
|   | x |
+---+---+
|   | x |   (0.25, 0.75)  (0.75, 0.75)
| x |   |
+---+---+
```

```c
static inline Vec3 trace_pixel_aa4(int px, int py, int width, int height,
                                   const CameraBasis *basis,
                                   const Scene *scene,
                                   const RenderSettings *settings) {
  static const float jx[4] = { 0.25f, 0.75f, 0.25f, 0.75f };
  static const float jy[4] = { 0.25f, 0.25f, 0.75f, 0.75f };

  Vec3 sum = vec3_make(0.0f, 0.0f, 0.0f);
  for (int s = 0; s < 4; s++) {
    Vec3 c = trace_subpixel((float)px + jx[s], (float)py + jy[s],
                            width, height, basis, scene, settings);
    sum = vec3_add(sum, c);
  }
  return vec3_scale(sum, 0.25f);  /* average of 4 samples */
}
```

Cost: 4x the rays per pixel. At 800x600 with scale 2 (400x300), that is 480K rays vs 120K — still fast with multi-threading.

### Adaptive quality

During camera drag, the user wants smooth interaction, not perfect image quality. The adaptive system:

1. `camera_update` returns 1 if the camera moved
2. `game_update` tracks `still_frames` (counter reset to 0 on movement, increments when still)
3. `game_render` builds a `frame_settings` copy:
   - Moving (`still_frames < 3`): use user's scale but force `aa_samples = 1`
   - Still (`still_frames >= 3`): use full user settings (scale + AA)

This avoids the jarring quality drop of scale-3 blockiness while keeping interaction smooth.

### HUD overlay

The HUD uses `RenderContext` from `utils/render_explicit.h` — a precomputed struct of world→pixel conversion coefficients built once per frame. All positions are expressed in **game units** (`GAME_UNITS_W × GAME_UNITS_H`); the `RenderContext` converts them to pixels at each draw call. This makes the HUD resolution-independent — if the backbuffer size changes, the HUD scales proportionally.

The key setup uses the **two-context pattern**: the HUD `GameWorldConfig` is _derived_ from the `world_config` parameter that the platform passes into `game_render`, inheriting `coord_origin`, then overriding camera position and zoom to lock the HUD to screen space. The zoom **must** be set to `1.0f` — zero-init (`{0}`) leaves it at `0.0f` and `make_render_context()` would scale everything to zero.

Because `coord_origin` is inherited from the platform (typically `COORD_ORIGIN_LEFT_BOTTOM`), y=0 is at the screen _bottom_. The HUD must appear at the _top_, so all HUD y-coordinates use the `HUD_TOP_Y(offset)` macro, defined as `GAME_UNITS_H - offset`. Large y values (near `GAME_UNITS_H=12`) map to the screen top.

```c
/* Derive the HUD render context from the platform-supplied world_config.
 * Inherit coord_origin so the HUD respects the same axis convention.
 * Override camera and zoom to lock the HUD to screen space. */
GameWorldConfig hud_cfg = world_config;  /* inherit coord_origin from platform */
hud_cfg.camera_x    = 0.0f;
hud_cfg.camera_y    = 0.0f;
hud_cfg.camera_zoom = 1.0f;
RenderContext ctx = make_render_context(bb, hud_cfg);

/* HUD_TOP_Y(offset) = GAME_UNITS_H - offset
 * With COORD_ORIGIN_LEFT_BOTTOM, y=0 is the bottom.
 * Subtract from GAME_UNITS_H to place items near the top. */
#define HUD_TOP_Y(offset) (GAME_UNITS_H - (offset))

/* Semi-transparent background. */
draw_rect(bb,
  world_rect_px_x(&ctx, 0.08f, 11.6f),
  world_rect_px_y(&ctx, HUD_TOP_Y(0.08f + 2.08f), 2.08f),
  world_w(&ctx, 11.6f), world_h(&ctx, 2.08f),
  0.0f, 0.0f, 0.0f, 0.706f);

/* Line 1: performance metrics. */
char l1[128];
snprintf(l1, sizeof(l1),
  "%.1f fps | %.1f ms | %dK rays | scale %d (%dx%d) aa:%d %s",
  state->fps_smoothed, render_ms, rays_k,
  s, rw, rh, aa,
  state->still_frames < 3 ? "[moving]" : "");
draw_text(bb,
  world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(0.16f)),
  1, l1,
  state->fps_smoothed >= 30.0f ? COLOR_GREEN :
  state->fps_smoothed >= 15.0f ? COLOR_YELLOW : COLOR_RED);
```

`world_rect_px_x(&ctx, wx, ww)` returns the pixel left edge of a rect at world position `wx` with width `ww`. `world_x(&ctx, wx)` converts a single X coordinate. Both are zero-cost `static inline` functions from `render_explicit.h`.

`snprintf` is the safe version of `sprintf` — it takes a max length to prevent buffer overflow. The FPS color coding (green/yellow/red) gives instant visual feedback on performance. Color constants like `COLOR_GREEN` expand to four float arguments (r, g, b, a) — the draw API takes float colors [0.0–1.0], not packed uint32_t.

### Frame timing

```c
static double get_time_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
```

`CLOCK_MONOTONIC` is a clock that only moves forward (not affected by NTP adjustments). Nanosecond precision allows measuring sub-millisecond render times. `_POSIX_C_SOURCE 200809L` is required at the top of the file to enable `clock_gettime`.

The FPS display uses exponential moving average for smooth readout:

```c
float instant_fps = 1000.0f / render_ms;
state->fps_smoothed = state->fps_smoothed * 0.9f + instant_fps * 0.1f;
```

## Code walkthrough

### `game/settings.h` — complete file

```c
#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H

typedef struct {
  int show_voxels;
  int show_floor;
  int show_boxes;
  int show_meshes;
  int show_reflections;
  int show_refractions;
  int show_shadows;
  int show_envmap;
  int render_scale;      /* 1=full, 2=half, 3=quarter */
  int aa_samples;        /* 1=off, 4=2x2 jittered SSAA */
} RenderSettings;

static inline void render_settings_init(RenderSettings *s) {
  s->show_voxels      = 1;
  s->show_floor       = 1;
  s->show_boxes       = 1;
  s->show_meshes      = 1;
  s->show_reflections = 1;
  s->show_refractions = 1;
  s->show_shadows     = 1;
  s->show_envmap      = 1;
  s->render_scale     = 2;  /* default: half resolution for speed */
  s->aa_samples       = 4;  /* default: 2x2 SSAA when still */
}

#endif /* GAME_SETTINGS_H */
```

**Key design:** All fields default to "on" / highest quality. The adaptive system temporarily overrides `aa_samples` during camera movement. `render_settings_init` is `static inline` (header-only) — no `.c` file needed for this small struct.

### `game/main.c` — toggle handling in game_update

```c
void game_update(RaytracerState *state, GameInput *input, float delta_time) {
  int cam_moved = camera_update(&state->camera, input, delta_time);

  /* Adaptive quality tracking. */
  if (cam_moved) {
    state->still_frames = 0;
  } else {
    if (state->still_frames < 1000) state->still_frames++;
  }

  /* Feature toggles — XOR flips 0↔1 on each keypress. */
  if (button_just_pressed(input->buttons.toggle_voxels))
    state->settings.show_voxels ^= 1;
  if (button_just_pressed(input->buttons.toggle_floor))
    state->settings.show_floor ^= 1;
  if (button_just_pressed(input->buttons.toggle_boxes))
    state->settings.show_boxes ^= 1;
  if (button_just_pressed(input->buttons.toggle_reflections))
    state->settings.show_reflections ^= 1;
  if (button_just_pressed(input->buttons.toggle_refractions))
    state->settings.show_refractions ^= 1;
  if (button_just_pressed(input->buttons.toggle_shadows))
    state->settings.show_shadows ^= 1;

  /* AA toggle: 1 ↔ 4. */
  if (button_just_pressed(input->buttons.toggle_aa))
    state->settings.aa_samples = (state->settings.aa_samples == 1) ? 4 : 1;

  /* Render scale cycle: 2 → 1 → 3 → 2. */
  if (button_just_pressed(input->buttons.scale_cycle)) {
    int s = state->settings.render_scale;
    state->settings.render_scale = (s == 2) ? 1 : (s == 1) ? 3 : 2;
  }

  /* PPM export. */
  if (button_just_pressed(input->buttons.export_ppm))
    state->export_ppm_requested = 1;
}
```

**Key lines:**

- `button_just_pressed` is a helper that checks `ended_down && half_transitions > 0` — it fires once on the keydown edge, not continuously while held. This is essential for toggles.
- `^= 1` is XOR assignment — flips 0 to 1 and 1 to 0. Cleaner than `= !value` for int fields.
- Scale cycle uses nested ternary: `2→1→3→2`. Scale 2 is the default (half resolution), scale 1 is full quality, scale 3 is fast/blocky for testing.

### `game/main.c` — game_render with timed render and HUD

`game_render` now receives a third parameter `GameWorldConfig world_config` supplied by the platform. This is the same config used to render the scene, and the HUD derives its own config from it (the two-context pattern).

```c
void game_render(RaytracerState *state, Backbuffer *bb,
                 GameWorldConfig world_config) {
  /* Build per-frame settings. */
  RenderSettings frame_settings = state->settings;
  if (state->still_frames < 3) {
    frame_settings.aa_samples = 1;  /* single sample while dragging */
  }

  /* Timed render. */
  double t0 = get_time_sec();
  render_scene(bb, &state->scene, &state->camera, &frame_settings);
  double t1 = get_time_sec();
  float render_ms = (float)((t1 - t0) * 1000.0);
  state->frame_time_ms = render_ms;

  /* EMA for smooth FPS. */
  float instant_fps = (render_ms > 0.1f) ? (1000.0f / render_ms) : 999.0f;
  state->fps_smoothed = state->fps_smoothed * 0.9f + instant_fps * 0.1f;

  /* Ray count. */
  int s = frame_settings.render_scale;
  int rw = bb->width / s, rh = bb->height / s;
  int aa = frame_settings.aa_samples;
  int rays_k = (rw * rh * aa) / 1000;

  /* HUD overlay — two-context pattern.
   * Derive hud_cfg from world_config to inherit coord_origin from the platform,
   * then override camera and zoom to lock the HUD to screen space.
   * camera_zoom=1.0f is required; zero leaves zoom at 0 and scales everything
   * to zero. */
  GameWorldConfig hud_cfg = world_config;  /* inherit coord_origin from platform */
  hud_cfg.camera_x    = 0.0f;
  hud_cfg.camera_y    = 0.0f;
  hud_cfg.camera_zoom = 1.0f;
  RenderContext ctx = make_render_context(bb, hud_cfg);

  /* With COORD_ORIGIN_LEFT_BOTTOM (inherited), y=0 is the screen bottom.
   * HUD_TOP_Y(offset) = GAME_UNITS_H - offset places items near the top. */
#define HUD_TOP_Y(offset) (GAME_UNITS_H - (offset))

  draw_rect(bb,
    world_rect_px_x(&ctx, 0.08f, 11.6f),
    world_rect_px_y(&ctx, HUD_TOP_Y(0.08f + 2.08f), 2.08f),
    world_w(&ctx, 11.6f), world_h(&ctx, 2.08f),
    0.0f, 0.0f, 0.0f, 0.706f);

  char l1[128];
  snprintf(l1, sizeof(l1),
    "%.1f fps | %.1f ms | %dK rays | scale %d (%dx%d) aa:%d %s",
    state->fps_smoothed, render_ms, rays_k, s, rw, rh, aa,
    state->still_frames < 3 ? "[moving]" : "");
  draw_text(bb,
    world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(0.16f)),
    1, l1,
    state->fps_smoothed >= 30.0f ? COLOR_GREEN :
    state->fps_smoothed >= 15.0f ? COLOR_YELLOW : COLOR_RED);

  /* Additional HUD lines use HUD_TOP_Y for each row, stepping down by ~0.48f:
   *   HUD_TOP_Y(0.64f)  — controls line
   *   HUD_TOP_Y(1.12f)  — toggle states line
   *   HUD_TOP_Y(1.60f)  — camera info line */

  /* PPM export (after HUD is drawn). */
  if (state->export_ppm_requested) {
    ppm_export(bb, "out.ppm");
    state->export_ppm_requested = 0;
  }
}
```

**Key design:**

- `game_render` receives `world_config` from the platform — the game never hardcodes `coord_origin`.
- `hud_cfg` is a struct copy of `world_config`, not a fresh zero-init. This ensures `coord_origin` (and any other platform-set fields) are inherited automatically.
- `HUD_TOP_Y` translates from "distance from top" thinking into the bottom-left coordinate space. Each HUD text line increments the offset by ~0.48 game units.
- `frame_settings` is a _copy_ of `state->settings`, modified for this frame only. The original settings are preserved for the HUD display and for the next frame.
- PPM export happens _after_ HUD drawing, so the exported image includes the HUD. This is intentional — the export captures exactly what the user sees.

### `game/base.h` — expanded button layout (L12 final)

The button count grows from 7 (L11) to 22 to accommodate all toggles and exports:

```c
typedef struct {
  union {
    GameButtonState all[24];
    struct {
      /* Navigation (7 buttons — from L11) */
      GameButtonState quit;
      GameButtonState camera_left;
      GameButtonState camera_right;
      GameButtonState camera_up;
      GameButtonState camera_down;
      GameButtonState camera_forward;
      GameButtonState camera_backward;
      /* Exports (5 buttons — new) */
      GameButtonState export_ppm;
      GameButtonState export_anaglyph;
      GameButtonState export_sidebyside;
      GameButtonState export_stereogram;
      GameButtonState export_stereogram_cross;
      /* Feature toggles (9 buttons — new) */
      GameButtonState toggle_voxels;       /* V */
      GameButtonState toggle_floor;        /* F */
      GameButtonState toggle_boxes;        /* B */
      GameButtonState toggle_meshes;       /* M */
      GameButtonState toggle_reflections;  /* R */
      GameButtonState toggle_refractions;  /* T */
      GameButtonState toggle_shadows;      /* H */
      GameButtonState toggle_envmap;       /* E */
      GameButtonState toggle_aa;           /* X */
      /* Render scale (1 button — new) */
      GameButtonState scale_cycle;         /* Tab */
    };
  } buttons;
  MouseState mouse;
} GameInput;
```

The `all[24]` array (slightly larger than the struct count) accommodates future additions without breaking the union.

### Platform backend — toggle key bindings (X11)

```c
static void handle_key(GameInput *curr, KeySym sym, int is_down) {
  switch (sym) {
  /* ... existing keys ... */
  /* Feature toggles (new in L12) */
  case XK_v: case XK_V: UPDATE_BUTTON(&curr->buttons.toggle_voxels, is_down); break;
  case XK_f: case XK_F: UPDATE_BUTTON(&curr->buttons.toggle_floor, is_down); break;
  case XK_b: case XK_B: UPDATE_BUTTON(&curr->buttons.toggle_boxes, is_down); break;
  case XK_r: case XK_R: UPDATE_BUTTON(&curr->buttons.toggle_reflections, is_down); break;
  case XK_t: case XK_T: UPDATE_BUTTON(&curr->buttons.toggle_refractions, is_down); break;
  case XK_h: case XK_H: UPDATE_BUTTON(&curr->buttons.toggle_shadows, is_down); break;
  case XK_x: case XK_X: UPDATE_BUTTON(&curr->buttons.toggle_aa, is_down); break;
  case XK_Tab: UPDATE_BUTTON(&curr->buttons.scale_cycle, is_down); break;
  case XK_p: case XK_P: UPDATE_BUTTON(&curr->buttons.export_ppm, is_down); break;
  }
}
```

Each toggle key maps to its button via `UPDATE_BUTTON`. Both lowercase and uppercase variants are handled for convenience.

## Common mistakes

| Symptom                                                     | Cause                                                                                                     | Fix                                                                                                                                                                                                                                                                                                   |
| ----------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Compile errors everywhere after adding `settings` parameter | Not all callers updated to new signature                                                                  | Grep for every call to `scene_intersect`, `compute_lighting`, and `cast_ray`; update all                                                                                                                                                                                                              |
| Toggle has no effect                                        | Using `button.ended_down` instead of `button_just_pressed`                                                | `ended_down` fires every frame while held; `button_just_pressed` fires once on edge                                                                                                                                                                                                                   |
| HUD text overlaps or is invisible                           | Wrong coordinates or missing background rect                                                              | Derive `hud_cfg` from `world_config` (inherit `coord_origin`), set `camera_zoom=1.0f`, use `HUD_TOP_Y(offset)` for all y-coords: `draw_rect(bb, world_rect_px_x(&ctx, 0.08f, 11.6f), world_rect_px_y(&ctx, HUD_TOP_Y(0.08f+2.08f), 2.08f), world_w(&ctx, 11.6f), world_h(&ctx, 2.08f), 0,0,0,0.706f)` |
| HUD items appear at screen bottom instead of top            | Using small y values (e.g. `0.08f`) without `HUD_TOP_Y` when `coord_origin` is `COORD_ORIGIN_LEFT_BOTTOM` | Wrap all HUD y-coords with `HUD_TOP_Y(offset)` — defined as `GAME_UNITS_H - offset`; small offsets from the top need large y values in bottom-left space                                                                                                                                              |
| HUD is invisible (nothing rendered)                         | `camera_zoom` left at `0.0f` (zero-init or forgotten)                                                     | Always set `hud_cfg.camera_zoom = 1.0f` after copying from `world_config` — `make_render_context` scales everything to zero if zoom is zero                                                                                                                                                           |
| AA makes no visual difference                               | `aa_samples` not reaching the render thread                                                               | Verify `frame_settings.aa_samples` is passed through `RenderJob` to `sample_pixel`                                                                                                                                                                                                                    |
| FPS display jumps wildly                                    | Using instant FPS instead of smoothed                                                                     | Apply EMA: `smoothed = smoothed * 0.9 + instant * 0.1`                                                                                                                                                                                                                                                |
| PPM export is blank                                         | Exporting before `render_scene` runs                                                                      | Call `ppm_export` after `render_scene` in `game_render`                                                                                                                                                                                                                                               |
| Scale 3 looks terrible                                      | Expected — that is 267x200 pixels upscaled                                                                | Tab back to scale 2 or 1 for better quality                                                                                                                                                                                                                                                           |

## Exercise

> Press H to disable shadows, then R to disable reflections, then T to disable refractions. Observe how each feature contributes to the scene's realism. The scene with all three disabled looks like L06 (flat Phong shading). Re-enable them one by one to appreciate each effect. Then press X to toggle AA and observe the difference on sphere edges — zoom in (scroll wheel) to see the jaggies more clearly.

## JS ↔ C concept map

| JS / Web concept                                  | C equivalent in this lesson                                                                                                  | Key difference                                                                                                                      |
| ------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `renderer.setPixelRatio(2)`                       | `settings.render_scale = 2` (half resolution, upscaled)                                                                      | Manual upscale loop instead of GPU scaling                                                                                          |
| `renderer.antialias = true`                       | `settings.aa_samples = 4` (4x SSAA)                                                                                          | CPU supersampling, not GPU MSAA; 4x the ray cost                                                                                    |
| `performance.now()`                               | `clock_gettime(CLOCK_MONOTONIC, &ts)`                                                                                        | Nanosecond resolution; requires `_POSIX_C_SOURCE 200809L`                                                                           |
| `stats.js` FPS counter                            | `snprintf(buf, ..., "%.1f fps", smoothed_fps)` + `draw_text(bb, world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(0.16f)), ...)` | Manual text rendering; position in game units via `RenderContext`; `HUD_TOP_Y` maps top-relative offsets to bottom-left coordinates |
| `gui.add(settings, 'shadows')` (dat.gui)          | `if (button_just_pressed(toggle_shadows)) settings.show_shadows ^= 1`                                                        | No GUI library; raw keyboard toggles                                                                                                |
| `{ ...defaultSettings, shadows: false }` (spread) | `RenderSettings frame = state->settings; frame.aa_samples = 1;`                                                              | Struct copy + field override; no spread operator                                                                                    |
