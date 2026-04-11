#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * game/render.c — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * PERFORMANCE OPTIMIZATIONS:
 * 1. Camera basis (forward/right/up/tanf/aspect) computed ONCE per frame.
 *    Before: recomputed for every pixel (120K+ redundant normalize+cross).
 * 2. Multi-threaded rendering via pthreads.
 *    Rows are split across CPU cores.  Raytracing is embarrassingly
 *    parallel — no shared mutable state between pixels.
 * 3. Adaptive quality: caller can set render_scale dynamically.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "render.h"
#include "base.h"
#include "ray.h"
#include "raytracer.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h> /* malloc/free for bilinear upscale buffer */
#include <unistd.h> /* sysconf */

/* ── Camera ───────────────────────────────────────────────────────────── */

void camera_init(RtCamera *cam) {
  cam->target = vec3_make(0.0f, 0.0f, -14.0f);
  cam->yaw = 0.0f;
  cam->pitch = 0.2f;
  cam->fov = DEFAULT_FOV;
  cam->orbit_radius = 18.0f;

  float r = cam->orbit_radius;
  cam->position =
      vec3_add(cam->target, vec3_make(r * sinf(cam->yaw) * cosf(cam->pitch),
                                      r * sinf(cam->pitch),
                                      r * cosf(cam->yaw) * cosf(cam->pitch)));
}

static void recompute_position(RtCamera *cam) {
  float r = cam->orbit_radius;
  cam->position =
      vec3_add(cam->target, vec3_make(r * sinf(cam->yaw) * cosf(cam->pitch),
                                      r * sinf(cam->pitch),
                                      r * cosf(cam->yaw) * cosf(cam->pitch)));
}

int camera_update(RtCamera *cam, const void *input_ptr, float delta_time) {
  const GameInput *input = (const GameInput *)input_ptr;
  const MouseState *m = &input->mouse;

  float old_yaw = cam->yaw;
  float old_pitch = cam->pitch;
  float old_radius = cam->orbit_radius;
  Vec3 old_target = cam->target;

  if (m->left_down) {
    cam->yaw -= m->dx * MOUSE_ORBIT_SPEED;
    cam->pitch += m->dy * MOUSE_ORBIT_SPEED;
  }

  if (m->right_down || m->middle_down) {
    Vec3 forward = vec3_normalize(vec3_sub(cam->target, cam->position));
    Vec3 world_up = vec3_make(0.0f, 1.0f, 0.0f);
    Vec3 right = vec3_normalize(vec3_cross(forward, world_up));
    Vec3 up = vec3_cross(right, forward);
    float pan_scale = cam->orbit_radius * MOUSE_PAN_SPEED;
    cam->target =
        vec3_add(cam->target, vec3_add(vec3_scale(right, -m->dx * pan_scale),
                                       vec3_scale(up, m->dy * pan_scale)));
  }

  if (m->scroll != 0.0f)
    cam->orbit_radius -= m->scroll * SCROLL_ZOOM_SPEED;

  if (input->buttons.camera_left.ended_down)
    cam->yaw -= CAMERA_ORBIT_SPEED * delta_time;
  if (input->buttons.camera_right.ended_down)
    cam->yaw += CAMERA_ORBIT_SPEED * delta_time;
  if (input->buttons.camera_up.ended_down)
    cam->pitch += CAMERA_ORBIT_SPEED * delta_time;
  if (input->buttons.camera_down.ended_down)
    cam->pitch -= CAMERA_ORBIT_SPEED * delta_time;
  if (input->buttons.camera_forward.ended_down)
    cam->orbit_radius -= ZOOM_SPEED * delta_time;
  if (input->buttons.camera_backward.ended_down)
    cam->orbit_radius += ZOOM_SPEED * delta_time;

  float max_pitch = (float)M_PI * 0.44f;
  if (cam->pitch > max_pitch)
    cam->pitch = max_pitch;
  if (cam->pitch < -max_pitch)
    cam->pitch = -max_pitch;
  if (cam->orbit_radius < MIN_ORBIT_RADIUS)
    cam->orbit_radius = MIN_ORBIT_RADIUS;
  if (cam->orbit_radius > MAX_ORBIT_RADIUS)
    cam->orbit_radius = MAX_ORBIT_RADIUS;

  recompute_position(cam);

  /* Return 1 if anything changed (for adaptive quality). */
  float dy = cam->yaw - old_yaw;
  float dp = cam->pitch - old_pitch;
  float dr = cam->orbit_radius - old_radius;
  Vec3 dt = vec3_sub(cam->target, old_target);
  return (dy * dy + dp * dp + dr * dr + vec3_dot(dt, dt)) > 1e-10f;
}

