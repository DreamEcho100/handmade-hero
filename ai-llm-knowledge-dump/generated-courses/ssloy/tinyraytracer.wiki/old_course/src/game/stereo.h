#ifndef GAME_STEREO_H
#define GAME_STEREO_H

#include "scene.h"
#include "render.h"
#include <stdint.h>

#define EYE_SEPARATION 0.065f

void render_anaglyph(int width, int height,
                     const Scene *scene, const RtCamera *cam,
                     const char *filename, const RenderSettings *settings);

void render_side_by_side(int width, int height,
                         const Scene *scene, const RtCamera *cam,
                         const char *filename, const RenderSettings *settings);

#endif /* GAME_STEREO_H */
