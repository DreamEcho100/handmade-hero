/* ═══════════════════════════════════════════════════════════════════════════
 * utils/particles.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./particles.h"
#include "./backbuffer.h"
#include "../utils/rng.h"

#include <math.h>
#include <string.h>

/* Particle-only RNG (isolated from gameplay entropy). */
static RNG s_particle_rng = {.state = 54321};

static float randf(float min, float max) {
  return rng_float_range(&s_particle_rng, min, max);
}

void particles_init(ParticleSystem *ps) {
  memset(ps, 0, sizeof(*ps));
}

void particles_emit(ParticleSystem *ps, float x, float y, int count,
                    const ParticleConfig *config) {
  for (int i = 0; i < count; i++) {
    Particle *p = &ps->pool[ps->write_idx];

    float angle = randf(config->angle_min, config->angle_max);
    float speed = randf(config->speed_min, config->speed_max);

    p->x = x;
    p->y = y;
    p->vx = cosf(angle) * speed;
    p->vy = sinf(angle) * speed;
    p->life = randf(config->life_min, config->life_max);
    p->max_life = p->life;
    p->size = randf(config->size_min, config->size_max);
    p->color = config->color;

    ps->write_idx = (ps->write_idx + 1) % MAX_PARTICLES;
    if (ps->active_count < MAX_PARTICLES)
      ps->active_count++;
  }
}

void particles_update(ParticleSystem *ps, float dt) {
  int alive = 0;
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle *p = &ps->pool[i];
    if (p->life <= 0.0f) continue;

    p->life -= dt;
    if (p->life <= 0.0f) {
      p->life = 0.0f;
      continue;
    }

    p->x += p->vx * dt;
    p->y += p->vy * dt;

    /* Gravity on particles */
    p->vy += 50.0f * dt;

    alive++;
  }
  ps->active_count = alive;
}

void particles_draw(const ParticleSystem *ps, Backbuffer *bb, float camera_x) {
  int bb_pitch = bb->pitch / 4;

  for (int i = 0; i < MAX_PARTICLES; i++) {
    const Particle *p = &ps->pool[i];
    if (p->life <= 0.0f) continue;

    /* Alpha fade based on remaining life */
    float alpha = p->life / p->max_life;
    if (alpha > 1.0f) alpha = 1.0f;

    int px = (int)(p->x - camera_x);
    int py = (int)p->y;
    int sz = (int)p->size;
    if (sz < 1) sz = 1;

    /* Extract color channels */
    uint32_t c = p->color;
    unsigned int cr = (c >> 16) & 0xFF;
    unsigned int cg = (c >>  8) & 0xFF;
    unsigned int cb =  c        & 0xFF;
    unsigned int ca = (unsigned int)(alpha * 255.0f);

    uint32_t final_color = (ca << 24) | (cr << 16) | (cg << 8) | cb;

    /* Draw as filled square */
    for (int dy = 0; dy < sz; dy++) {
      int ry = py + dy;
      if (ry < 0 || ry >= bb->height) continue;
      for (int dx = 0; dx < sz; dx++) {
        int rx = px + dx;
        if (rx < 0 || rx >= bb->width) continue;

        if (ca >= 200) {
          /* Near-opaque: direct write */
          bb->pixels[ry * bb_pitch + rx] = final_color;
        } else if (ca > 0) {
          /* Alpha blend */
          uint32_t dst = bb->pixels[ry * bb_pitch + rx];
          unsigned int dr = (dst >> 16) & 0xFF;
          unsigned int dg = (dst >>  8) & 0xFF;
          unsigned int db =  dst        & 0xFF;
          unsigned int inv_a = 255 - ca;
          unsigned int or_ = (cr * ca + dr * inv_a) / 255;
          unsigned int og  = (cg * ca + dg * inv_a) / 255;
          unsigned int ob  = (cb * ca + db * inv_a) / 255;
          bb->pixels[ry * bb_pitch + rx] =
              ((uint32_t)0xFF << 24) | (or_ << 16) | (og << 8) | ob;
        }
      }
    }
  }
}