/* ── Pre-compute camera basis ONCE per frame ──────────────────────────── */
CameraBasis camera_compute_basis(const RtCamera *cam, int width, int height) {
  GameCamera3D cfg;
  cfg.cam_pos    = cam->position;
  cfg.cam_target = cam->target;
  cfg.cam_up     = vec3_make(0.0f, 1.0f, 0.0f);
  cfg.fov        = cam->fov;
  cfg.near_plane = 0.001f;
  cfg.far_plane  = 1000.0f;
  /* make_render_context_3d needs a Backbuffer for aspect + px_w/px_h. */
  Backbuffer fake_bb = {0};
  fake_bb.width  = width;
  fake_bb.height = height;
  return make_render_context_3d(&fake_bb, cfg);
}

/* ── Sub-pixel ray trace (accepts float offsets for AA jitter) ────────── */
static inline Vec3 trace_subpixel(float fpx, float fpy, int width, int height,
                                  const CameraBasis *basis, const Scene *scene,
                                  const RenderSettings *settings) {
  float x =
      (2.0f * fpx / (float)width - 1.0f) * basis->half_fov_tan * basis->aspect;
  float y = -(2.0f * fpy / (float)height - 1.0f) * basis->half_fov_tan;

  Vec3 dir = vec3_normalize(
      vec3_add(vec3_add(vec3_scale(basis->cam_right, x), vec3_scale(basis->cam_up, y)),
               basis->cam_forward));

  RtRay ray = {.origin = basis->cam_pos, .direction = dir};
  return cast_ray(ray, scene, 0, settings);
}

/* Single-sample pixel trace (center of pixel). */
static inline Vec3 trace_pixel(int px, int py, int width, int height,
                               const CameraBasis *basis, const Scene *scene,
                               const RenderSettings *settings) {
  return trace_subpixel((float)px + 0.5f, (float)py + 0.5f, width, height,
                        basis, scene, settings);
}

/* 2×2 jittered supersampling: 4 sub-pixel rays, averaged.
 * Jitter offsets avoid regular grid artifacts (Poisson-disc approximation).
 * Cost: 4× rays per pixel, but produces smooth sphere edges and clean
 * shadow boundaries.                                                       */
static inline Vec3 trace_pixel_aa4(int px, int py, int width, int height,
                                   const CameraBasis *basis, const Scene *scene,
                                   const RenderSettings *settings) {
  /* Fixed 2×2 rotated grid offsets (better than regular grid for edges). */
  static const float jx[4] = {0.25f, 0.75f, 0.25f, 0.75f};
  static const float jy[4] = {0.25f, 0.25f, 0.75f, 0.75f};

  Vec3 sum = vec3_make(0.0f, 0.0f, 0.0f);
  for (int s = 0; s < 4; s++) {
    Vec3 c = trace_subpixel((float)px + jx[s], (float)py + jy[s], width, height,
                            basis, scene, settings);
    sum = vec3_add(sum, c);
  }
  return vec3_scale(sum, 0.25f);
}

/* ── Multi-threaded rendering ─────────────────────────────────────────── */

typedef struct {
  int start_row, end_row;  /* row range for this thread              */
  int render_w, render_h;  /* internal resolution                    */
  int scale;               /* upscale factor                         */
  int bb_width, bb_height; /* backbuffer dimensions                  */
  int stride;              /* backbuffer stride (pixels)             */
  uint32_t *pixels;        /* backbuffer pixel array                 */
  const CameraBasis *basis;
  const Scene *scene;
  const RenderSettings *settings;
} RenderJob;

/* Choose trace function based on AA setting. */
static inline Vec3 sample_pixel(int px, int py, int w, int h,
                                const RenderJob *job) {
  if (job->settings->aa_samples >= 4)
    return trace_pixel_aa4(px, py, w, h, job->basis, job->scene, job->settings);
  return trace_pixel(px, py, w, h, job->basis, job->scene, job->settings);
}

static inline uint32_t color_to_pixel(Vec3 c) {
  int r = (int)(fminf(1.0f, fmaxf(0.0f, c.x)) * 255.0f);
  int g = (int)(fminf(1.0f, fmaxf(0.0f, c.y)) * 255.0f);
  int b = (int)(fminf(1.0f, fmaxf(0.0f, c.z)) * 255.0f);
  return GAME_RGB(r, g, b);
}

