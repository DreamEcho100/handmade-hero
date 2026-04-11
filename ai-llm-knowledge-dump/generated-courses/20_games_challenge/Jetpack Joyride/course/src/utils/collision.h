#ifndef UTILS_COLLISION_H
#define UTILS_COLLISION_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/collision.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * AABB (Axis-Aligned Bounding Box) collision detection.
 * Uses top-left origin (x, y) with width and height — matches our
 * COORD_ORIGIN_LEFT_TOP coordinate system.
 *
 * LESSON 10 — AABB overlap, point-in-box, near-miss detection.
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* ── AABB ────────────────────────────────────────────────────────────── */
/* x, y: top-left corner. w, h: dimensions. All in world units.          */
typedef struct {
  float x, y, w, h;
} AABB;

/* Construct an AABB from center point and half-extents. */
static inline AABB aabb_from_center(float cx, float cy, float hw, float hh) {
  AABB r = {cx - hw, cy - hh, hw * 2.0f, hh * 2.0f};
  return r;
}

/* Construct an AABB from top-left position and size. */
static inline AABB aabb_make(float x, float y, float w, float h) {
  AABB r = {x, y, w, h};
  return r;
}

/* ── Overlap test ────────────────────────────────────────────────────── */
/* Returns 1 if a and b overlap (share at least one point). */
static inline int aabb_overlap(AABB a, AABB b) {
  return (a.x < b.x + b.w) && (a.x + a.w > b.x) &&
         (a.y < b.y + b.h) && (a.y + a.h > b.y);
}

/* ── Point-in-box ────────────────────────────────────────────────────── */
/* Returns 1 if point (px, py) is inside the box. */
static inline int aabb_contains_point(AABB box, float px, float py) {
  return (px >= box.x) && (px < box.x + box.w) &&
         (py >= box.y) && (py < box.y + box.h);
}

/* ── Near-miss detection ─────────────────────────────────────────────── */
/* Returns 1 if `entity` is within `margin` of `obstacle` but NOT
 * actually overlapping. Used for the near-miss bonus (+50 pts).
 *
 * Algorithm: expand obstacle by margin on all sides, check if entity
 * overlaps the expanded box but NOT the original box.                   */
static inline int aabb_near_miss(AABB entity, AABB obstacle, float margin) {
  /* Expanded obstacle */
  AABB expanded = {
    obstacle.x - margin,
    obstacle.y - margin,
    obstacle.w + margin * 2.0f,
    obstacle.h + margin * 2.0f,
  };
  return aabb_overlap(entity, expanded) && !aabb_overlap(entity, obstacle);
}

#endif /* UTILS_COLLISION_H */
