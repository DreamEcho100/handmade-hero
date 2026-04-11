#ifndef GAME_RENDER_H
#define GAME_RENDER_H

#include "../utils/backbuffer.h"
#include "../utils/render3d.h"
#include "scene.h"
#include "settings.h"
#include "vec3.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CAMERA_ORBIT_SPEED 1.5f
#define MOUSE_ORBIT_SPEED 0.005f
#define MOUSE_PAN_SPEED 0.01f
#define ZOOM_SPEED 0.5f
#define SCROLL_ZOOM_SPEED 0.3f
#define DEFAULT_FOV ((float)M_PI / 3.0f)
#define DEFAULT_ORBIT_RADIUS 5.0f
#define MIN_ORBIT_RADIUS 1.0f
#define MAX_ORBIT_RADIUS 50.0f

/* Max threads for parallel rendering. Auto-detected at runtime. */
#define MAX_RENDER_THREADS 32

typedef struct {
  Vec3 position;
  Vec3 target;
  float yaw;
  float pitch;
  float fov;
  float orbit_radius;
} RtCamera;

/* Pre-computed per-frame camera data (computed ONCE, used by all pixels).
 * CameraBasis is an alias for RenderContext3D — the same struct from
 * utils/render3d.h.  Field mapping:
 *   origin   → cam_pos       forward → cam_forward
 *   right    → cam_right     up      → cam_up
 *   half_fov → half_fov_tan  aspect  → aspect (unchanged)
 */
typedef RenderContext3D CameraBasis;

void camera_init(RtCamera *cam);

/* Returns 1 if camera moved this frame, 0 if unchanged. */
int camera_update(RtCamera *cam, const void *input, float delta_time);

CameraBasis camera_compute_basis(const RtCamera *cam, int width, int height);

void render_scene(Backbuffer *bb, const Scene *scene, const RtCamera *cam,
                  const RenderSettings *settings);

void render_scene_to_float(Vec3 *framebuffer, int width, int height,
                           const Scene *scene, const RtCamera *cam,
                           const RenderSettings *settings);

/* Multi-threaded float render (used by CPU-OPT bilinear upscale path). */
void render_scene_to_float_mt(Vec3 *framebuffer, int width, int height,
                              const Scene *scene, const RtCamera *cam,
                              const RenderSettings *settings);

/* Bilinear upscale from float buffer to backbuffer (L22 CPU-OPT). */
void bilinear_upscale(const Vec3 *src, int sw, int sh, Backbuffer *bb);

#endif /* GAME_RENDER_H */