static void *render_thread_fn(void *arg) {
  const RenderJob *job = (const RenderJob *)arg;
  int scale = job->scale;
  int w = job->render_w, h = job->render_h;

  if (scale <= 1) {
    for (int j = job->start_row; j < job->end_row; j++)
      for (int i = 0; i < w; i++)
        job->pixels[j * job->stride + i] =
            color_to_pixel(sample_pixel(i, j, w, h, job));
  } else {
    for (int rj = job->start_row; rj < job->end_row; rj++) {
      for (int ri = 0; ri < w; ri++) {
        uint32_t pixel = color_to_pixel(sample_pixel(ri, rj, w, h, job));
        int by = rj * scale, bx = ri * scale;
        for (int dy = 0; dy < scale && (by + dy) < job->bb_height; dy++)
          for (int dx = 0; dx < scale && (bx + dx) < job->bb_width; dx++)
            job->pixels[(by + dy) * job->stride + (bx + dx)] = pixel;
      }
    }
  }
  return NULL;
}

static int get_num_threads(void) {
  long n = sysconf(_SC_NPROCESSORS_ONLN);
  if (n < 1)
    n = 1;
  if (n > MAX_RENDER_THREADS)
    n = MAX_RENDER_THREADS;
  return (int)n;
}

/* ── Public render functions ──────────────────────────────────────────── */

/* CPU-OPT: bilinear interpolation upscale from float buffer to backbuffer.
 * For each output pixel, maps back to float source coords and blends the
 * 4 nearest source pixels.  Much smoother than block-fill at scale > 1.   */
void bilinear_upscale(const Vec3 *src, int sw, int sh, Backbuffer *bb) {
  int stride = bb->pitch / 4;
  for (int by = 0; by < bb->height; by++) {
    float sy = ((float)by + 0.5f) * (float)sh / (float)bb->height - 0.5f;
    int j0 = (int)sy;
    float fy = sy - (float)j0;
    if (j0 < 0) {
      j0 = 0;
      fy = 0.0f;
    }
    if (j0 >= sh - 1) {
      j0 = sh - 2;
      fy = 1.0f;
    }
    int j1 = j0 + 1;

    for (int bx = 0; bx < bb->width; bx++) {
      float sx = ((float)bx + 0.5f) * (float)sw / (float)bb->width - 0.5f;
      int i0 = (int)sx;
      float fx = sx - (float)i0;
      if (i0 < 0) {
        i0 = 0;
        fx = 0.0f;
      }
      if (i0 >= sw - 1) {
        i0 = sw - 2;
        fx = 1.0f;
      }
      int i1 = i0 + 1;

      /* 4 source pixels */
      Vec3 c00 = src[j0 * sw + i0];
      Vec3 c10 = src[j0 * sw + i1];
      Vec3 c01 = src[j1 * sw + i0];
      Vec3 c11 = src[j1 * sw + i1];

      /* Bilinear blend */
      Vec3 top = vec3_add(vec3_scale(c00, 1.0f - fx), vec3_scale(c10, fx));
      Vec3 bot = vec3_add(vec3_scale(c01, 1.0f - fx), vec3_scale(c11, fx));
      Vec3 c = vec3_add(vec3_scale(top, 1.0f - fy), vec3_scale(bot, fy));

      bb->pixels[by * stride + bx] = color_to_pixel(c);
    }
  }
}

/* Multi-threaded float render (same structure as render_thread_fn but
 * writes Vec3 floats instead of uint32_t pixels — for bilinear input). */
typedef struct {
  int start_row, end_row;
  int render_w, render_h;
  Vec3 *out;
  const CameraBasis *basis;
  const Scene *scene;
  const RenderSettings *settings;
} FloatRenderJob;

static void *float_render_thread_fn(void *arg) {
  const FloatRenderJob *job = (const FloatRenderJob *)arg;
  int w = job->render_w, h = job->render_h;
  for (int j = job->start_row; j < job->end_row; j++) {
    for (int i = 0; i < w; i++) {
      if (job->settings->aa_samples >= 4)
        job->out[j * w + i] =
            trace_pixel_aa4(i, j, w, h, job->basis, job->scene, job->settings);
      else
        job->out[j * w + i] =
            trace_pixel(i, j, w, h, job->basis, job->scene, job->settings);
    }
  }
  return NULL;
}

