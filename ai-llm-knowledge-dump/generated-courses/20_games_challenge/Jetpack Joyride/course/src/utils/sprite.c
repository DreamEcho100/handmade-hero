/* ═══════════════════════════════════════════════════════════════════════════
 * utils/sprite.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Sprite loading, blitting, atlas frames, animation, rotation.
 *
 * stb_image integration: STB_IMAGE_IMPLEMENTATION defined here — this file
 * is the single compilation unit that contains the stb_image decoder.
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Make all stb_image functions static to avoid symbol conflicts with Raylib,
 * which also includes stb_image internally.                                */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../../vendor/stb_image.h"
#pragma GCC diagnostic pop

#include "./sprite.h"
#include "./backbuffer.h"

#include <math.h>   /* sinf, cosf, tanf, floorf */
#include <stdio.h>  /* fprintf */
#include <string.h> /* memset */

/* ═══════════════════════════════════════════════════════════════════════════
 * sprite_load — decode PNG/BMP/TGA via stb_image
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * stbi_load returns pixels in [R,G,B,A] byte order when requesting 4 channels.
 * Our backbuffer uses GAME_RGBA(r,g,b,a) = (a<<24)|(r<<16)|(g<<8)|b which on
 * little-endian produces memory bytes [B,G,R,A] — BGRA order.
 *
 * So we must swizzle each pixel from stb's RGBA to our ARGB-packed format.  */
