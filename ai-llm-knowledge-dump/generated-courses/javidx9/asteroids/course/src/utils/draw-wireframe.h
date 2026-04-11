#ifndef UTILS_DRAW_WIREFRAME_H
#define UTILS_DRAW_WIREFRAME_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-wireframe.h — Wireframe renderer for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 05 — draw_wireframe: render a polygon model as a closed loop of
 * line segments.  This is the core primitive for ship, asteroid, and bullet
 * visual representation in Asteroids.
 *
 * HOW IT WORKS
 * ────────────
 * Given a list of 2D vertices in LOCAL SPACE (centered on origin, unit scale):
 *   1. For each vertex: rotate by `angle`, scale by `scale`, translate to (cx, cy).
 *   2. Convert each transformed world position to pixels via render_explicit.h.
 *   3. Draw a line between consecutive pixel positions.
 *   4. Close the loop by connecting the last vertex to the first.
 *
 * COORDINATE SYSTEM
 * ─────────────────
 * Models are defined in y-up Cartesian space:
 *   - (0, +1)  = "up" on screen (ship nose direction at angle=0)
 *   - (0, -1)  = "down" on screen
 *   - (-1, 0)  = "left" on screen
 *   - (+1, 0)  = "right" on screen
 *
 * Rotation is CLOCKWISE (increasing angle = CW on screen) — see vec2_rotate
 * in utils/math.h for the CW matrix derivation.
 *
 * PERFORMANCE NOTE
 * ────────────────
 * The caller computes cosf/sinf ONCE per object (not per vertex).
 * With 3 vertices (ship), 7 vertices (asteroid), this halves trig calls.
 *
 * Usage:
 *   float ca = cosf(ship.angle), sa = sinf(ship.angle);
 *   draw_wireframe(bb, &ctx, SHIP_MODEL, SHIP_VERT_COUNT,
 *                  ship.x, ship.y, ca, sa, SHIP_RENDER_SCALE,
 *                  COLOR_CYAN);
 *
 * The COLOR_CYAN macro expands to four float args: r, g, b, a.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./backbuffer.h"
#include "./render.h"       /* RenderContext, world_x, world_y */
#include "./math.h"         /* Vec2, vec2_rotate, vec2_scale, vec2_add */
#include "./draw-shapes.h"  /* draw_line */

/* draw_wireframe — render a closed polygon wireframe.
 *
 * bb:         destination backbuffer
 * ctx:        render context (world→pixel coefficients)
 * model:      array of 2D vertices in local space (unit scale, y-up)
 * vert_count: number of vertices (polygon is closed: last vertex connects
 *             back to first)
 * cx, cy:     world-space center position of the object
 * cos_a,sin_a: precomputed cosf/sinf of the rotation angle (CW rotation)
 * scale:      world-space scale factor (size of the rendered wireframe)
 * r,g,b,a:    float RGBA color channels [0.0–1.0]
 */
static inline void draw_wireframe(Backbuffer *bb, const RenderContext *ctx,
                                  const Vec2 *model, int vert_count,
                                  float cx, float cy,
                                  float cos_a, float sin_a,
                                  float scale,
                                  float r, float g, float b, float a) {
  if (!bb || !model || vert_count < 2) return;

  for (int i = 0; i < vert_count; i++) {
    int j = (i + 1) % vert_count;

    /* Transform vertex i: rotate then scale then translate to world space */
    Vec2 vi = vec2_rotate(model[i], cos_a, sin_a);
    vi = vec2_scale(vi, scale);
    vi = vec2_add(vi, (Vec2){ cx, cy });

    /* Transform vertex j */
    Vec2 vj = vec2_rotate(model[j], cos_a, sin_a);
    vj = vec2_scale(vj, scale);
    vj = vec2_add(vj, (Vec2){ cx, cy });

    /* Convert to pixel space using explicit coord helpers */
    float px0 = world_x(ctx, vi.x);
    float py0 = world_y(ctx, vi.y);
    float px1 = world_x(ctx, vj.x);
    float py1 = world_y(ctx, vj.y);

    draw_line(bb, px0, py0, px1, py1, r, g, b, a);
  }
}

#endif /* UTILS_DRAW_WIREFRAME_H */
