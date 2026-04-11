#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H

/* ── Rendering mode (N key cycles) ─────────────────────────────────────── */
typedef enum {
  RENDER_CPU_BASIC = 0, /* L01-L21: original CPU raytracer (baseline)    */
  RENDER_CPU_OPT,       /* L22: frame cap + bilinear upscale + octant BVH */
  RENDER_GPU,           /* L23: OpenGL fragment shader (Shadertoy GLSL)   */
  RENDER_MODE_COUNT
} RenderMode;

typedef struct {
  int show_voxels;
  int show_floor;
  int show_boxes;
  int show_meshes;
  int show_reflections;
  int show_refractions;
  int show_shadows;
  int show_envmap;        /* use envmap instead of procedural sky           */
  int render_scale;       /* 1=full, 2=half (default), 3=quarter           */
  int aa_samples;         /* 1=off, 4=2×2 jittered SSAA                    */
  RenderMode render_mode; /* CPU_BASIC / CPU_OPT / GPU                    */
} RenderSettings;

static inline void render_settings_init(RenderSettings *s) {
  s->show_voxels = 1;
  s->show_floor = 1;
  s->show_boxes = 1;
  s->show_meshes = 1;
  s->show_reflections = 1;
  s->show_refractions = 1;
  s->show_shadows = 1;
  s->show_envmap = 1;
  s->render_scale = 2;
  s->aa_samples = 4;
  s->render_mode = RENDER_CPU_BASIC;
}

#endif /* GAME_SETTINGS_H */