int sprite_load(const char *path, Sprite *out) {
  int w, h, channels;
  unsigned char *data = stbi_load(path, &w, &h, &channels, 4); /* force RGBA */
  if (!data) {
    fprintf(stderr, "sprite_load: failed to load '%s': %s\n",
            path, stbi_failure_reason());
    return -1;
  }

  /* Swizzle RGBA bytes → packed GAME_RGBA format.
   * stb gives us bytes: [R][G][B][A] per pixel.
   * We need uint32_t: (A<<24)|(R<<16)|(G<<8)|B.                          */
  int pixel_count = w * h;
  uint32_t *pixels = (uint32_t *)data; /* reuse same buffer in-place */
  for (int i = 0; i < pixel_count; i++) {
    unsigned char *p = (unsigned char *)&pixels[i];
    unsigned char r = p[0];
    unsigned char g = p[1];
    unsigned char b = p[2];
    unsigned char a = p[3];
    pixels[i] = GAME_RGBA(r, g, b, a);
  }

  out->pixels = pixels;
  out->width = w;
  out->height = h;
  return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * sprite_free
 * ═══════════════════════════════════════════════════════════════════════════
 */
void sprite_free(Sprite *s) {
  if (s->pixels) {
    stbi_image_free(s->pixels);
    s->pixels = NULL;
  }
  s->width = 0;
  s->height = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * sprite_blit — fast blit with alpha test (binary: draw or skip)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * For each source pixel: if alpha >= 128, write it; else skip.
 * This is fast (no per-pixel multiply) and sufficient for pixel art with
 * hard transparency edges.                                                */
void sprite_blit(Backbuffer *bb, const Sprite *sprite, SpriteRect src,
                 int dst_x, int dst_y) {
  /* Clip source rect to sprite bounds */
  int sx0 = src.x < 0 ? 0 : src.x;
  int sy0 = src.y < 0 ? 0 : src.y;
  int sx1 = src.x + src.w;
  int sy1 = src.y + src.h;
  if (sx1 > sprite->width) sx1 = sprite->width;
  if (sy1 > sprite->height) sy1 = sprite->height;

  int src_w = sx1 - sx0;
  int src_h = sy1 - sy0;
  if (src_w <= 0 || src_h <= 0) return;

  /* Clip destination to backbuffer bounds */
  int dx0 = dst_x + (sx0 - src.x);
  int dy0 = dst_y + (sy0 - src.y);
  int clip_left = 0, clip_top = 0;

  if (dx0 < 0) { clip_left = -dx0; dx0 = 0; }
  if (dy0 < 0) { clip_top = -dy0; dy0 = 0; }

  int draw_w = src_w - clip_left;
  int draw_h = src_h - clip_top;
  if (dx0 + draw_w > bb->width) draw_w = bb->width - dx0;
  if (dy0 + draw_h > bb->height) draw_h = bb->height - dy0;
  if (draw_w <= 0 || draw_h <= 0) return;

  int bb_pitch = bb->pitch / 4;
  int sp_pitch = sprite->width;

  for (int row = 0; row < draw_h; row++) {
    uint32_t *dst_row = bb->pixels + (dy0 + row) * bb_pitch + dx0;
    const uint32_t *src_row =
        sprite->pixels + (sy0 + clip_top + row) * sp_pitch + sx0 + clip_left;

    for (int col = 0; col < draw_w; col++) {
      uint32_t pixel = src_row[col];
      unsigned int a = (pixel >> 24) & 0xFF;
      if (a >= 128) {
        dst_row[col] = pixel;
      }
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * sprite_blit_alpha — full per-pixel alpha blending
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * out = src * a + dst * (1 - a), per channel.
 * Slower than sprite_blit but produces smooth semi-transparent edges.     */
void sprite_blit_alpha(Backbuffer *bb, const Sprite *sprite, SpriteRect src,
                       int dst_x, int dst_y) {
  int sx0 = src.x < 0 ? 0 : src.x;
  int sy0 = src.y < 0 ? 0 : src.y;
  int sx1 = src.x + src.w;
  int sy1 = src.y + src.h;
  if (sx1 > sprite->width) sx1 = sprite->width;
  if (sy1 > sprite->height) sy1 = sprite->height;

  int src_w = sx1 - sx0;
  int src_h = sy1 - sy0;
  if (src_w <= 0 || src_h <= 0) return;

  int dx0 = dst_x + (sx0 - src.x);
  int dy0 = dst_y + (sy0 - src.y);
  int clip_left = 0, clip_top = 0;

  if (dx0 < 0) { clip_left = -dx0; dx0 = 0; }
  if (dy0 < 0) { clip_top = -dy0; dy0 = 0; }

  int draw_w = src_w - clip_left;
  int draw_h = src_h - clip_top;
  if (dx0 + draw_w > bb->width) draw_w = bb->width - dx0;
  if (dy0 + draw_h > bb->height) draw_h = bb->height - dy0;
  if (draw_w <= 0 || draw_h <= 0) return;

  int bb_pitch = bb->pitch / 4;
  int sp_pitch = sprite->width;

  for (int row = 0; row < draw_h; row++) {
    uint32_t *dst_row = bb->pixels + (dy0 + row) * bb_pitch + dx0;
    const uint32_t *src_row =
        sprite->pixels + (sy0 + clip_top + row) * sp_pitch + sx0 + clip_left;

    for (int col = 0; col < draw_w; col++) {
      uint32_t sp = src_row[col];
      unsigned int sa = (sp >> 24) & 0xFF;

      if (sa == 0) continue;       /* fully transparent */
      if (sa == 255) {              /* fully opaque — fast path */
        dst_row[col] = sp;
        continue;
      }

      /* Per-channel blend: out = src*a/255 + dst*(255-a)/255 */
      uint32_t dp = dst_row[col];
      unsigned int sr = (sp >> 16) & 0xFF;
      unsigned int sg = (sp >>  8) & 0xFF;
      unsigned int sb =  sp        & 0xFF;
      unsigned int dr = (dp >> 16) & 0xFF;
      unsigned int dg = (dp >>  8) & 0xFF;
      unsigned int db =  dp        & 0xFF;
      unsigned int da = (dp >> 24) & 0xFF;

      unsigned int inv_a = 255 - sa;
      unsigned int or = (sr * sa + dr * inv_a) / 255;
      unsigned int og = (sg * sa + dg * inv_a) / 255;
      unsigned int ob = (sb * sa + db * inv_a) / 255;
      unsigned int oa = sa + (da * inv_a) / 255;
      if (oa > 255) oa = 255;

      dst_row[col] = (oa << 24) | (or << 16) | (og << 8) | ob;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * sprite_blit_rotated — 3-shear Paeth rotation
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Rotates a sprite sub-rectangle around its center and blits to backbuffer.
 * Uses the naive pixel-lookup approach: for each output pixel, find the
 * source pixel via inverse rotation. Simple and correct for small sprites. */
void sprite_blit_rotated(Backbuffer *bb, const Sprite *sprite, SpriteRect src,
                         int dst_cx, int dst_cy, float angle) {
  float cos_a = cosf(angle);
  float sin_a = sinf(angle);

  /* Source center */
  float scx = (float)src.w * 0.5f;
  float scy = (float)src.h * 0.5f;

  /* Output bounding box: rotated corners determine the size */
  float half_w = (float)src.w * 0.5f;
  float half_h = (float)src.h * 0.5f;
  float abs_cos = cos_a < 0 ? -cos_a : cos_a;
  float abs_sin = sin_a < 0 ? -sin_a : sin_a;
  int out_w = (int)(half_w * abs_cos + half_h * abs_sin + 1.0f) * 2;
  int out_h = (int)(half_w * abs_sin + half_h * abs_cos + 1.0f) * 2;

  int out_x0 = dst_cx - out_w / 2;
  int out_y0 = dst_cy - out_h / 2;

  int bb_pitch = bb->pitch / 4;
  int sp_pitch = sprite->width;

  for (int oy = 0; oy < out_h; oy++) {
    int py = out_y0 + oy;
    if (py < 0 || py >= bb->height) continue;

    for (int ox = 0; ox < out_w; ox++) {
      int px = out_x0 + ox;
      if (px < 0 || px >= bb->width) continue;

      /* Inverse rotation: find source pixel */
      float rx = (float)(ox - out_w / 2);
      float ry = (float)(oy - out_h / 2);
      float sx_f = cos_a * rx + sin_a * ry + scx;
      float sy_f = -sin_a * rx + cos_a * ry + scy;

      int sx = (int)sx_f;
      int sy = (int)sy_f;
      if (sx < 0 || sx >= src.w || sy < 0 || sy >= src.h) continue;

      uint32_t pixel = sprite->pixels[(src.y + sy) * sp_pitch + src.x + sx];
      unsigned int a = (pixel >> 24) & 0xFF;
      if (a >= 128) {
        bb->pixels[py * bb_pitch + px] = pixel;
      }
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Spritesheet helpers
 * ═══════════════════════════════════════════════════════════════════════════
 */
SpriteRect spritesheet_frame_rect(const SpriteSheet *ss, int frame_idx) {
  int col = frame_idx % ss->cols;
  int row = frame_idx / ss->cols;
  SpriteRect r = {
    .x = col * ss->frame_w,
    .y = row * ss->frame_h,
    .w = ss->frame_w,
    .h = ss->frame_h,
  };
  return r;
}

void spritesheet_init(SpriteSheet *ss, Sprite sprite, int frame_w, int frame_h) {
  ss->sprite = sprite;
  ss->frame_w = frame_w;
  ss->frame_h = frame_h;
  ss->cols = sprite.width / frame_w;
  ss->rows = sprite.height / frame_h;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Animation
 * ═══════════════════════════════════════════════════════════════════════════
 */
void anim_init(SpriteAnimation *anim, SpriteSheet *sheet,
               int start_frame, int frame_count, float fps, int loop) {
  anim->sheet = sheet;
  anim->start_frame = start_frame;
  anim->frame_count = frame_count;
  anim->fps = fps;
  anim->elapsed = 0.0f;
  anim->current_frame = 0;
  anim->loop = loop;
  anim->finished = 0;
}

void anim_update(SpriteAnimation *anim, float dt) {
  if (anim->finished || anim->frame_count <= 1 || anim->fps <= 0.0f)
    return;

  anim->elapsed += dt;
  float frame_duration = 1.0f / anim->fps;

  while (anim->elapsed >= frame_duration) {
    anim->elapsed -= frame_duration;
    anim->current_frame++;

    if (anim->current_frame >= anim->frame_count) {
      if (anim->loop) {
        anim->current_frame = 0;
      } else {
        anim->current_frame = anim->frame_count - 1;
        anim->finished = 1;
        anim->elapsed = 0.0f;
        return;
      }
    }
  }
}

void anim_reset(SpriteAnimation *anim) {
  anim->current_frame = 0;
  anim->elapsed = 0.0f;
  anim->finished = 0;
}

void anim_draw(const SpriteAnimation *anim, Backbuffer *bb,
               int dst_x, int dst_y) {
  if (!anim->sheet || !anim->sheet->sprite.pixels)
    return;

  int frame = anim->start_frame + anim->current_frame;
  SpriteRect rect = spritesheet_frame_rect(anim->sheet, frame);
  sprite_blit(bb, &anim->sheet->sprite, rect, dst_x, dst_y);
}
