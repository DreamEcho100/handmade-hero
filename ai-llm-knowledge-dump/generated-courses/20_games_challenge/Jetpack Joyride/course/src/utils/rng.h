#ifndef UTILS_RNG_H
#define UTILS_RNG_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/rng.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Xorshift32 pseudo-random number generator.
 * Fast, deterministic, sufficient for gameplay randomness.
 *
 * Adapted from Handmade Hero's RNG pattern.
 * Separate game RNG from effects RNG to preserve determinism.
 *
 * LESSON 12 — Introduced for obstacle variety selection.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>

typedef struct {
  uint32_t state; /* Must be nonzero. */
} RNG;

/* Seed the RNG. seed must be nonzero; if zero, defaults to 1. */
static inline void rng_seed(RNG *r, uint32_t seed) {
  r->state = seed ? seed : 1;
}

/* Generate next pseudo-random uint32_t (xorshift32 algorithm). */
static inline uint32_t rng_next(RNG *r) {
  uint32_t x = r->state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  r->state = x;
  return x;
}

/* Random integer in [min, max] (inclusive). */
static inline int rng_range(RNG *r, int min, int max) {
  if (min >= max) return min;
  uint32_t range = (uint32_t)(max - min + 1);
  return min + (int)(rng_next(r) % range);
}

/* Random float in [0.0, 1.0). */
static inline float rng_float(RNG *r) {
  return (float)(rng_next(r) & 0x00FFFFFF) / (float)0x01000000;
}

/* Random float in [min, max). */
static inline float rng_float_range(RNG *r, float min, float max) {
  return min + rng_float(r) * (max - min);
}

#endif /* UTILS_RNG_H */
