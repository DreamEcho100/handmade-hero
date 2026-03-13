#ifndef GAME_LIGHTING_H
#define GAME_LIGHTING_H

#include "vec3.h"
#include "scene.h"
#include "settings.h"

typedef struct {
  float diffuse_intensity;
  float specular_intensity;
} LightingResult;

LightingResult compute_lighting(Vec3 point, Vec3 normal, Vec3 view_dir,
                                const RtMaterial *material,
                                const Scene *scene,
                                const RenderSettings *settings);

#endif /* GAME_LIGHTING_H */
