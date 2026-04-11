#ifndef GAME_RAYTRACER_H
#define GAME_RAYTRACER_H

#include "vec3.h"
#include "ray.h"
#include "scene.h"
#include "settings.h"

#define MAX_RECURSION_DEPTH 3

Vec3 cast_ray(RtRay ray, const Scene *scene, int depth,
              const RenderSettings *settings);

#endif /* GAME_RAYTRACER_H */
