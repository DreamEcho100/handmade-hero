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
  int show_envmap;       /* use envmap instead of procedural sky           */
  int render_scale;      /* 1=full, 2=half (default), 3=quarter           */
  int aa_samples;        /* 1=off, 4=2×2 jittered SSAA                    */
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
  s->render_scale     = 2;
  s->aa_samples       = 4;
}

#endif /* GAME_SETTINGS_H */