void render_scene_to_float_mt(Vec3 *framebuffer, int width, int height,
                              const Scene *scene, const RtCamera *cam,
                              const RenderSettings *settings) {
  CameraBasis basis = camera_compute_basis(cam, width, height);
  int num_threads = get_num_threads();
  if (num_threads > height)
    num_threads = height;

  if (num_threads <= 1) {
    FloatRenderJob job = {
        .start_row = 0,
        .end_row = height,
        .render_w = width,
        .render_h = height,
        .out = framebuffer,
        .basis = &basis,
        .scene = scene,
        .settings = settings,
    };
    float_render_thread_fn(&job);
  } else {
    pthread_t threads[MAX_RENDER_THREADS];
    FloatRenderJob jobs[MAX_RENDER_THREADS];
    int rows_per = height / num_threads;
    int rem = height % num_threads;
    int row = 0;
    for (int t = 0; t < num_threads; t++) {
      int count = rows_per + (t < rem ? 1 : 0);
      jobs[t] = (FloatRenderJob){
          .start_row = row,
          .end_row = row + count,
          .render_w = width,
          .render_h = height,
          .out = framebuffer,
          .basis = &basis,
          .scene = scene,
          .settings = settings,
      };
      pthread_create(&threads[t], NULL, float_render_thread_fn, &jobs[t]);
      row += count;
    }
    for (int t = 0; t < num_threads; t++)
      pthread_join(threads[t], NULL);
  }
}

void render_scene(Backbuffer *bb, const Scene *scene, const RtCamera *cam,
                  const RenderSettings *settings) {
  int stride = bb->pitch / 4;
  int scale =
      (settings && settings->render_scale > 1) ? settings->render_scale : 1;
  int render_w = bb->width / scale;
  int render_h = bb->height / scale;

  /* CPU-OPT at scale > 1: render to float buffer, bilinear upscale. */
  if (settings && settings->render_mode == RENDER_CPU_OPT && scale > 1) {
    /* Stack-allocate for small buffers, heap for large. */
    Vec3 stack_buf[400 * 300]; /* enough for scale=2 at 800x600 */
    Vec3 *fbuf = stack_buf;
    int need = render_w * render_h;
    if (need > (int)(sizeof(stack_buf) / sizeof(stack_buf[0]))) {
      fbuf = (Vec3 *)malloc((size_t)need * sizeof(Vec3));
      if (!fbuf)
        fbuf = stack_buf; /* fallback */
    }
    render_scene_to_float_mt(fbuf, render_w, render_h, scene, cam, settings);
    bilinear_upscale(fbuf, render_w, render_h, bb);
    if (fbuf != stack_buf)
      free(fbuf);
    return;
  }

  /* Compute camera basis ONCE for the entire frame. */
  CameraBasis basis = camera_compute_basis(cam, render_w, render_h);

  int num_threads = get_num_threads();
  /* Don't use more threads than rows. */
  if (num_threads > render_h)
    num_threads = render_h;

  if (num_threads <= 1) {
    /* Single-threaded fallback. */
    RenderJob job = {
        .start_row = 0,
        .end_row = render_h,
        .render_w = render_w,
        .render_h = render_h,
        .scale = scale,
        .bb_width = bb->width,
        .bb_height = bb->height,
        .stride = stride,
        .pixels = bb->pixels,
        .basis = &basis,
        .scene = scene,
        .settings = settings,
    };
    render_thread_fn(&job);
  } else {
    /* Multi-threaded: split rows evenly. */
    pthread_t threads[MAX_RENDER_THREADS];
    RenderJob jobs[MAX_RENDER_THREADS];

    int rows_per_thread = render_h / num_threads;
    int remainder = render_h % num_threads;

    int row = 0;
    for (int t = 0; t < num_threads; t++) {
      int count = rows_per_thread + (t < remainder ? 1 : 0);
      jobs[t] = (RenderJob){
          .start_row = row,
          .end_row = row + count,
          .render_w = render_w,
          .render_h = render_h,
          .scale = scale,
          .bb_width = bb->width,
          .bb_height = bb->height,
          .stride = stride,
          .pixels = bb->pixels,
          .basis = &basis,
          .scene = scene,
          .settings = settings,
      };
      pthread_create(&threads[t], NULL, render_thread_fn, &jobs[t]);
      row += count;
    }

    for (int t = 0; t < num_threads; t++)
      pthread_join(threads[t], NULL);
  }
}

void render_scene_to_float(Vec3 *framebuffer, int width, int height,
                           const Scene *scene, const RtCamera *cam,
                           const RenderSettings *settings) {
  CameraBasis basis = camera_compute_basis(cam, width, height);
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      framebuffer[j * width + i] =
          trace_pixel(i, j, width, height, &basis, scene, settings);
    }
  }
}
