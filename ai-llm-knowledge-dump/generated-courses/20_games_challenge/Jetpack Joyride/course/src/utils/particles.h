#ifndef UTILS_PARTICLES_H
#define UTILS_PARTICLES_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/particles.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ring-buffer particle pool for visual effects: jetpack exhaust, death
 * burst, shield break sparkle, pellet collect flash.
 *
 * LESSON 20 — Particle system.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include "./backbuffer.h"

typedef struct {
  float x, y;          /* position */
  float vx, vy;        /* velocity */
  float life;          /* remaining life (seconds) */
  float max_life;      /* initial life (for alpha fade) */
  uint32_t color;      /* packed RGBA */
  float size;          /* pixel size */
} Particle;

#define MAX_PARTICLES 128

typedef struct {
  Particle pool[MAX_PARTICLES];
  int write_idx;       /* next write position (ring buffer) */
  int active_count;    /* number of alive particles */
} ParticleSystem;

/* Emission configuration for a burst. */
typedef struct {
  float speed_min, speed_max;   /* Particle speed range */
  float angle_min, angle_max;   /* Emission cone (radians) */
  float life_min, life_max;     /* Lifetime range */
  float size_min, size_max;     /* Size range */
  uint32_t color;               /* Base color */
} ParticleConfig;

/* Initialize the particle system. */
void particles_init(ParticleSystem *ps);

/* Emit count particles at (x, y) with the given configuration. */
void particles_emit(ParticleSystem *ps, float x, float y, int count,
                    const ParticleConfig *config);

/* Update all particles (integrate position, age out dead ones). */
void particles_update(ParticleSystem *ps, float dt);

/* Render all alive particles. camera_x offsets for scrolling. */
void particles_draw(const ParticleSystem *ps, Backbuffer *bb, float camera_x);

#endif /* UTILS_PARTICLES_H */
