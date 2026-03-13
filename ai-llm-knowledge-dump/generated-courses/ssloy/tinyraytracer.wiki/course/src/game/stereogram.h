#ifndef GAME_STEREOGRAM_H
#define GAME_STEREOGRAM_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/stereogram.h — TinyRaytracer Course (L15)
 * ═══════════════════════════════════════════════════════════════════════════
 * Autostereogram (SIRDS) generation — Part 2 of ssloy's wiki.
 *
 * WALL-EYED vs CROSS-EYED (from ssloy Part 2, lines 70-83):
 * ──────────────────────────────────────────────────────────
 * The same stereogram can be viewed two ways:
 *   WALL-EYED:  eyes diverge (focus behind screen) → near objects pop OUT
 *   CROSS-EYED: eyes converge (focus in front)     → near objects pop IN
 *
 * Using the wrong method inverts the depth — hills become valleys.
 * The `cross_eyed` flag swaps the parallax direction to produce a
 * stereogram correct for cross-eyed viewing.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "scene.h"
#include "render.h"

/* Near plane as fraction of far plane distance (ssloy uses 1/3). */
#define STEREO_MU         0.33f

/* Interpupillary distance in pixels (ssloy uses 400). */
#define STEREOGRAM_EYE_PX 400

/* Render a wall-eyed autostereogram (default: G key). */
void render_autostereogram(int width, int height,
                           const Scene *scene, const RtCamera *cam,
                           const char *filename, const RenderSettings *settings);

/* Render a cross-eyed autostereogram (Shift+G key).
 * Same algorithm but parallax constraint is reversed:
 * `uf_union(right, left)` instead of `uf_union(left, right)`.            */
void render_autostereogram_crosseyed(int width, int height,
                                     const Scene *scene, const RtCamera *cam,
                                     const char *filename,
                                     const RenderSettings *settings);

#endif /* GAME_STEREOGRAM_H */
