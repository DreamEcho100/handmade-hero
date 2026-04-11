#ifndef UTILS_RENDER3D_H
#define UTILS_RENDER3D_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/render3d.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 3D camera config → baked context → projection helpers.
 * Mirrors the 2D pattern in render.h:
 *
 *   2D: GameWorldConfig  → make_render_context(bb, cfg)     → RenderContext
 *   3D: GameCamera3D     → make_render_context_3d(bb, cam)  → RenderContext3D
 *
 * Usage:
 *   #include "utils/render3d.h"   // in game layer
 *
 *   GameCamera3D cam = { .cam_pos = ..., .cam_target = ..., ... };
 *   RenderContext3D ctx = make_render_context_3d(bb, cam);
 *   // per-pixel: project_3d_to_screen(&ctx, world_pos, &px, &py, NULL)
 *
 * NOTE: Raylib defines Camera3D — this file uses GameCamera3D to avoid
 * naming conflicts.
 *
 * COORDINATE SYSTEM
 * ─────────────────
 * Right-handed, Y-up.  cam_forward = normalize(cam_target - cam_pos).
 * cam_right  = normalize(cross(cam_forward, world_up)).
 * cam_up     = cross(cam_right, cam_forward).
 *
 * Screen space: pixel (0,0) is top-left.  NDC x: -1=left, +1=right.
 *               NDC y: +1=top, -1=bottom  (y flipped for screen).
 *
 * half_fov_tan = tan(fov / 2)  — baked once, reused every pixel.
 *
 * HH COMPARISON
 * ─────────────
 * Handmade Hero bakes the camera basis implicitly in its software
 * renderer.  This file makes it explicit and configurable — same
 * performance, visible boundary, teachable.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <math.h>
#include "./backbuffer.h"
#include "./vec3.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* ── GameCamera3D — input config, set once by game layer ─────────────────
 *
 * ZII-safe: a zero-initialised GameCamera3D is not useful (cam_pos ==
 * cam_target → degenerate forward), but make_render_context_3d() guards
 * against a zero fov by falling back to M_PI/3 (60°).
 */
typedef struct {
  Vec3  cam_pos;     /* world-space camera position                */
  Vec3  cam_target;  /* look-at point                              */
  Vec3  cam_up;      /* world up — ZII (0,0,0) falls back to Y-up  */
  float fov;         /* vertical FoV in radians; ZII falls back to 60° */
  float near_plane;  /* ZII falls back to 0.001                    */
  float far_plane;   /* ZII falls back to 1000.0                   */
} GameCamera3D;

/* ── RenderContext3D — baked per-frame, built by make_render_context_3d() ─
 *
 * All fields computed once; subsequent draw/project calls are branch-free
 * float math.  Analogous to RenderContext for 2D.
 */
typedef struct {
  Vec3  cam_pos;       /* world position                            */
  Vec3  cam_forward;   /* unit forward vector                       */
  Vec3  cam_right;     /* unit right vector                         */
  Vec3  cam_up;        /* unit up vector (reorthogonalised)         */
  float half_fov_tan;  /* tan(fov/2) — baked for ray generation     */
  float aspect;        /* bb->width / bb->height                    */
  float near_plane;    /* geometry behind this is clipped           */
  float far_plane;     /* geometry beyond this is clipped           */
  float px_w, px_h;   /* backbuffer dimensions (for projection)    */
} RenderContext3D;

/* ── make_render_context_3d — build RenderContext3D from bb + camera ─────
 *
 * Call ONCE per frame.  Pass the result (by pointer) to draw helpers.
 * ZII fallbacks: fov → 60°, cam_up → Y-up, near → 0.001, far → 1000.
 */
