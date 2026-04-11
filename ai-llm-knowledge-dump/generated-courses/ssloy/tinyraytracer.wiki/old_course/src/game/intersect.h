#ifndef GAME_INTERSECT_H
#define GAME_INTERSECT_H

#include "ray.h"
#include "scene.h"
#include "settings.h"

int sphere_intersect(RtRay ray, const Sphere *sphere, HitRecord *hit);
int plane_intersect(RtRay ray, HitRecord *hit);
int box_intersect(RtRay ray, const Box *box, HitRecord *hit);

/* Test ray against all objects in scene.  `settings` controls which object
 * types are active (voxels, floor, boxes).  Pass NULL for all-on.         */
int scene_intersect(RtRay ray, const Scene *scene, HitRecord *hit,
                    Vec3 *voxel_color_out, const RenderSettings *settings);

#endif /* GAME_INTERSECT_H */
