#define _POSIX_C_SOURCE 200809L

/* =============================================================================
 * game/main.c -- TinyRaytracer Course
 * =============================================================================
 */

#include "main.h"
#include "../platform.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../utils/render.h"
#include "ppm.h"
#include "render.h"
#include "shader_glsl.h"
#include "stereo.h"
#include "stereogram.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h> /* sysconf for thread count */

/* -- High-resolution timer ------------------------------------------------- */
static double get_time_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int get_thread_count(void) {
  long n = sysconf(_SC_NPROCESSORS_ONLN);
  if (n < 1)
    n = 1;
  if (n > 32)
    n = 32;
  return (int)n;
}

void game_init(RaytracerState *state) {
  memset(state, 0, sizeof(*state));
  scene_init(&state->scene);
  camera_init(&state->camera);
  render_settings_init(&state->settings);
  state->active_scale = state->settings.render_scale;
  state->fps_smoothed = 30.0f;
  state->thread_count = get_thread_count();
  state->show_hud = 1;
}

void game_update(RaytracerState *state, GameInput *input, float delta_time) {
  int cam_moved = camera_update(&state->camera, input, delta_time);

  /* -- Adaptive quality ---------------------------------------------------
   * Moving: use user scale but disable AA (1 sample = fast + decent).
   * Still 3+ frames: full quality (user scale + user AA setting).        */
  if (cam_moved) {
    state->still_frames = 0;
  } else {
    if (state->still_frames < 1000)
      state->still_frames++;
  }

  /* Feature toggles */
  if (button_just_pressed(input->buttons.toggle_voxels))
    state->settings.show_voxels ^= 1;
  if (button_just_pressed(input->buttons.toggle_floor))
    state->settings.show_floor ^= 1;
  if (button_just_pressed(input->buttons.toggle_boxes))
    state->settings.show_boxes ^= 1;
  if (button_just_pressed(input->buttons.toggle_meshes))
    state->settings.show_meshes ^= 1;
  if (button_just_pressed(input->buttons.toggle_reflections))
    state->settings.show_reflections ^= 1;
  if (button_just_pressed(input->buttons.toggle_refractions))
    state->settings.show_refractions ^= 1;
  if (button_just_pressed(input->buttons.toggle_shadows))
    state->settings.show_shadows ^= 1;
  if (button_just_pressed(input->buttons.toggle_envmap))
    state->settings.show_envmap ^= 1;
  if (button_just_pressed(input->buttons.toggle_aa))
    state->settings.aa_samples = (state->settings.aa_samples == 1) ? 4 : 1;

  /* Cycle envmap mode: procedural -> equirect -> cubemap (L20). */
  if (button_just_pressed(input->buttons.cycle_envmap_mode)) {
    EnvMap *em = &state->scene.envmap;
    int next = ((int)em->mode + 1) % 3;
    if (next == ENVMAP_EQUIRECT && !em->pixels)
      next = (next + 1) % 3;
    if (next == ENVMAP_CUBEMAP && !em->faces[0].pixels)
      next = (next + 1) % 3;
    em->mode = (EnvMapMode)next;
  }

  /* Render scale cycle (Tab: 2->1->3->2) */
  if (button_just_pressed(input->buttons.scale_cycle)) {
    int s = state->settings.render_scale;
    state->settings.render_scale = (s == 2) ? 1 : (s == 1) ? 3 : 2;
  }

  /* Render mode cycle (N: CPU-BASIC -> CPU-OPT -> GPU -> ...) */
  if (button_just_pressed(input->buttons.cycle_render_mode)) {
    int next = ((int)state->settings.render_mode + 1) % RENDER_MODE_COUNT;
    /* Skip GPU mode if shader failed to compile. */
    if (next == RENDER_GPU && !state->gpu_available)
      next = (next + 1) % RENDER_MODE_COUNT;
    state->settings.render_mode = (RenderMode)next;
  }

  /* HUD toggle (F1) */
  if (button_just_pressed(input->buttons.toggle_hud))
    state->show_hud ^= 1;

  /* Exports */
  if (button_just_pressed(input->buttons.export_ppm))
    state->export_ppm_requested = 1;
  if (button_just_pressed(input->buttons.export_anaglyph))
    state->export_anaglyph_requested = 1;
  if (button_just_pressed(input->buttons.export_sidebyside))
    state->export_sidebyside_requested = 1;
  if (button_just_pressed(input->buttons.export_stereogram))
    state->export_stereogram_requested = 1;
  if (button_just_pressed(input->buttons.export_stereogram_cross))
    state->export_stereogram_cross_requested = 1;
  if (button_just_pressed(input->buttons.export_glsl))
    state->export_glsl_requested = 1;
}

/* -- Mode label strings ---------------------------------------------------- */
static const char *render_mode_label(RenderMode m) {
  switch (m) {
  case RENDER_CPU_BASIC:
    return "[CPU-BASIC]";
  case RENDER_CPU_OPT:
    return "[CPU-OPT]";
  case RENDER_GPU:
    return "[GPU]";
  default:
    return "[???]";
  }
}