static inline RenderContext3D make_render_context_3d(const Backbuffer *bb,
                                                     GameCamera3D cam) {
  RenderContext3D ctx;

  ctx.cam_pos = cam.cam_pos;

  /* Forward: normalize(target - pos).  Degenerate if pos == target. */
  ctx.cam_forward = vec3_normalize(vec3_sub(cam.cam_target, cam.cam_pos));

  /* World up: fall back to Y-up when zero-initialised. */
  Vec3 world_up = (cam.cam_up.x == 0.0f &&
                   cam.cam_up.y == 0.0f &&
                   cam.cam_up.z == 0.0f)
                  ? vec3_make(0.0f, 1.0f, 0.0f)
                  : cam.cam_up;

  /* Reorthogonalise: right = normalize(cross(forward, world_up)). */
  ctx.cam_right = vec3_normalize(vec3_cross(ctx.cam_forward, world_up));
  /* Up = cross(right, forward) — already unit length. */
  ctx.cam_up    = vec3_cross(ctx.cam_right, ctx.cam_forward);

  /* half_fov_tan: ZII fov=0 falls back to 60° (M_PI/3). */
  float fov_rad    = (cam.fov > 0.0f) ? cam.fov : (float)(M_PI / 3.0);
  ctx.half_fov_tan = tanf(fov_rad * 0.5f);

  ctx.aspect    = (bb->height > 0)
                  ? (float)bb->width / (float)bb->height
                  : 1.0f;
  ctx.near_plane = (cam.near_plane > 0.0f) ? cam.near_plane : 0.001f;
  ctx.far_plane  = (cam.far_plane  > 0.0f) ? cam.far_plane  : 1000.0f;
  ctx.px_w       = (float)bb->width;
  ctx.px_h       = (float)bb->height;

  return ctx;
}

/* ── project_3d_to_screen — world pos → pixel ────────────────────────────
 *
 * Returns 1 if in front of near plane; 0 if behind (clipped).
 * *px, *py   = pixel coordinates (top-left origin, matches backbuffer).
 * *depth     = distance along forward axis from camera (may be NULL).
 */
static inline int project_3d_to_screen(const RenderContext3D *ctx,
                                        Vec3 world_pos,
                                        float *px, float *py, float *depth) {
  Vec3  delta = vec3_sub(world_pos, ctx->cam_pos);
  float d     = vec3_dot(delta, ctx->cam_forward);
  if (d < ctx->near_plane) return 0;
  if (depth) *depth = d;

  float rx     = vec3_dot(delta, ctx->cam_right);
  float ry     = vec3_dot(delta, ctx->cam_up);
  float inv_d  = 1.0f / d;
  /* NDC: sx in [-1,+1], sy in [-1,+1] (sy positive = up in world). */
  float sx = (rx * inv_d) / (ctx->half_fov_tan * ctx->aspect);
  float sy = (ry * inv_d) /  ctx->half_fov_tan;
  /* Convert NDC → pixel.  Y is flipped: NDC +1 = pixel top (y=0). */
  if (px) *px = ( sx * 0.5f + 0.5f) * ctx->px_w;
  if (py) *py = (-sy * 0.5f + 0.5f) * ctx->px_h;
  return 1;
}

/* ── unproject_screen_to_ray — pixel → world-space ray ──────────────────
 *
 * Converts a pixel coordinate to a world-space ray for mouse picking,
 * cursor intersection, and similar operations.
 * out_origin = camera position (ray starts here).
 * out_dir    = unit direction into the scene.
 */
static inline void unproject_screen_to_ray(const RenderContext3D *ctx,
                                            float px, float py,
                                            Vec3 *out_origin, Vec3 *out_dir) {
  /* Pixel → NDC → camera space direction. */
  float sx = ( px / ctx->px_w * 2.0f - 1.0f) * ctx->half_fov_tan * ctx->aspect;
  float sy = (-py / ctx->px_h * 2.0f + 1.0f) * ctx->half_fov_tan;
  *out_origin = ctx->cam_pos;
  *out_dir    = vec3_normalize(
    vec3_add(
      vec3_add(vec3_scale(ctx->cam_right,   sx),
               vec3_scale(ctx->cam_up,      sy)),
      ctx->cam_forward));
}

/* ── world_pos_3d_is_visible — sphere frustum test ───────────────────────
 *
 * Tests whether a sphere (center, radius) is visible within the near/far
 * range.  A quick depth-only test — no lateral frustum planes.
 * Returns 1 if potentially visible; 0 if culled.
 */
static inline int world_pos_3d_is_visible(const RenderContext3D *ctx,
                                           Vec3 center, float radius) {
  Vec3  delta = vec3_sub(center, ctx->cam_pos);
  float d     = vec3_dot(delta, ctx->cam_forward);
  return (d + radius >= ctx->near_plane) && (d - radius <= ctx->far_plane);
}

#endif /* UTILS_RENDER3D_H */
