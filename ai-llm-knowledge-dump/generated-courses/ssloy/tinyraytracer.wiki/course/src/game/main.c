#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * game/main.c — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "main.h"
#include "render.h"
#include "ppm.h"
#include "stereo.h"
#include "stereogram.h"
#include "shader_glsl.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../platform.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ── High-resolution timer ────────────────────────────────────────────── */
static double get_time_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void game_init(RaytracerState *state) {
  memset(state, 0, sizeof(*state));
  scene_init(&state->scene);
  camera_init(&state->camera);
  render_settings_init(&state->settings);
  state->active_scale = state->settings.render_scale;
  state->fps_smoothed = 30.0f;
}

void game_update(RaytracerState *state, GameInput *input, float delta_time) {
  int cam_moved = camera_update(&state->camera, input, delta_time);

  /* ── Adaptive quality ──────────────────────────────────────────────────
   * Moving: use user scale but disable AA (1 sample = fast + decent).
   * Still 3+ frames: full quality (user scale + user AA setting).
   *
   * This avoids the scale-3 blockiness while keeping smooth interaction. */
  if (cam_moved) {
    state->still_frames = 0;
  } else {
    if (state->still_frames < 1000) state->still_frames++;
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

  /* Cycle envmap mode: procedural → equirect → cubemap → procedural (L20).
   * Only cycles to modes with loaded data.                                  */
  if (button_just_pressed(input->buttons.cycle_envmap_mode)) {
    EnvMap *em = &state->scene.envmap;
    int next = ((int)em->mode + 1) % 3;
    /* Skip equirect if no equirect data loaded. */
    if (next == ENVMAP_EQUIRECT && !em->pixels)
      next = (next + 1) % 3;
    /* Skip cubemap if no cubemap data loaded. */
    if (next == ENVMAP_CUBEMAP && !em->faces[0].pixels)
      next = (next + 1) % 3;
    em->mode = (EnvMapMode)next;
  }

  /* Render scale cycle (Tab: 2→1→3→2) */
  if (button_just_pressed(input->buttons.scale_cycle)) {
    int s = state->settings.render_scale;
    state->settings.render_scale = (s == 2) ? 1 : (s == 1) ? 3 : 2;
  }

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

void game_render(RaytracerState *state, Backbuffer *bb) {
  /* Build per-frame settings:
   * - While moving: keep user's scale but disable AA for speed.
   * - When still: full user settings (scale + AA).                        */
  RenderSettings frame_settings = state->settings;
  if (state->still_frames < 3) {
    frame_settings.aa_samples = 1; /* single sample while dragging */
  }
  state->active_scale = frame_settings.render_scale;

  /* ── Timed render ──────────────────────────────────────────────────── */
  double t0 = get_time_sec();
  render_scene(bb, &state->scene, &state->camera, &frame_settings);
  double t1 = get_time_sec();

  float render_ms = (float)((t1 - t0) * 1000.0);
  state->frame_time_ms = render_ms;

  /* Exponential moving average for smooth FPS display. */
  float instant_fps = (render_ms > 0.1f) ? (1000.0f / render_ms) : 999.0f;
  state->fps_smoothed = state->fps_smoothed * 0.9f + instant_fps * 0.1f;

  /* ── Compute ray count for display ─────────────────────────────────── */
  int s = frame_settings.render_scale;
  int rw = bb->width / s;
  int rh = bb->height / s;
  int aa = frame_settings.aa_samples;
  int rays_k = (rw * rh * aa) / 1000; /* in thousands */

  /* ── HUD overlay ─────────────────────────────────────────────────────── */
  draw_rect_blend(bb, 4, 4, 520, 68, GAME_RGBA(0, 0, 0, 170));

  char l1[128];
  snprintf(l1, sizeof(l1),
    "%.1f fps | %.1f ms | %dK rays | scale %d (%dx%d) aa:%d %s",
    state->fps_smoothed, render_ms, rays_k,
    s, rw, rh, aa,
    state->still_frames < 3 ? "[moving]" : "");
  draw_text(bb, 8, 8, 1, l1,
    state->fps_smoothed >= 30.0f ? COLOR_GREEN :
    state->fps_smoothed >= 15.0f ? COLOR_YELLOW : COLOR_RED);

  char l2[128];
  snprintf(l2, sizeof(l2),
    "Mouse:orbit/pan/scroll | WASD:move | Tab:scale | Esc:quit");
  draw_text(bb, 8, 20, 1, l2, COLOR_WHITE);

  char l3[160];
  {
    const char *envmap_mode_str =
      state->scene.envmap.mode == ENVMAP_CUBEMAP   ? "cube" :
      state->scene.envmap.mode == ENVMAP_EQUIRECT  ? "eq" : "sky";
    snprintf(l3, sizeof(l3),
      "V:%s F:%s B:%s M:%s R:%s T:%s H:%s E:%s X:%s C:%s",
      state->settings.show_voxels      ? "on" : "--",
      state->settings.show_floor       ? "on" : "--",
      state->settings.show_boxes       ? "on" : "--",
      state->settings.show_meshes      ? "on" : "--",
      state->settings.show_reflections ? "on" : "--",
      state->settings.show_refractions ? "on" : "--",
      state->settings.show_shadows     ? "on" : "--",
      state->settings.show_envmap      ? "on" : "--",
      state->settings.aa_samples >= 4  ? "AA" : "--",
      envmap_mode_str);
  }
  draw_text(bb, 8, 32, 1, l3, COLOR_GREEN);

  char l4[128];
  snprintf(l4, sizeof(l4),
    "dist:%.1f yaw:%.0f pitch:%.0f | P:ppm G:sirds L:glsl",
    state->camera.orbit_radius,
    state->camera.yaw   * (180.0f / (float)M_PI),
    state->camera.pitch  * (180.0f / (float)M_PI));
  draw_text(bb, 8, 44, 1, l4, COLOR_YELLOW);

  /* ── Exports ─────────────────────────────────────────────────────────── */
  if (state->export_ppm_requested) {
    ppm_export(bb, "out.ppm");
    state->export_ppm_requested = 0;
  }
  if (state->export_anaglyph_requested) {
    render_anaglyph(bb->width, bb->height,
                    &state->scene, &state->camera, "anaglyph.ppm",
                    &state->settings);
    state->export_anaglyph_requested = 0;
  }
  if (state->export_sidebyside_requested) {
    render_side_by_side(bb->width, bb->height,
                        &state->scene, &state->camera, "sidebyside.ppm",
                        &state->settings);
    state->export_sidebyside_requested = 0;
  }
  if (state->export_stereogram_requested) {
    render_autostereogram(bb->width, bb->height,
                          &state->scene, &state->camera, "stereogram.ppm",
                          &state->settings);
    state->export_stereogram_requested = 0;
  }
  if (state->export_stereogram_cross_requested) {
    render_autostereogram_crosseyed(bb->width, bb->height,
                                    &state->scene, &state->camera,
                                    "stereogram_cross.ppm",
                                    &state->settings);
    state->export_stereogram_cross_requested = 0;
  }
  if (state->export_glsl_requested) {
    shader_export_glsl("raytracer.glsl");
    state->export_glsl_requested = 0;
  }
}
