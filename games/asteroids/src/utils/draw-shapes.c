#include "../utils/draw-shapes.h"
#include "../utils/math.h"

void draw_rect(Backbuffer *backbuffer, float x, float y, float w, float h,
               float r, float g, float b, float a) {
  if (!backbuffer || !backbuffer->pixels)
    return;

  /* ── Tier 1: fully transparent → skip entirely ──────────── */
  if (a <= 0.0f)
    return;

  /* ── Clip rectangle to backbuffer bounds ─────────────────── */
  int x0 = MAX((int)x, 0);
  int y0 = MAX((int)y, 0);
  int x1 = MIN((int)(x + w), backbuffer->width);
  int y1 = MIN((int)(y + h), backbuffer->height);
  if (x0 >= x1 || y0 >= y1)
    return; /* entirely off-screen */

  /* ── Convert float colors [0.0, 1.0] to 8-bit [0, 255] ── */
  float clamped_r = CLAMP(r, 0.0f, 1.0f);
  float clamped_g = CLAMP(g, 0.0f, 1.0f);
  float clamped_b = CLAMP(b, 0.0f, 1.0f);
  float clamped_a = CLAMP(a, 0.0f, 1.0f);

  uint32_t r8 = (uint32_t)(clamped_r * 255.0f);
  uint32_t g8 = (uint32_t)(clamped_g * 255.0f);
  uint32_t b8 = (uint32_t)(clamped_b * 255.0f);
  uint32_t a8 = (uint32_t)(clamped_a * 255.0f);

  /* ── Tier 2: fully opaque → direct write (fast path) ───── */
  if (a >= 1.0f) {
    uint32_t color = GAME_RGBA(r8, g8, b8, 255);
    for (int row = y0; row < y1; row++) {
      uint32_t *dst = backbuffer->pixels + row * backbuffer->width + x0;
      for (int col = x0; col < x1; col++) {
        *dst++ = color;
      }
    }
    return;
  }

  /* ── Tier 3: semi-transparent → per-pixel alpha blend ──── */
  uint32_t src_a = a8;
  uint32_t inv_a = 255 - src_a;

  for (int row = y0; row < y1; row++) {
    uint32_t *dst = backbuffer->pixels + row * backbuffer->width + x0;
    for (int col = x0; col < x1; col++) {
      uint32_t dest = *dst;

      /* Extract destination channels from existing pixel */
      uint32_t dest_r = (dest) & 0xFF;
      uint32_t dest_g = (dest >> 8) & 0xFF;
      uint32_t dest_b = (dest >> 16) & 0xFF;

      /* Blend: out = src * alpha + dst * (1 - alpha), >>8 approx */
      uint32_t out_r = (r8 * src_a + dest_r * inv_a) >> 8;
      uint32_t out_g = (g8 * src_a + dest_g * inv_a) >> 8;
      uint32_t out_b = (b8 * src_a + dest_b * inv_a) >> 8;

      /* Write blended pixel with full opacity */
      *dst++ = (out_r) | (out_g << 8) | (out_b << 16) | 0xFF000000u;
    }
  }
}