void game_render(RaytracerState *state, Backbuffer *bb,
                 GameWorldConfig world_config) {
  RenderMode mode = state->settings.render_mode;

  /* Build per-frame settings:
   * - While moving: keep user's scale but disable AA for speed.
   * - When still: full user settings (scale + AA).                        */
  RenderSettings frame_settings = state->settings;
  if (state->still_frames < 3) {
    frame_settings.aa_samples = 1; /* single sample while dragging */
  }
  state->active_scale = frame_settings.render_scale;

  /* -- Timed render (CPU modes only) -------------------------------------- */
  float render_ms = 0.0f;
  int rays_k = 0;

  /* GPU mode: clear backbuffer to transparent black so the HUD overlay
   * doesn't cover the GPU-rendered scene.  CPU modes write every pixel
   * via render_scene, so they don't need this.                            */
  if (mode == RENDER_GPU) {
    memset(bb->pixels, 0, (size_t)(bb->width * bb->height) * 4);
  }

  if (mode != RENDER_GPU) {
    double t0 = get_time_sec();
    render_scene(bb, &state->scene, &state->camera, &frame_settings);
    double t1 = get_time_sec();
    float work_ms = (float)((t1 - t0) * 1000.0);

    /* Frame rate cap for CPU-OPT: sleep to hit ~30 FPS. */
    if (mode == RENDER_CPU_OPT) {
      float target_ms = 33.3f;
      float sleep_ms = target_ms - work_ms;
      if (sleep_ms > 1.0f) {
        struct timespec ts = {0, (long)(sleep_ms * 1e6f)};
        nanosleep(&ts, NULL);
      }
    }

    render_ms = work_ms;

    int s = frame_settings.render_scale;
    int rw = bb->width / (s > 0 ? s : 1);
    int rh = bb->height / (s > 0 ? s : 1);
    int aa = frame_settings.aa_samples;
    rays_k = (rw * rh * aa) / 1000;
  }

  state->frame_time_ms = render_ms;
  state->ray_count_k = rays_k;

  /* Exponential moving average for smooth FPS display. */
  float instant_fps = (render_ms > 0.1f) ? (1000.0f / render_ms) : 999.0f;
  if (mode == RENDER_GPU)
    instant_fps = 60.0f; /* placeholder until platform sets it */
  state->fps_smoothed = state->fps_smoothed * 0.9f + instant_fps * 0.1f;

  /* Derived metrics. */
  state->rays_per_sec_m =
      (render_ms > 0.1f) ? ((float)rays_k * 1000.0f) / (render_ms * 1000.0f)
                         : 0.0f;
  float target_budget = (mode == RENDER_CPU_OPT) ? 33.3f : 16.67f;
  state->cpu_util_pct = (target_budget > 0.1f && render_ms > 0.0f)
                            ? (render_ms / target_budget) * 100.0f
                            : 0.0f;

  /* -- HUD overlay --------------------------------------------------------- */
  if (state->show_hud) {
    /* HUD context: inherit coord origin from platform's world_config,
     * strip camera pan/zoom so HUD stays fixed on screen at full scale.
     * HUD uses BOTTOM_LEFT (same as game world): y=0 is screen bottom,
     * y=GAME_UNITS_H is screen top.  HUD_TOP_Y(offset) converts an
     * "offset from the top edge" into the correct world-y coordinate. */
    GameWorldConfig hud_cfg = world_config; /* inherit origin from platform */
    hud_cfg.camera_x = 0.0f;
    hud_cfg.camera_y = 0.0f;
    hud_cfg.camera_zoom = 1.0f;
    RenderContext ctx = make_render_context(bb, hud_cfg);

    /* Background rect: 0.08 margin from top, 2.08 units tall. */
    draw_rect(bb, world_rect_px_x(&ctx, 0.08f, 11.6f),
              world_rect_px_y(&ctx, HUD_TOP_Y(0.08f + 2.08f), 2.08f),
              world_w(&ctx, 11.6f), world_h(&ctx, 2.08f), 0.0f, 0.0f, 0.0f,
              0.706f);

    int s = state->active_scale;
    if (s < 1)
      s = 1;
    int rw = bb->width / s;
    int rh = bb->height / s;
    int aa = frame_settings.aa_samples;

    /* Line 1: Mode + metrics */
    char l1[160];
    if (mode == RENDER_GPU) {
      snprintf(l1, sizeof(l1), "%s %.0f fps | GPU | compile:%.0fms",
               render_mode_label(mode), state->fps_smoothed,
               state->gpu_compile_ms);
    } else {
      snprintf(l1, sizeof(l1),
               "%s %.1f fps | %.1f ms | %dK rays | %.1fM r/s | cpu:%d%% %s",
               render_mode_label(mode), state->fps_smoothed,
               state->frame_time_ms, state->ray_count_k, state->rays_per_sec_m,
               (int)state->cpu_util_pct,
               state->still_frames < 3 ? "[moving]" : "");
    }
    draw_text(bb, world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(0.16f)), 1, l1,
              state->fps_smoothed >= 30.0f   ? COLOR_GREEN
              : state->fps_smoothed >= 15.0f ? COLOR_YELLOW
                                             : COLOR_RED);

    /* Line 2: Feature toggles + envmap mode */
    char l2[160];
    {
      const char *envmap_mode_str =
          state->scene.envmap.mode == ENVMAP_CUBEMAP    ? "cube"
          : state->scene.envmap.mode == ENVMAP_EQUIRECT ? "eq"
                                                        : "sky";
      snprintf(l2, sizeof(l2),
               "V:%s F:%s B:%s M:%s R:%s T:%s H:%s E:%s X:%s C:%s",
               state->settings.show_voxels ? "on" : "--",
               state->settings.show_floor ? "on" : "--",
               state->settings.show_boxes ? "on" : "--",
               state->settings.show_meshes ? "on" : "--",
               state->settings.show_reflections ? "on" : "--",
               state->settings.show_refractions ? "on" : "--",
               state->settings.show_shadows ? "on" : "--",
               state->settings.show_envmap ? "on" : "--",
               state->settings.aa_samples >= 4 ? "AA" : "--", envmap_mode_str);
    }
    draw_text(bb, world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(0.40f)), 1, l2,
              COLOR_GREEN);

    /* Line 3: Camera + exports */
    char l3[128];
    snprintf(
        l3, sizeof(l3), "dist:%.1f yaw:%.0f pitch:%.0f | P:ppm G:sirds L:glsl",
        state->camera.orbit_radius, state->camera.yaw * (180.0f / (float)M_PI),
        state->camera.pitch * (180.0f / (float)M_PI));
    draw_text(bb, world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(0.64f)), 1, l3,
              COLOR_YELLOW);

    /* Line 4: Mode-specific details */
    char l4[160];
    switch (mode) {
    case RENDER_CPU_BASIC:
      snprintf(
          l4, sizeof(l4),
          "threads:%d | scale:%d (%dx%d block-fill) aa:%d | voxel:brute-force",
          state->thread_count, s, rw, rh, aa);
      break;
    case RENDER_CPU_OPT:
      snprintf(l4, sizeof(l4),
               "threads:%d | scale:%d (%dx%d bilinear) aa:%d | "
               "voxel:octant-BVH | cap:30fps",
               state->thread_count, s, rw, rh, aa);
      break;
    case RENDER_GPU:
      snprintf(l4, sizeof(l4),
               "GPU: %.40s | full scene: %dsph %dbox %dvox %dmesh",
               state->gpu_renderer[0] ? state->gpu_renderer : "N/A",
               state->scene.sphere_count, state->scene.box_count,
               state->scene.voxel_model_count, state->scene.mesh_count);
      break;
    default:
      l4[0] = '\0';
      break;
    }
    draw_text(bb, world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(0.88f)), 1, l4,
              0.706f, 0.706f, 1.0f, 1.0f);

    /* Line 5: Controls */
    char l5[160];
    snprintf(l5, sizeof(l5), "Arrow/WASD:cam N:mode Tab:scale F1:hud Esc:quit");
    draw_text(bb, world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(1.12f)), 1, l5,
              COLOR_WHITE);

    /* Line 6: Controls (exports + toggles) */
    char l6[160];
    snprintf(
        l6, sizeof(l6),
        "V:vox F:flr B:box M:mesh R:refl T:refr H:shad E:env X:aa C:envmode");
    draw_text(bb, world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(1.36f)), 1, l6,
              COLOR_WHITE);

    /* Line 7: Mouse + export controls */
    char l7[160];
    snprintf(l7, sizeof(l7),
             "Mouse: LMB=orbit RMB/MMB=pan Scroll=zoom | P:ppm "
             "Shift+A/S:stereo G:sirds Shift+L:glsl");
    draw_text(bb, world_x(&ctx, 0.16f), world_y(&ctx, HUD_TOP_Y(1.60f)), 1, l7,
              0.706f, 0.706f, 0.706f, 1.0f);
  }

  /* -- Exports ------------------------------------------------------------ */
  if (state->export_ppm_requested) {
    ppm_export(bb, "out.ppm");
    state->export_ppm_requested = 0;
  }
  if (state->export_anaglyph_requested) {
    render_anaglyph(bb->width, bb->height, &state->scene, &state->camera,
                    "anaglyph.ppm", &state->settings);
    state->export_anaglyph_requested = 0;
  }
  if (state->export_sidebyside_requested) {
    render_side_by_side(bb->width, bb->height, &state->scene, &state->camera,
                        "sidebyside.ppm", &state->settings);
    state->export_sidebyside_requested = 0;
  }
  if (state->export_stereogram_requested) {
    render_autostereogram(bb->width, bb->height, &state->scene, &state->camera,
                          "stereogram.ppm", &state->settings);
    state->export_stereogram_requested = 0;
  }
  if (state->export_stereogram_cross_requested) {
    render_autostereogram_crosseyed(bb->width, bb->height, &state->scene,
                                    &state->camera, "stereogram_cross.ppm",
                                    &state->settings);
    state->export_stereogram_cross_requested = 0;
  }
  if (state->export_glsl_requested) {
    shader_export_glsl("raytracer.glsl");
    state->export_glsl_requested = 0;
  }
}
