/* ══════════════════════════════════════════════════════════════════════ */
/*               game.c — LOATs Game Demo Implementation                 */
/*                                                                       */
/*  9-scene selector demonstrating variant implementations.              */
/*  Press 1-9 to switch scenes. Press R to reset current scene.          */
/* ══════════════════════════════════════════════════════════════════════ */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
#include "game.h"
#include <string.h> /* memset */
#include <stdio.h>  /* snprintf */
#include <math.h>   /* sinf, cosf, sqrtf */
#include <time.h>   /* clock_gettime (Scene 11 timing) */
#include <stdlib.h> /* malloc, free (Scene 13 Memory Arena Lab) */
#include <pthread.h> /* Scene 14: threaded mode */

/* ══════════════════════════════════════════════════════ */
/*              Simple RNG (xorshift32)                     */
/* ══════════════════════════════════════════════════════ */

static unsigned int rng_next(unsigned int *state)
{
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float rng_float(unsigned int *state)
{
    return (float)(rng_next(state) & 0xFFFF) / 65536.0f;
}

/* ══════════════════════════════════════════════════════ */
/*              Simple Pixel Drawing                       */
/* ══════════════════════════════════════════════════════ */

static void draw_rect(uint32_t *pixels, int buf_w, int buf_h,
                       int rx, int ry, int rw, int rh, uint32_t color)
{
    if (!pixels) return;
    int x0 = rx < 0 ? 0 : rx;
    int y0 = ry < 0 ? 0 : ry;
    int x1 = (rx + rw) > buf_w ? buf_w : (rx + rw);
    int y1 = (ry + rh) > buf_h ? buf_h : (ry + rh);

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            pixels[y * buf_w + x] = color;
        }
    }
}

/* ══════════════════════════════════════════════════════ */
/*              8x8 Bitmap Font (CP437-style)              */
/* ══════════════════════════════════════════════════════ */

static const uint8_t FONT_8X8[128][8] = {
  /* 0x00-0x1F : control characters (blank) */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x00 NUL */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x01 SOH */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x02 STX */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x03 ETX */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x04 EOT */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x05 ENQ */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x06 ACK */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x07 BEL */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x08 BS  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x09 HT  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x0A LF  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x0B VT  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x0C FF  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x0D CR  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x0E SO  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x0F SI  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x10 DLE */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x11 DC1 */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x12 DC2 */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x13 DC3 */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x14 DC4 */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x15 NAK */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x16 SYN */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x17 ETB */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x18 CAN */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x19 EM  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x1A SUB */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x1B ESC */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x1C FS  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x1D GS  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x1E RS  */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x1F US  */

  /* 0x20-0x2F : space, !"#$%&'()*+,-./ */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x20 ' ' */
  {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* 0x21 '!' */
  {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x22 '"' */
  {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, /* 0x23 '#' */
  {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, /* 0x24 '$' */
  {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, /* 0x25 '%' */
  {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, /* 0x26 '&' */
  {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, /* 0x27 ''' */
  {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, /* 0x28 '(' */
  {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, /* 0x29 ')' */
  {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* 0x2A '*' */
  {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, /* 0x2B '+' */
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, /* 0x2C ',' */
  {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, /* 0x2D '-' */
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, /* 0x2E '.' */
  {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, /* 0x2F '/' */

  /* 0x30-0x39 : digits 0-9 */
  {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, /* 0x30 '0' */
  {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, /* 0x31 '1' */
  {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, /* 0x32 '2' */
  {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, /* 0x33 '3' */
  {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, /* 0x34 '4' */
  {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, /* 0x35 '5' */
  {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, /* 0x36 '6' */
  {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, /* 0x37 '7' */
  {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, /* 0x38 '8' */
  {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, /* 0x39 '9' */

  /* 0x3A-0x40 : :;<=>?@ */
  {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, /* 0x3A ':' */
  {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, /* 0x3B ';' */
  {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, /* 0x3C '<' */
  {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, /* 0x3D '=' */
  {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* 0x3E '>' */
  {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, /* 0x3F '?' */
  {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, /* 0x40 '@' */

  /* 0x41-0x5A : uppercase A-Z */
  {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, /* 0x41 'A' */
  {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, /* 0x42 'B' */
  {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, /* 0x43 'C' */
  {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, /* 0x44 'D' */
  {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, /* 0x45 'E' */
  {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, /* 0x46 'F' */
  {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, /* 0x47 'G' */
  {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, /* 0x48 'H' */
  {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 0x49 'I' */
  {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, /* 0x4A 'J' */
  {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, /* 0x4B 'K' */
  {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, /* 0x4C 'L' */
  {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, /* 0x4D 'M' */
  {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, /* 0x4E 'N' */
  {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, /* 0x4F 'O' */
  {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, /* 0x50 'P' */
  {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, /* 0x51 'Q' */
  {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, /* 0x52 'R' */
  {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, /* 0x53 'S' */
  {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 0x54 'T' */
  {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, /* 0x55 'U' */
  {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* 0x56 'V' */
  {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* 0x57 'W' */
  {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, /* 0x58 'X' */
  {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, /* 0x59 'Y' */
  {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, /* 0x5A 'Z' */

  /* 0x5B-0x60 : [\]^_` */
  {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, /* 0x5B '[' */
  {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* 0x5C '\' */
  {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, /* 0x5D ']' */
  {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, /* 0x5E '^' */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* 0x5F '_' */
  {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, /* 0x60 '`' */

  /* 0x61-0x7A : lowercase a-z */
  {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, /* 0x61 'a' */
  {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, /* 0x62 'b' */
  {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, /* 0x63 'c' */
  {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, /* 0x64 'd' */
  {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, /* 0x65 'e' */
  {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, /* 0x66 'f' */
  {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, /* 0x67 'g' */
  {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, /* 0x68 'h' */
  {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, /* 0x69 'i' */
  {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, /* 0x6A 'j' */
  {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, /* 0x6B 'k' */
  {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 0x6C 'l' */
  {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, /* 0x6D 'm' */
  {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, /* 0x6E 'n' */
  {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, /* 0x6F 'o' */
  {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, /* 0x70 'p' */
  {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, /* 0x71 'q' */
  {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, /* 0x72 'r' */
  {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, /* 0x73 's' */
  {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, /* 0x74 't' */
  {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, /* 0x75 'u' */
  {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* 0x76 'v' */
  {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* 0x77 'w' */
  {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, /* 0x78 'x' */
  {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, /* 0x79 'y' */
  {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, /* 0x7A 'z' */

  /* 0x7B-0x7E : {|}~ */
  {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, /* 0x7B '{' */
  {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* 0x7C '|' */
  {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, /* 0x7D '}' */
  {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x7E '~' */

  /* 0x7F : DEL (blank) */
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x7F DEL */
};

/* Draw text onto pixel buffer using 8x8 bitmap font.
 * Each character is 8x8 pixels. No scaling. */
static void draw_text_buf(uint32_t *pixels, int buf_w, int buf_h,
                          int x, int y, const char *text, uint32_t color)
{
    if (!pixels) return;
    for (int i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i] & 0x7F;
        if (ch == '\n') break; /* stop at newline -- caller handles multi-line */
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (FONT_8X8[ch][row] & (1 << col)) {
                    int px = x + i * 8 + col;
                    int py = y + row;
                    if (px >= 0 && px < buf_w && py >= 0 && py < buf_h)
                        pixels[py * buf_w + px] = color;
                }
            }
        }
    }
}

/* ══════════════════════════════════════════════════════ */
/*              Scene Names                                */
/* ══════════════════════════════════════════════════════ */

const char *scene_name(int scene)
{
    switch (scene) {
        case 1: return "Singly-Linked Inventory";
        case 2: return "Circular Doubly-Linked Inventory";
        case 3: return "Kind-Check vs used[] Liveness";
        case 4: return "Fat Struct Info";
        case 5: return "Free List ON vs OFF";
        case 6: return "Generational IDs Demo";
        case 7: return "Prepend vs Append Order";
        case 8: return "Default Movement Demo";
        case 9: return "Separate Pools";
        case 10: return "Particle Laboratory";
        case 11: return "Data Layout Toggle Lab";
        case 12: return "Spatial Partition Lab";
        case 13: return "Memory Arena Lab";
        case 14: return "Infinite Growth Lab";
        default: return "Unknown";
    }
}

/* ══════════════════════════════════════════════════════ */
/*         Helper: spawn player at center                  */
/* ══════════════════════════════════════════════════════ */

static thing_ref spawn_player(thing_pool *pool)
{
    thing_ref ref = thing_pool_add(pool, THING_KIND_PLAYER);
    thing *p = thing_pool_get_ref(pool, ref);
    if (thing_is_valid(p)) {
        p->x     = (float)SCREEN_W / 2.0f;
        p->y     = (float)SCREEN_H / 2.0f;
        p->size  = PLAYER_SIZE;
        p->color = 0xFFFF8800; /* orange */
    }
    return ref;
}

/* Helper: move player with input and clamp to screen */
static void move_player(thing *player, const game_input *input, float dt)
{
    if (!thing_is_valid(player)) return;
    float dx = 0.0f, dy = 0.0f;
    if (input->left)  dx -= 1.0f;
    if (input->right) dx += 1.0f;
    if (input->up)    dy -= 1.0f;
    if (input->down)  dy += 1.0f;
    player->x += dx * PLAYER_SPEED * dt;
    player->y += dy * PLAYER_SPEED * dt;
    if (player->x < 0) player->x = 0;
    if (player->y < 0) player->y = 0;
    if (player->x > SCREEN_W - player->size) player->x = SCREEN_W - player->size;
    if (player->y > SCREEN_H - player->size) player->y = SCREEN_H - player->size;
}

/* Helper: render all living things in a pool as colored rects */
static void render_pool(const thing_pool *pool, uint32_t *pixels, int w, int h)
{
    if (!pixels) return;
    thing_pool *p = (thing_pool *)pool;
    for (thing_iter it = thing_iter_begin(p);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        thing *t = thing_iter_get(&it);
        draw_rect(pixels, w, h, (int)t->x, (int)t->y,
                  (int)t->size, (int)t->size, t->color);
    }
}

/* Helper: count living things of a given kind in a pool */
static int count_kind(const thing_pool *pool, thing_kind kind)
{
    int n = 0;
    thing_pool *p = (thing_pool *)pool;
    for (thing_iter it = thing_iter_begin(p);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        thing *t = thing_iter_get(&it);
        if (t->kind == kind) n++;
    }
    return n;
}

/* Helper: count active (used) entities in a pool (skipping nil sentinel at 0) */
static int count_pool_active(const thing_pool *pool) {
    int count = 0;
    for (int i = 1; i < pool->next_empty && i < MAX_THINGS; i++) {
        if (pool->used[i]) count++;
    }
    return count;
}

/* Helper: spawn an item at random position */
static thing_ref spawn_item(thing_pool *pool, unsigned int *rng, uint32_t color)
{
    thing_ref ref = thing_pool_add(pool, THING_KIND_ITEM);
    thing *t = thing_pool_get_ref(pool, ref);
    if (thing_is_valid(t)) {
        t->x     = rng_float(rng) * (SCREEN_W - 30) + 15;
        t->y     = rng_float(rng) * (SCREEN_H - 80) + 60;
        t->size  = 10.0f;
        t->color = color;
    }
    return ref;
}

/* ══════════════════════════════════════════════════════ */
/*         Scene 1: Singly-linked inventory                */
/* ══════════════════════════════════════════════════════ */

static void scene1_init(game_state *state)
{
    thing_pool_init(&state->pool);
    state->rng_state = 12345;
    state->player_ref = spawn_player(&state->pool);
    state->spawn_timer = 0;
    state->trolls_spawned = 0; /* reuse as item count on ground */
    /* Spawn some items on the ground */
    uint32_t item_colors[] = {0xFF44FF44, 0xFF4444FF, 0xFFFF44FF, 0xFFFFFF44, 0xFF44FFFF};
    for (int i = 0; i < 5; i++) {
        spawn_item(&state->pool, &state->rng_state, item_colors[i % 5]);
    }
}

static void scene1_update(game_state *state, const game_input *input, float dt)
{
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    move_player(player, input, dt);

    /* Respawn items periodically */
    state->spawn_timer += dt;
    if (state->spawn_timer >= 2.0f) {
        state->spawn_timer = 0;
        uint32_t colors[] = {0xFF44FF44, 0xFF4444FF, 0xFFFF44FF};
        spawn_item(&state->pool, &state->rng_state,
                   colors[rng_next(&state->rng_state) % 3]);
    }

    /* Collision: pick up items (add_child_singly) */
    if (thing_is_valid(player)) {
        for (thing_iter it = thing_iter_begin(&state->pool);
             thing_iter_valid(&it);
             thing_iter_next(&it))
        {
            thing *t = thing_iter_get(&it);
            if (t->kind != THING_KIND_ITEM || t->parent != 0) continue;

            float dx = (player->x + player->size/2) - (t->x + t->size/2);
            float dy = (player->y + player->size/2) - (t->y + t->size/2);
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx < (player->size/2 + t->size/2) && dy < (player->size/2 + t->size/2)) {
                thing_add_child_singly(&state->pool, state->player_ref.index, it.current);
                /* Move item off-screen visually (carried by player) */
                t->x = -100; t->y = -100;
                break;
            }
        }
    }

    /* Space: drop last item (O(n) singly-linked removal) */
    if (input->space && thing_is_valid(player)) {
        thing_idx child = player->first_child;
        if (child != 0) {
            /* Find the LAST child in the singly-linked list (no prev_sibling) */
            thing_idx last = child;
            thing *c = thing_pool_get(&state->pool, child);
            while (c->next_sibling != 0) {
                last = c->next_sibling;
                c = thing_pool_get(&state->pool, c->next_sibling);
            }
            thing_remove_child_singly(&state->pool, last);
            /* Place dropped item near player */
            thing *dropped = thing_pool_get(&state->pool, last);
            dropped->x = player->x + 30;
            dropped->y = player->y;
        }
    }
}

static void scene1_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    render_pool(&state->pool, pixels, w, h);

    /* Build inventory string */
    char inv[128] = "";
    int inv_len = 0;
    thing *player = thing_pool_get_ref((thing_pool *)&state->pool, state->player_ref);
    if (thing_is_valid(player)) {
        thing_idx ci = player->first_child;
        int count = 0;
        while (ci != 0 && count < 8) {
            if (inv_len > 0) {
                inv_len += snprintf(inv + inv_len, sizeof(inv) - (size_t)inv_len, " -> ");
            }
            inv_len += snprintf(inv + inv_len, sizeof(inv) - (size_t)inv_len, "Item");
            thing *c = &((thing_pool *)&state->pool)->things[ci];
            ci = c->next_sibling;
            count++;
        }
    }
    if (inv_len == 0) snprintf(inv, sizeof(inv), "(empty)");

    snprintf(hud, hud_size,
             "Singly-linked  |  Items: %s  |  Remove: O(n) walk  |  Pool: %d/%d  |  [Space] Drop",
             inv, count_pool_active(&state->pool), MAX_THINGS - 1);
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 2: Circular doubly-linked inventory             */
/* ══════════════════════════════════════════════════════ */

static void scene2_init(game_state *state)
{
    thing_pool_init(&state->pool);
    state->rng_state = 12345;
    state->player_ref = spawn_player(&state->pool);
    state->spawn_timer = 0;
    uint32_t item_colors[] = {0xFF44FF44, 0xFF4444FF, 0xFFFF44FF, 0xFFFFFF44, 0xFF44FFFF};
    for (int i = 0; i < 5; i++) {
        spawn_item(&state->pool, &state->rng_state, item_colors[i % 5]);
    }
}

static void scene2_update(game_state *state, const game_input *input, float dt)
{
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    move_player(player, input, dt);

    state->spawn_timer += dt;
    if (state->spawn_timer >= 2.0f) {
        state->spawn_timer = 0;
        uint32_t colors[] = {0xFF44FF44, 0xFF4444FF, 0xFFFF44FF};
        spawn_item(&state->pool, &state->rng_state,
                   colors[rng_next(&state->rng_state) % 3]);
    }

    /* Pick up items using circular doubly-linked add_child */
    if (thing_is_valid(player)) {
        for (thing_iter it = thing_iter_begin(&state->pool);
             thing_iter_valid(&it);
             thing_iter_next(&it))
        {
            thing *t = thing_iter_get(&it);
            if (t->kind != THING_KIND_ITEM || t->parent != 0) continue;

            float dx = (player->x + player->size/2) - (t->x + t->size/2);
            float dy = (player->y + player->size/2) - (t->y + t->size/2);
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx < (player->size/2 + t->size/2) && dy < (player->size/2 + t->size/2)) {
                thing_add_child(&state->pool, state->player_ref.index, it.current);
                t->x = -100; t->y = -100;
                break;
            }
        }
    }

    /* Space: drop first child (O(1) with circular doubly-linked) */
    if (input->space && thing_is_valid(player) && player->first_child != 0) {
        thing_idx to_drop = player->first_child;
        thing_unlink_child(&state->pool, to_drop);
        thing *dropped = thing_pool_get(&state->pool, to_drop);
        dropped->x = player->x + 30;
        dropped->y = player->y;
    }
}

static void scene2_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    render_pool(&state->pool, pixels, w, h);

    char inv[128] = "";
    int inv_len = 0;
    thing *player = thing_pool_get_ref((thing_pool *)&state->pool, state->player_ref);
    if (thing_is_valid(player) && player->first_child != 0) {
        thing_idx first = player->first_child;
        thing_idx ci = first;
        int count = 0;
        do {
            if (inv_len > 0)
                inv_len += snprintf(inv + inv_len, sizeof(inv) - (size_t)inv_len, " -> ");
            inv_len += snprintf(inv + inv_len, sizeof(inv) - (size_t)inv_len, "Item");
            thing *c = &((thing_pool *)&state->pool)->things[ci];
            ci = c->next_sibling;
            count++;
        } while (ci != first && count < 8);
    }
    if (inv_len == 0) snprintf(inv, sizeof(inv), "(empty)");

    snprintf(hud, hud_size,
             "Circular  |  Items: %s  |  Remove: O(1) direct  |  Pool: %d/%d  |  [Space] Drop",
             inv, count_pool_active(&state->pool), MAX_THINGS - 1);
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 3: Kind-check vs used[] liveness                */
/* ══════════════════════════════════════════════════════ */

static void scene3_init(game_state *state)
{
    thing_pool_init(&state->pool);
    state->rng_state = 42;
    state->player_ref = spawn_player(&state->pool);
    state->spawn_timer = 0;
    state->trolls_spawned = 0;
    state->trolls_killed = 0;
}

static void scene3_update(game_state *state, const game_input *input, float dt)
{
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    move_player(player, input, dt);

    /* Rapid spawn/remove cycle */
    state->spawn_timer += dt;
    if (state->spawn_timer >= 0.3f) {
        state->spawn_timer = 0;
        thing_ref ref = thing_pool_add(&state->pool, THING_KIND_TROLL);
        thing *t = thing_pool_get_ref(&state->pool, ref);
        if (thing_is_valid(t)) {
            t->x     = rng_float(&state->rng_state) * (SCREEN_W - 30) + 15;
            t->y     = rng_float(&state->rng_state) * (SCREEN_H - 80) + 60;
            t->size  = TROLL_SIZE;
            t->color = 0xFF4488CC;
            state->trolls_spawned++;
        }
    }

    /* Remove random trolls occasionally */
    if (state->trolls_spawned > 3 && (rng_next(&state->rng_state) % 5) == 0) {
        for (thing_iter it = thing_iter_begin(&state->pool);
             thing_iter_valid(&it);
             thing_iter_next(&it))
        {
            thing *t = thing_iter_get(&it);
            if (t->kind == THING_KIND_TROLL) {
                thing_pool_remove(&state->pool, it.current);
                state->trolls_killed++;
                break;
            }
        }
    }
}

static void scene3_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    render_pool(&state->pool, pixels, w, h);

    /* Count alive by both methods */
    int kind_count = 0, used_count = 0;
    thing_pool *pool = (thing_pool *)&state->pool;
    for (int i = 1; i < pool->next_empty; i++) {
        if (pool->things[i].kind != THING_KIND_NIL) kind_count++;
        if (pool->used[i]) used_count++;
    }

    int dead = (pool->next_empty - 1) - used_count;
    snprintf(hud, hud_size,
             "kind!=NIL: %d  |  used[]: %d  |  Agree: %s  |  Dead: %d  |  next_empty: %d",
             kind_count, used_count,
             kind_count == used_count ? "YES" : "NO",
             dead, pool->next_empty);
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 4: Fat struct info                              */
/* ══════════════════════════════════════════════════════ */

static void scene4_init(game_state *state)
{
    thing_pool_init(&state->pool);
    state->rng_state = 12345;
    state->player_ref = spawn_player(&state->pool);
    /* Spawn a few entities for visual interest */
    for (int i = 0; i < 8; i++) {
        thing_ref ref = thing_pool_add(&state->pool, THING_KIND_TROLL);
        thing *t = thing_pool_get_ref(&state->pool, ref);
        if (thing_is_valid(t)) {
            t->x     = rng_float(&state->rng_state) * (SCREEN_W - 30) + 15;
            t->y     = rng_float(&state->rng_state) * (SCREEN_H - 80) + 60;
            t->size  = TROLL_SIZE;
            t->color = 0xFF4488CC;
        }
    }
}

static void scene4_update(game_state *state, const game_input *input, float dt)
{
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    move_player(player, input, dt);
}

static void scene4_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    render_pool(&state->pool, pixels, w, h);

    int active = count_pool_active(&state->pool);
    snprintf(hud, hud_size,
             "sizeof(thing)=%d B  |  Pool: %d active / %d max = %d KB  |  Union: ~36B (-%d%%)",
             (int)sizeof(thing), active, MAX_THINGS,
             (int)(MAX_THINGS * sizeof(thing) / 1024),
             (int)(100 - 36 * 100 / (int)sizeof(thing)));
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 5: Free list ON vs OFF                          */
/* ══════════════════════════════════════════════════════ */

static void scene5_init(game_state *state)
{
    thing_pool_init(&state->pool);
    thing_pool_init(&state->pool_no_freelist);
    state->rng_state = 12345;
    state->s5_with_next = 1;
    state->s5_without_next = 1;
    state->trolls_spawned = 0;
    state->trolls_killed = 0;
}

static void scene5_update(game_state *state, const game_input *input, float dt)
{
    (void)input;

    state->spawn_timer += dt;
    if (state->spawn_timer >= 0.15f) {
        state->spawn_timer = 0;

        /* Phase: alternate between adding and removing */
        if (state->trolls_spawned < 30 || (state->trolls_spawned % 2 == 0)) {
            /* Add to both pools */
            thing_ref r1 = thing_pool_add(&state->pool, THING_KIND_TROLL);
            thing *t1 = thing_pool_get_ref(&state->pool, r1);
            if (thing_is_valid(t1)) {
                t1->x = rng_float(&state->rng_state) * (SCREEN_W/2 - 30) + 15;
                t1->y = rng_float(&state->rng_state) * (SCREEN_H - 80) + 60;
                t1->size = TROLL_SIZE;
                t1->color = 0xFF44CC44;
            }

            thing_ref r2 = thing_pool_add_no_freelist(&state->pool_no_freelist, THING_KIND_TROLL);
            thing *t2 = thing_pool_get_ref(&state->pool_no_freelist, r2);
            if (thing_is_valid(t2)) {
                t2->x = rng_float(&state->rng_state) * (SCREEN_W/2 - 30) + (float)SCREEN_W/2 + 15;
                t2->y = rng_float(&state->rng_state) * (SCREEN_H - 80) + 60;
                t2->size = TROLL_SIZE;
                t2->color = 0xFFCC4444;
            }
            state->trolls_spawned++;
        }

        /* Remove a random troll from both every few spawns */
        if (state->trolls_spawned > 5 && (state->trolls_spawned % 3 == 0)) {
            for (thing_iter it = thing_iter_begin(&state->pool);
                 thing_iter_valid(&it);
                 thing_iter_next(&it))
            {
                if (thing_iter_get(&it)->kind == THING_KIND_TROLL) {
                    thing_pool_remove(&state->pool, it.current);
                    break;
                }
            }
            for (thing_iter it = thing_iter_begin(&state->pool_no_freelist);
                 thing_iter_valid(&it);
                 thing_iter_next(&it))
            {
                if (thing_iter_get(&it)->kind == THING_KIND_TROLL) {
                    thing_pool_remove(&state->pool_no_freelist, it.current);
                    break;
                }
            }
            state->trolls_killed++;
        }

        state->s5_with_next = state->pool.next_empty;
        state->s5_without_next = state->pool_no_freelist.next_empty;
    }
}

static void scene5_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    /* Draw dividing line */
    draw_rect(pixels, w, h, w/2 - 1, 0, 2, h, 0xFF888888);

    /* Render both pools */
    render_pool(&state->pool, pixels, w, h);
    render_pool(&state->pool_no_freelist, pixels, w, h);

    int wasted = state->s5_without_next - state->s5_with_next;
    if (wasted < 0) wasted = 0;

    int active_with = count_pool_active(&state->pool);
    int active_without = count_pool_active(&state->pool_no_freelist);
    snprintf(hud, hud_size,
             "WITH freelist: next=%d, active=%d  |  WITHOUT: next=%d, active=%d  |  Wasted: %d slots",
             state->s5_with_next, active_with,
             state->s5_without_next, active_without, wasted);
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 6: Generational IDs demo                        */
/* ══════════════════════════════════════════════════════ */

static void scene6_init(game_state *state)
{
    thing_pool_init(&state->pool);
    state->rng_state = 12345;
    state->player_ref = spawn_player(&state->pool);
    state->s6_phase = 0;
    state->s6_timer = 0;
    state->s6_saved_ref = (thing_ref){0, 0};
}

static void scene6_update(game_state *state, const game_input *input, float dt)
{
    (void)input;

    state->s6_timer += dt;

    /* Auto-advance through phases */
    if (state->s6_timer >= 2.0f) {
        state->s6_timer = 0;

        switch (state->s6_phase) {
            case 0: {
                /* Spawn a troll and save its ref */
                thing_ref ref = thing_pool_add(&state->pool, THING_KIND_TROLL);
                thing *t = thing_pool_get_ref(&state->pool, ref);
                if (thing_is_valid(t)) {
                    t->x = SCREEN_W / 2.0f - 50;
                    t->y = SCREEN_H / 2.0f;
                    t->size = 25;
                    t->color = 0xFF4488CC;
                }
                state->s6_saved_ref = ref;
                state->s6_phase = 1;
                break;
            }
            case 1: {
                /* Remove the troll */
                thing_pool_remove(&state->pool, state->s6_saved_ref.index);
                state->s6_phase = 2;
                break;
            }
            case 2: {
                /* Spawn a NEW entity at (hopefully) the same slot */
                thing_ref new_ref = thing_pool_add(&state->pool, THING_KIND_GOBLIN);
                thing *t = thing_pool_get_ref(&state->pool, new_ref);
                if (thing_is_valid(t)) {
                    t->x = SCREEN_W / 2.0f + 50;
                    t->y = SCREEN_H / 2.0f;
                    t->size = 25;
                    t->color = 0xFFCC44CC;
                }
                state->s6_phase = 3;
                break;
            }
            case 3: {
                /* Reset cycle */
                state->s6_phase = 0;
                /* Clear pool for next demo cycle */
                thing_pool_init(&state->pool);
                state->player_ref = spawn_player(&state->pool);
                state->s6_saved_ref = (thing_ref){0, 0};
                break;
            }
        }
    }
}

static void scene6_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    render_pool(&state->pool, pixels, w, h);

    const char *with_gen = "---";
    const char *without_gen = "---";

    if (state->s6_phase >= 3) {
        /* Try to access via the old saved ref */
        thing *via_gen = thing_pool_get_ref((thing_pool *)&state->pool, state->s6_saved_ref);
        thing *via_no_gen = thing_pool_get_no_gen((thing_pool *)&state->pool, state->s6_saved_ref);

        with_gen = thing_is_valid(via_gen) ? "WRONG ENTITY (bug!)" : "NIL (safe)";
        without_gen = thing_is_valid(via_no_gen) ? "WRONG ENTITY (bug!)" : "NIL (safe)";
    }

    const char *phase_names[] = {"Spawning troll...", "Troll saved (ref stored)",
                                  "Troll removed", "New entity at same slot"};
    int phase = state->s6_phase < 4 ? state->s6_phase : 3;

    int active = count_pool_active(&state->pool);
    snprintf(hud, hud_size,
             "%s  |  With gen: %s  |  Without: %s  |  Slot: %d gen: %u  |  Pool: %d/%d",
             phase_names[phase], with_gen, without_gen,
             state->s6_saved_ref.index, state->s6_saved_ref.generation,
             active, MAX_THINGS - 1);
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 7: Prepend vs Append order                      */
/* ══════════════════════════════════════════════════════ */

static void scene7_init(game_state *state)
{
    thing_pool_init(&state->pool);
    state->rng_state = 12345;

    /* Create two "container" entities */
    thing_ref prepend_ref = thing_pool_add(&state->pool, THING_KIND_PLAYER);
    thing *pp = thing_pool_get_ref(&state->pool, prepend_ref);
    if (thing_is_valid(pp)) {
        pp->x = 100; pp->y = 150; pp->size = 30; pp->color = 0xFFFF8800;
    }
    state->s7_prepend_parent = prepend_ref.index;

    thing_ref append_ref = thing_pool_add(&state->pool, THING_KIND_PLAYER);
    thing *ap = thing_pool_get_ref(&state->pool, append_ref);
    if (thing_is_valid(ap)) {
        ap->x = 100; ap->y = 350; ap->size = 30; ap->color = 0xFFFF8800;
    }
    state->s7_append_parent = append_ref.index;

    state->s7_items_added = 0;
    state->spawn_timer = 0;
}

static void scene7_update(game_state *state, const game_input *input, float dt)
{
    (void)input;

    state->spawn_timer += dt;

    /* Add items A, B, C one at a time */
    if (state->spawn_timer >= 1.5f && state->s7_items_added < 5) {
        state->spawn_timer = 0;

        uint32_t colors[] = {0xFF44FF44, 0xFF4444FF, 0xFFFF44FF, 0xFFFFFF44, 0xFF44FFFF};

        /* Add to prepend parent (inserts at FRONT) */
        thing_ref p_ref = thing_pool_add(&state->pool, THING_KIND_ITEM);
        thing *pi = thing_pool_get_ref(&state->pool, p_ref);
        if (thing_is_valid(pi)) {
            pi->size = 15; pi->color = colors[state->s7_items_added];
            thing_add_child(&state->pool, state->s7_prepend_parent, p_ref.index);
        }

        /* Add to append parent (inserts at END) */
        thing_ref a_ref = thing_pool_add(&state->pool, THING_KIND_ITEM);
        thing *ai = thing_pool_get_ref(&state->pool, a_ref);
        if (thing_is_valid(ai)) {
            ai->size = 15; ai->color = colors[state->s7_items_added];
            thing_add_child_append(&state->pool, state->s7_append_parent, a_ref.index);
        }

        state->s7_items_added++;
    }
}

static void scene7_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    thing_pool *pool = (thing_pool *)&state->pool;

    /* Draw labels as colored blocks at left */
    draw_rect(pixels, w, h, 50, 140, 30, 30, 0xFFFF8800); /* prepend label */
    draw_rect(pixels, w, h, 50, 340, 30, 30, 0xFFFF8800); /* append label */

    /* Render prepend children in a row */
    thing *pp = thing_pool_get(pool, state->s7_prepend_parent);
    if (thing_is_valid(pp) && pp->first_child != 0) {
        int x_off = 160;
        thing_idx first = pp->first_child;
        thing_idx ci = first;
        int count = 0;
        do {
            thing *c = &pool->things[ci];
            draw_rect(pixels, w, h, x_off, 145, (int)c->size, (int)c->size, c->color);
            /* Draw arrow between items */
            if (count > 0) {
                draw_rect(pixels, w, h, x_off - 15, 150, 10, 3, 0xFFCCCCCC);
            }
            x_off += 50;
            ci = c->next_sibling;
            count++;
        } while (ci != first && count < 10);
    }

    /* Render append children in a row */
    thing *ap = thing_pool_get(pool, state->s7_append_parent);
    if (thing_is_valid(ap) && ap->first_child != 0) {
        int x_off = 160;
        thing_idx first = ap->first_child;
        thing_idx ci = first;
        int count = 0;
        do {
            thing *c = &pool->things[ci];
            draw_rect(pixels, w, h, x_off, 345, (int)c->size, (int)c->size, c->color);
            if (count > 0) {
                draw_rect(pixels, w, h, x_off - 15, 350, 10, 3, 0xFFCCCCCC);
            }
            x_off += 50;
            ci = c->next_sibling;
            count++;
        } while (ci != first && count < 10);
    }

    if (hud && hud_size > 0) {
        /* Build child lists as strings */
        char prepend_str[128] = "";
        char append_str[128] = "";
        int prepend_len = 0, append_len = 0;

        /* Walk prepend parent's children */
        thing *prepend_parent = thing_pool_get((thing_pool*)&state->pool, state->s7_prepend_parent);
        if (thing_is_valid(prepend_parent)) {
            thing_idx cur = prepend_parent->first_child;
            thing_idx first = cur;
            if (cur != 0) {
                do {
                    thing *child = thing_pool_get((thing_pool*)&state->pool, cur);
                    if (prepend_len > 0) prepend_len += snprintf(prepend_str + prepend_len,
                        sizeof(prepend_str) - prepend_len, "->");
                    prepend_len += snprintf(prepend_str + prepend_len,
                        sizeof(prepend_str) - prepend_len, "%d", cur);
                    cur = child->next_sibling;
                } while (cur != first && cur != 0 && prepend_len < 100);
            }
        }

        /* Walk append parent's children */
        thing *append_parent = thing_pool_get((thing_pool*)&state->pool, state->s7_append_parent);
        if (thing_is_valid(append_parent)) {
            thing_idx cur = append_parent->first_child;
            thing_idx first = cur;
            if (cur != 0) {
                do {
                    thing *child = thing_pool_get((thing_pool*)&state->pool, cur);
                    if (append_len > 0) append_len += snprintf(append_str + append_len,
                        sizeof(append_str) - append_len, "->");
                    append_len += snprintf(append_str + append_len,
                        sizeof(append_str) - append_len, "%d", cur);
                    cur = child->next_sibling;
                } while (cur != first && cur != 0 && append_len < 100);
            }
        }

        int active = count_pool_active(&state->pool);
        snprintf(hud, hud_size,
                 "Prepend: %s  |  Append: %s  |  Items: %d  |  Pool: %d/%d",
                 prepend_str[0] ? prepend_str : "(empty)",
                 append_str[0] ? append_str : "(empty)",
                 state->s7_items_added, active, MAX_THINGS - 1);
    }
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 8: Default movement demo (original game)        */
/* ══════════════════════════════════════════════════════ */

static void scene8_init(game_state *state)
{
    thing_pool_init(&state->pool);
    state->rng_state = 12345;
    state->player_ref = spawn_player(&state->pool);
    state->spawn_timer = 0;
    state->trolls_spawned = 0;
    state->trolls_killed = 0;
    state->slots_reused = 0;
}

static void scene8_update(game_state *state, const game_input *input, float dt)
{
    /* This is the original game_update logic */
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    move_player(player, input, dt);

    /* Spawn trolls on a timer */
    state->spawn_timer += dt;
    if (state->spawn_timer >= SPAWN_INTERVAL && state->trolls_spawned < MAX_TROLLS * 3) {
        state->spawn_timer = 0.0f;
        thing_idx old_first_free = state->pool.first_free;

        thing_ref troll_ref = thing_pool_add(&state->pool, THING_KIND_TROLL);
        thing *troll = thing_pool_get_ref(&state->pool, troll_ref);
        if (thing_is_valid(troll)) {
            troll->x     = rng_float(&state->rng_state) * (SCREEN_W - TROLL_SIZE * 2) + TROLL_SIZE;
            troll->y     = rng_float(&state->rng_state) * (SCREEN_H - TROLL_SIZE * 2) + TROLL_SIZE;
            troll->size  = TROLL_SIZE;
            troll->color = 0xFF4488CC;
            state->trolls_spawned++;

            if (old_first_free != 0 && troll_ref.index <= state->pool.next_empty) {
                state->slots_reused++;
            }
        }
    }

    /* Collision: player vs trolls */
    if (thing_is_valid(player)) {
        for (thing_iter it = thing_iter_begin(&state->pool);
             thing_iter_valid(&it);
             thing_iter_next(&it))
        {
            thing *t = thing_iter_get(&it);
            if (t->kind != THING_KIND_TROLL) continue;

            float half_p = player->size / 2.0f;
            float half_t = t->size / 2.0f;
            float pcx = player->x + half_p, pcy = player->y + half_p;
            float tcx = t->x + half_t, tcy = t->y + half_t;

            float dx = pcx - tcx;
            float dy = pcy - tcy;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;

            if (dx < (half_p + half_t) && dy < (half_p + half_t)) {
                thing_pool_remove(&state->pool, it.current);
                state->trolls_killed++;
            }
        }
    }
}

static void scene8_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    render_pool(&state->pool, pixels, w, h);

    int active = count_pool_active(&state->pool);
    snprintf(hud, hud_size,
             "Spawned: %d  Killed: %d  Reused: %d  |  Active: %d/%d  |  Free list: %s",
             state->trolls_spawned, state->trolls_killed, state->slots_reused,
             active, MAX_THINGS - 1,
             state->pool.first_free ? "has slots" : "empty");
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 9: Separate pools                               */
/* ══════════════════════════════════════════════════════ */

static void scene9_init(game_state *state)
{
    thing_pool_init(&state->s9_player_pool);
    thing_pool_init(&state->s9_troll_pool);
    state->rng_state = 12345;
    state->spawn_timer = 0;
    state->s9_troll_count = 0;

    /* Player in its own pool */
    state->s9_player_ref = spawn_player(&state->s9_player_pool);
}

static void scene9_update(game_state *state, const game_input *input, float dt)
{
    thing *player = thing_pool_get_ref(&state->s9_player_pool, state->s9_player_ref);
    move_player(player, input, dt);

    /* Spawn trolls in the troll pool */
    state->spawn_timer += dt;
    if (state->spawn_timer >= SPAWN_INTERVAL && state->s9_troll_count < MAX_TROLLS) {
        state->spawn_timer = 0;
        thing_ref troll_ref = thing_pool_add(&state->s9_troll_pool, THING_KIND_TROLL);
        thing *troll = thing_pool_get_ref(&state->s9_troll_pool, troll_ref);
        if (thing_is_valid(troll)) {
            troll->x     = rng_float(&state->rng_state) * (SCREEN_W - TROLL_SIZE * 2) + TROLL_SIZE;
            troll->y     = rng_float(&state->rng_state) * (SCREEN_H - TROLL_SIZE * 2) + TROLL_SIZE;
            troll->size  = TROLL_SIZE;
            troll->color = 0xFF4488CC;
            state->s9_troll_count++;
        }
    }

    /* Cross-pool collision: player pool vs troll pool */
    if (thing_is_valid(player)) {
        for (thing_iter it = thing_iter_begin(&state->s9_troll_pool);
             thing_iter_valid(&it);
             thing_iter_next(&it))
        {
            thing *t = thing_iter_get(&it);
            float half_p = player->size / 2.0f;
            float half_t = t->size / 2.0f;
            float pcx = player->x + half_p, pcy = player->y + half_p;
            float tcx = t->x + half_t, tcy = t->y + half_t;

            float dx = pcx - tcx;
            float dy = pcy - tcy;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;

            if (dx < (half_p + half_t) && dy < (half_p + half_t)) {
                thing_pool_remove(&state->s9_troll_pool, it.current);
                state->s9_troll_count--;
            }
        }
    }
}

static void scene9_render(const game_state *state, uint32_t *pixels, int w, int h,
                           char *hud, int hud_size)
{
    render_pool(&state->s9_player_pool, pixels, w, h);
    render_pool(&state->s9_troll_pool, pixels, w, h);

    int p_used = count_kind(&state->s9_player_pool, THING_KIND_PLAYER);
    int t_used = state->s9_troll_count;

    snprintf(hud, hud_size,
             "Player pool: %d/%d  |  Troll pool: %d/%d  |  Cross-pool collision  |  Killed: %d",
             p_used, MAX_THINGS, t_used, MAX_THINGS, state->trolls_killed);
}

/* ══════════════════════════════════════════════════════ */
/*   Scene 10: Particle Laboratory (conditional)           */
/* ══════════════════════════════════════════════════════ */


#ifndef LAB_MIN
#define LAB_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static uint32_t archetype_color(int archetype)
{
    switch (archetype) {
        case ARCHETYPE_BOUNCE:          return 0xFF88CCFF; /* light blue */
        case ARCHETYPE_WANDER:          return 0xFFFFFF44; /* yellow */
        case ARCHETYPE_SEEK_PLAYER:     return 0xFFFF4444; /* red */
        case ARCHETYPE_PREDICTIVE_SEEK: return 0xFFFF88CC; /* pink */
        case ARCHETYPE_ORBIT:           return 0xFFFFAA44; /* orange */
        case ARCHETYPE_DRIFT_NOISE:     return 0xFF44FF44; /* green */
        default:                        return 0xFFFFFFFF; /* white */
    }
}

static void scene10_init(game_state *state)
{
    thing_pool_init(&state->pool);
    state->rng_state = 31337;
    state->player_ref = spawn_player(&state->pool);

    /* Reset lab-specific counters */
    state->lab_particle_count = 0;
    state->lab_score = 0;
    state->lab_archetype_changes = 0;
    state->lab_collision_checks = 0;

    /* Spawn LAB_MAX_PARTICLES goblins with random archetypes */
    for (int i = 0; i < LAB_MAX_PARTICLES; i++) {
        thing_ref ref = thing_pool_add(&state->pool, THING_KIND_GOBLIN);
        thing *p = thing_pool_get_ref(&state->pool, ref);
        if (!thing_is_valid(p)) break; /* pool full */

        int idx = ref.index;
        int arch = (int)(rng_next(&state->rng_state) % ARCHETYPE_COUNT);

        /* Random position across the screen */
        p->x = rng_float(&state->rng_state) * (SCREEN_W - 20) + 10;
        p->y = rng_float(&state->rng_state) * (SCREEN_H - 20) + 10;

        /* Size 4-8 px */
        p->size = 4.0f + rng_float(&state->rng_state) * 4.0f;

        /* Color based on archetype */
        p->color = archetype_color(arch);

        /* Initial velocity based on archetype */
        switch (arch) {
            case ARCHETYPE_BOUNCE: {
                float speed = 50.0f + rng_float(&state->rng_state) * 100.0f;
                float angle = rng_float(&state->rng_state) * 6.2831853f;
                p->dx = cosf(angle) * speed;
                p->dy = sinf(angle) * speed;
                break;
            }
            case ARCHETYPE_WANDER: {
                float speed = 20.0f + rng_float(&state->rng_state) * 40.0f;
                float angle = rng_float(&state->rng_state) * 6.2831853f;
                p->dx = cosf(angle) * speed;
                p->dy = sinf(angle) * speed;
                break;
            }
            default: {
                /* Seek, predictive seek, orbit, drift: start slow */
                p->dx = (rng_float(&state->rng_state) - 0.5f) * 30.0f;
                p->dy = (rng_float(&state->rng_state) - 0.5f) * 30.0f;
                break;
            }
        }

        /* Parallel arrays — behavior data separate from thing struct */
        state->particle_archetype[idx] = (uint8_t)arch;
        state->particle_aggressiveness[idx] = (uint8_t)(rng_next(&state->rng_state) % 256);
        state->particle_prediction_error[idx] = rng_float(&state->rng_state);
        state->particle_value_score[idx] = 10;
        state->particle_phase[idx] = rng_float(&state->rng_state) * 6.2831853f;

        state->lab_particle_count++;
    }

    /* Initialize previous player position for velocity estimation */
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    if (thing_is_valid(player)) {
        state->lab_prev_player_x = player->x;
        state->lab_prev_player_y = player->y;
    }
}

static void scene10_update(game_state *state, const game_input *input, float dt)
{
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    move_player(player, input, dt);

    /* Estimate player velocity from frame-to-frame position delta */
    float player_vx = 0.0f, player_vy = 0.0f;
    if (thing_is_valid(player) && dt > 0.0001f) {
        player_vx = (player->x - state->lab_prev_player_x) / dt;
        player_vy = (player->y - state->lab_prev_player_y) / dt;
        state->lab_prev_player_x = player->x;
        state->lab_prev_player_y = player->y;
    }

    float px = thing_is_valid(player) ? player->x + player->size / 2.0f : (float)SCREEN_W / 2.0f;
    float py = thing_is_valid(player) ? player->y + player->size / 2.0f : (float)SCREEN_H / 2.0f;

    /* Reset per-frame counters */
    state->lab_collision_checks = 0;

    /* ── Archetype movement update — EXPLICIT for loop (not iterator) ── */
    for (int i = 1; i < state->pool.next_empty; i++) {
        if (!state->pool.used[i]) continue;
        thing *p = &state->pool.things[i];
        if (p->kind != THING_KIND_GOBLIN) continue;

        int arch = state->particle_archetype[i];
        float aggr = (float)state->particle_aggressiveness[i] / 255.0f;

        switch (arch) {
            case ARCHETYPE_BOUNCE: {
                /* Simple wall-reflection movement */
                p->x += p->dx * dt;
                p->y += p->dy * dt;
                if (p->x < 0)           { p->x = 0;           p->dx = -p->dx; }
                if (p->x > SCREEN_W - p->size) { p->x = SCREEN_W - p->size; p->dx = -p->dx; }
                if (p->y < 0)           { p->y = 0;           p->dy = -p->dy; }
                if (p->y > SCREEN_H - p->size) { p->y = SCREEN_H - p->size; p->dy = -p->dy; }
                break;
            }
            case ARCHETYPE_WANDER: {
                /* Small random velocity perturbation + damping */
                p->dx += (rng_float(&state->rng_state) - 0.5f) * 200.0f * dt;
                p->dy += (rng_float(&state->rng_state) - 0.5f) * 200.0f * dt;
                /* Damping */
                p->dx *= 0.98f;
                p->dy *= 0.98f;
                p->x += p->dx * dt;
                p->y += p->dy * dt;
                /* Soft edge wrap */
                if (p->x < -10)  p->x = (float)SCREEN_W + 5;
                if (p->x > SCREEN_W + 10) p->x = -5;
                if (p->y < -10)  p->y = (float)SCREEN_H + 5;
                if (p->y > SCREEN_H + 10) p->y = -5;
                break;
            }
            case ARCHETYPE_SEEK_PLAYER: {
                /* Steer toward current player position */
                float dx = px - (p->x + p->size / 2.0f);
                float dy = py - (p->y + p->size / 2.0f);
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > 1.0f) {
                    float accel = 100.0f + aggr * 200.0f;
                    p->dx += (dx / dist) * accel * dt;
                    p->dy += (dy / dist) * accel * dt;
                }
                /* Speed limit */
                float speed = sqrtf(p->dx * p->dx + p->dy * p->dy);
                float max_speed = 120.0f + aggr * 80.0f;
                if (speed > max_speed) {
                    p->dx = (p->dx / speed) * max_speed;
                    p->dy = (p->dy / speed) * max_speed;
                }
                p->x += p->dx * dt;
                p->y += p->dy * dt;
                break;
            }
            case ARCHETYPE_PREDICTIVE_SEEK: {
                /* Steer toward PREDICTED player position with error */
                float lookahead = 0.3f + aggr * 0.5f;
                float error = state->particle_prediction_error[i];
                float future_x = px + player_vx * lookahead
                               + (rng_float(&state->rng_state) - 0.5f) * error * 100.0f;
                float future_y = py + player_vy * lookahead
                               + (rng_float(&state->rng_state) - 0.5f) * error * 100.0f;
                float dx = future_x - (p->x + p->size / 2.0f);
                float dy = future_y - (p->y + p->size / 2.0f);
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > 1.0f) {
                    float accel = 120.0f + aggr * 250.0f;
                    p->dx += (dx / dist) * accel * dt;
                    p->dy += (dy / dist) * accel * dt;
                }
                float speed = sqrtf(p->dx * p->dx + p->dy * p->dy);
                float max_speed = 150.0f + aggr * 100.0f;
                if (speed > max_speed) {
                    p->dx = (p->dx / speed) * max_speed;
                    p->dy = (p->dy / speed) * max_speed;
                }
                p->x += p->dx * dt;
                p->y += p->dy * dt;
                break;
            }
            case ARCHETYPE_ORBIT: {
                /* Circular orbit around player */
                float orbit_speed = 1.5f + aggr * 2.0f;
                float radius = 60.0f + (float)(i % 80);
                state->particle_phase[i] += orbit_speed * dt;
                float desired_x = px + radius * cosf(state->particle_phase[i]);
                float desired_y = py + radius * sinf(state->particle_phase[i]);
                /* Smooth convergence toward desired position */
                float converge = 3.0f + aggr * 4.0f;
                p->dx = (desired_x - p->x) * converge;
                p->dy = (desired_y - p->y) * converge;
                p->x += p->dx * dt;
                p->y += p->dy * dt;
                break;
            }
            case ARCHETYPE_DRIFT_NOISE: {
                /* Constant base drift + sinusoidal noise overlay */
                float base_speed = 40.0f + aggr * 60.0f;
                state->particle_phase[i] += 2.0f * dt;
                float noise_x = sinf(state->particle_phase[i] * 1.3f + (float)i) * 50.0f;
                float noise_y = cosf(state->particle_phase[i] * 0.9f + (float)i * 0.7f) * 50.0f;
                p->dx = base_speed + noise_x;
                p->dy = noise_y;
                p->x += p->dx * dt;
                p->y += p->dy * dt;
                /* Wrap around screen edges */
                if (p->x < -10) p->x = (float)SCREEN_W + 5;
                if (p->x > SCREEN_W + 10) p->x = -5;
                if (p->y < -10) p->y = (float)SCREEN_H + 5;
                if (p->y > SCREEN_H + 10) p->y = -5;
                break;
            }
        }

        /* Clamp to screen bounds (except bounce, which handles its own) */
        if (arch != ARCHETYPE_BOUNCE && arch != ARCHETYPE_WANDER
            && arch != ARCHETYPE_DRIFT_NOISE) {
            if (p->x < 0) p->x = 0;
            if (p->y < 0) p->y = 0;
            if (p->x > SCREEN_W - p->size) p->x = SCREEN_W - p->size;
            if (p->y > SCREEN_H - p->size) p->y = SCREEN_H - p->size;
        }
    }

    /* ── Collision check: player vs particles — EXPLICIT for loop ── */
    if (thing_is_valid(player)) {
        float half_p = player->size / 2.0f;
        float pcx = player->x + half_p;
        float pcy = player->y + half_p;

        for (int i = 1; i < state->pool.next_empty; i++) {
            if (!state->pool.used[i]) continue;
            thing *p = &state->pool.things[i];
            if (p->kind != THING_KIND_GOBLIN) continue;

            state->lab_collision_checks++;

            /* AABB overlap check */
            float half_t = p->size / 2.0f;
            float tcx = p->x + half_t;
            float tcy = p->y + half_t;
            float dx = pcx - tcx;
            float dy = pcy - tcy;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;

            if (dx < (half_p + half_t) && dy < (half_p + half_t)) {
                /* Collision! DATA REWRITE — mutation, not deletion */
                state->lab_score += state->particle_value_score[i];

                /* Reassign archetype */
                state->particle_archetype[i] = (uint8_t)(rng_next(&state->rng_state) % ARCHETYPE_COUNT);

                /* Increase aggressiveness (capped at 255) */
                int new_aggr = state->particle_aggressiveness[i] + 30;
                state->particle_aggressiveness[i] = (uint8_t)LAB_MIN(255, new_aggr);

                /* Reduce prediction error (gets more accurate) */
                state->particle_prediction_error[i] *= 0.8f;

                /* Update color to match new archetype */
                p->color = archetype_color(state->particle_archetype[i]);

                state->lab_archetype_changes++;

                /* Reset position to random screen edge */
                int edge = (int)(rng_next(&state->rng_state) % 4);
                switch (edge) {
                    case 0: p->x = 0;                    p->y = rng_float(&state->rng_state) * SCREEN_H; break;
                    case 1: p->x = SCREEN_W - p->size;   p->y = rng_float(&state->rng_state) * SCREEN_H; break;
                    case 2: p->x = rng_float(&state->rng_state) * SCREEN_W; p->y = 0;                    break;
                    case 3: p->x = rng_float(&state->rng_state) * SCREEN_W; p->y = SCREEN_H - p->size;   break;
                }

                /* Give it a new random velocity */
                float angle = rng_float(&state->rng_state) * 6.2831853f;
                float speed = 50.0f + rng_float(&state->rng_state) * 100.0f;
                p->dx = cosf(angle) * speed;
                p->dy = sinf(angle) * speed;
            }
        }
    }
}

static void scene10_render(const game_state *state, uint32_t *pixels, int w, int h)
{
    render_pool(&state->pool, pixels, w, h);
}

static void scene10_hud(const game_state *state, char *hud, int hud_size)
{
    /* Count archetypes by iterating the pool explicitly */
    int counts[ARCHETYPE_COUNT];
    memset(counts, 0, sizeof(counts));
    int total = 0;

    for (int i = 1; i < state->pool.next_empty; i++) {
        if (!state->pool.used[i]) continue;
        if (state->pool.things[i].kind != THING_KIND_GOBLIN) continue;
        int arch = state->particle_archetype[i];
        if (arch >= 0 && arch < ARCHETYPE_COUNT) counts[arch]++;
        total++;
    }

    snprintf(hud, hud_size,
             "Entities: %d/%d  |  Score: %d  |  Changes: %d  |  Checks: %d  |  Update: %.0fus\n"
             "B:%d W:%d S:%d P:%d O:%d D:%d",
             total, MAX_THINGS - 1, state->lab_score, state->lab_archetype_changes,
             state->lab_collision_checks, state->lab_update_time_us,
             counts[ARCHETYPE_BOUNCE], counts[ARCHETYPE_WANDER],
             counts[ARCHETYPE_SEEK_PLAYER], counts[ARCHETYPE_PREDICTIVE_SEEK],
             counts[ARCHETYPE_ORBIT], counts[ARCHETYPE_DRIFT_NOISE]);
}


/* ══════════════════════════════════════════════════════ */
/*   Scene 11: Data Layout Toggle Laboratory (conditional) */
/* ══════════════════════════════════════════════════════ */


/* ── Global storage for all three layouts ── */
dl_particle_aos  g_dl_aos[DL_MAX_ENTITIES];
dl_particles_soa g_dl_soa;
dl_particle_hot  g_dl_hot[DL_MAX_ENTITIES];
dl_particle_cold g_dl_cold[DL_MAX_ENTITIES];

const char *dl_layout_name(int layout)
{
    switch (layout) {
        case DL_LAYOUT_AOS:    return "AoS";
        case DL_LAYOUT_SOA:    return "SoA";
        case DL_LAYOUT_HYBRID: return "Hybrid";
        default:               return "???";
    }
}

/* ── Sync helpers: copy active layout data to the other two ── */

static void dl_sync_from_aos(int count)
{
    for (int i = 0; i < count; i++) {
        /* AoS -> SoA */
        g_dl_soa.x[i]      = g_dl_aos[i].x;
        g_dl_soa.y[i]      = g_dl_aos[i].y;
        g_dl_soa.dx[i]     = g_dl_aos[i].dx;
        g_dl_soa.dy[i]     = g_dl_aos[i].dy;
        g_dl_soa.color[i]  = g_dl_aos[i].color;
        g_dl_soa.active[i] = g_dl_aos[i].active;

        /* AoS -> Hybrid */
        g_dl_hot[i].x      = g_dl_aos[i].x;
        g_dl_hot[i].y      = g_dl_aos[i].y;
        g_dl_hot[i].dx     = g_dl_aos[i].dx;
        g_dl_hot[i].dy     = g_dl_aos[i].dy;
        g_dl_cold[i].color  = g_dl_aos[i].color;
        g_dl_cold[i].active = g_dl_aos[i].active;
    }
}

static void dl_sync_from_soa(int count)
{
    for (int i = 0; i < count; i++) {
        /* SoA -> AoS */
        g_dl_aos[i].x      = g_dl_soa.x[i];
        g_dl_aos[i].y      = g_dl_soa.y[i];
        g_dl_aos[i].dx     = g_dl_soa.dx[i];
        g_dl_aos[i].dy     = g_dl_soa.dy[i];
        g_dl_aos[i].color  = g_dl_soa.color[i];
        g_dl_aos[i].active = g_dl_soa.active[i];

        /* SoA -> Hybrid */
        g_dl_hot[i].x      = g_dl_soa.x[i];
        g_dl_hot[i].y      = g_dl_soa.y[i];
        g_dl_hot[i].dx     = g_dl_soa.dx[i];
        g_dl_hot[i].dy     = g_dl_soa.dy[i];
        g_dl_cold[i].color  = g_dl_soa.color[i];
        g_dl_cold[i].active = g_dl_soa.active[i];
    }
}

static void dl_sync_from_hybrid(int count)
{
    for (int i = 0; i < count; i++) {
        /* Hybrid -> AoS */
        g_dl_aos[i].x      = g_dl_hot[i].x;
        g_dl_aos[i].y      = g_dl_hot[i].y;
        g_dl_aos[i].dx     = g_dl_hot[i].dx;
        g_dl_aos[i].dy     = g_dl_hot[i].dy;
        g_dl_aos[i].color  = g_dl_cold[i].color;
        g_dl_aos[i].active = g_dl_cold[i].active;

        /* Hybrid -> SoA */
        g_dl_soa.x[i]      = g_dl_hot[i].x;
        g_dl_soa.y[i]      = g_dl_hot[i].y;
        g_dl_soa.dx[i]     = g_dl_hot[i].dx;
        g_dl_soa.dy[i]     = g_dl_hot[i].dy;
        g_dl_soa.color[i]  = g_dl_cold[i].color;
        g_dl_soa.active[i] = g_dl_cold[i].active;
    }
}

static void scene11_init(game_state *state)
{
    state->rng_state = 54321;
    state->dl_entity_count = DL_MAX_ENTITIES;
    state->dl_layout = DL_LAYOUT_AOS;
    state->dl_layout_switches = 0;
    state->dl_update_time_us = 0.0f;
    state->dl_render_time_us = 0.0f;

    for (int i = 0; i < DL_MAX_ENTITIES; i++) {
        float rx  = rng_float(&state->rng_state) * (float)(SCREEN_W - 10) + 5.0f;
        float ry  = rng_float(&state->rng_state) * (float)(SCREEN_H - 10) + 5.0f;
        float spd = 50.0f + rng_float(&state->rng_state) * 100.0f;
        float ang = rng_float(&state->rng_state) * 6.2831853f;
        float rdx = cosf(ang) * spd;
        float rdy = sinf(ang) * spd;

        /* Simple color hash from index */
        uint32_t color;
        switch (i % 6) {
            case 0: color = 0xFF88CCFF; break; /* light blue */
            case 1: color = 0xFFFF8844; break; /* orange */
            case 2: color = 0xFF44FF88; break; /* green */
            case 3: color = 0xFFFF44FF; break; /* pink */
            case 4: color = 0xFFFFFF44; break; /* yellow */
            default: color = 0xFF44FFFF; break; /* cyan */
        }

        /* AoS */
        g_dl_aos[i].x      = rx;
        g_dl_aos[i].y      = ry;
        g_dl_aos[i].dx     = rdx;
        g_dl_aos[i].dy     = rdy;
        g_dl_aos[i].color  = color;
        g_dl_aos[i].active = 1;

        /* SoA */
        g_dl_soa.x[i]      = rx;
        g_dl_soa.y[i]      = ry;
        g_dl_soa.dx[i]     = rdx;
        g_dl_soa.dy[i]     = rdy;
        g_dl_soa.color[i]  = color;
        g_dl_soa.active[i] = 1;

        /* Hybrid */
        g_dl_hot[i].x      = rx;
        g_dl_hot[i].y      = ry;
        g_dl_hot[i].dx     = rdx;
        g_dl_hot[i].dy     = rdy;
        g_dl_cold[i].color  = color;
        g_dl_cold[i].active = 1;
    }
}

static void scene11_update(game_state *state, const game_input *input, float dt)
{
    /* Tab toggle: cycle layout */
    if (input->tab_pressed) {
        state->dl_layout = (state->dl_layout + 1) % DL_LAYOUT_COUNT;
        state->dl_layout_switches++;
    }

    int count = state->dl_entity_count;

    /* ── Measure update time with clock_gettime ── */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /* ── THREE explicit update loops — identical logic, different access patterns ── */
    switch (state->dl_layout) {
    case DL_LAYOUT_AOS:
        for (int i = 0; i < count; i++) {
            if (!g_dl_aos[i].active) continue;
            /* Move */
            g_dl_aos[i].x += g_dl_aos[i].dx * dt;
            g_dl_aos[i].y += g_dl_aos[i].dy * dt;
            /* Bounce off edges */
            if (g_dl_aos[i].x < 0 || g_dl_aos[i].x > SCREEN_W)
                g_dl_aos[i].dx = -g_dl_aos[i].dx;
            if (g_dl_aos[i].y < 0 || g_dl_aos[i].y > SCREEN_H)
                g_dl_aos[i].dy = -g_dl_aos[i].dy;
            /* Clamp */
            if (g_dl_aos[i].x < 0) g_dl_aos[i].x = 0;
            if (g_dl_aos[i].x > SCREEN_W) g_dl_aos[i].x = (float)SCREEN_W;
            if (g_dl_aos[i].y < 0) g_dl_aos[i].y = 0;
            if (g_dl_aos[i].y > SCREEN_H) g_dl_aos[i].y = (float)SCREEN_H;
        }
        break;

    case DL_LAYOUT_SOA:
        for (int i = 0; i < count; i++) {
            if (!g_dl_soa.active[i]) continue;
            /* Move */
            g_dl_soa.x[i] += g_dl_soa.dx[i] * dt;
            g_dl_soa.y[i] += g_dl_soa.dy[i] * dt;
            /* Bounce off edges */
            if (g_dl_soa.x[i] < 0 || g_dl_soa.x[i] > SCREEN_W)
                g_dl_soa.dx[i] = -g_dl_soa.dx[i];
            if (g_dl_soa.y[i] < 0 || g_dl_soa.y[i] > SCREEN_H)
                g_dl_soa.dy[i] = -g_dl_soa.dy[i];
            /* Clamp */
            if (g_dl_soa.x[i] < 0) g_dl_soa.x[i] = 0;
            if (g_dl_soa.x[i] > SCREEN_W) g_dl_soa.x[i] = (float)SCREEN_W;
            if (g_dl_soa.y[i] < 0) g_dl_soa.y[i] = 0;
            if (g_dl_soa.y[i] > SCREEN_H) g_dl_soa.y[i] = (float)SCREEN_H;
        }
        break;

    case DL_LAYOUT_HYBRID:
        for (int i = 0; i < count; i++) {
            if (!g_dl_cold[i].active) continue;
            /* Move */
            g_dl_hot[i].x += g_dl_hot[i].dx * dt;
            g_dl_hot[i].y += g_dl_hot[i].dy * dt;
            /* Bounce off edges */
            if (g_dl_hot[i].x < 0 || g_dl_hot[i].x > SCREEN_W)
                g_dl_hot[i].dx = -g_dl_hot[i].dx;
            if (g_dl_hot[i].y < 0 || g_dl_hot[i].y > SCREEN_H)
                g_dl_hot[i].dy = -g_dl_hot[i].dy;
            /* Clamp */
            if (g_dl_hot[i].x < 0) g_dl_hot[i].x = 0;
            if (g_dl_hot[i].x > SCREEN_W) g_dl_hot[i].x = (float)SCREEN_W;
            if (g_dl_hot[i].y < 0) g_dl_hot[i].y = 0;
            if (g_dl_hot[i].y > SCREEN_H) g_dl_hot[i].y = (float)SCREEN_H;
        }
        break;
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    {
        long sec_diff  = t_end.tv_sec  - t_start.tv_sec;
        long nsec_diff = t_end.tv_nsec - t_start.tv_nsec;
        float elapsed_us = (float)(sec_diff * 1000000L + nsec_diff / 1000L);
        /* Exponential moving average for smoother display */
        state->dl_update_time_us = state->dl_update_time_us * 0.9f + elapsed_us * 0.1f;
    }

    /* ── Sync from active layout to the other two ── */
    switch (state->dl_layout) {
        case DL_LAYOUT_AOS:    dl_sync_from_aos(count);    break;
        case DL_LAYOUT_SOA:    dl_sync_from_soa(count);    break;
        case DL_LAYOUT_HYBRID: dl_sync_from_hybrid(count); break;
    }
}

static void scene11_render(const game_state *state, uint32_t *pixels, int width, int height)
{
    int count = state->dl_entity_count;

    switch (state->dl_layout) {
    case DL_LAYOUT_AOS:
        for (int i = 0; i < count; i++) {
            if (!g_dl_aos[i].active) continue;
            draw_rect(pixels, width, height,
                      (int)g_dl_aos[i].x, (int)g_dl_aos[i].y,
                      4, 4, g_dl_aos[i].color);
        }
        break;

    case DL_LAYOUT_SOA:
        for (int i = 0; i < count; i++) {
            if (!g_dl_soa.active[i]) continue;
            draw_rect(pixels, width, height,
                      (int)g_dl_soa.x[i], (int)g_dl_soa.y[i],
                      4, 4, g_dl_soa.color[i]);
        }
        break;

    case DL_LAYOUT_HYBRID:
        for (int i = 0; i < count; i++) {
            if (!g_dl_cold[i].active) continue;
            draw_rect(pixels, width, height,
                      (int)g_dl_hot[i].x, (int)g_dl_hot[i].y,
                      4, 4, g_dl_cold[i].color);
        }
        break;
    }
}

static void scene11_hud(const game_state *state, char *hud, int hud_size)
{
    snprintf(hud, hud_size,
             "Data Layout Lab  [L] Enter  [Tab] Toggle  [R] Reset\n"
             "Layout: %s | Entities: %d | Switches: %d | Update: %.0f us",
             dl_layout_name(state->dl_layout),
             state->dl_entity_count,
             state->dl_layout_switches,
             state->dl_update_time_us);
}


/* ══════════════════════════════════════════════════════ */
/*   Scene 12: Spatial Partition Laboratory                 */
/* ══════════════════════════════════════════════════════ */

/* ── Static storage for spatial structures ── */
static sp_grid_cell    g_sp_grid[SP_GRID_CELLS];
static sp_hash_bucket  g_sp_hash[SP_HASH_BUCKETS];
static sp_qt_node      g_sp_qt_nodes[SP_QT_MAX_NODES];
static int             g_sp_qt_node_count;

const char *sp_mode_name(int mode)
{
    switch (mode) {
        case SP_MODE_NAIVE:    return "Naive O(N^2)";
        case SP_MODE_GRID:     return "Uniform Grid";
        case SP_MODE_HASH:     return "Spatial Hash";
        case SP_MODE_QUADTREE: return "Quadtree";
        default:               return "???";
    }
}

/* ── Spatial structure builders ── */

static void sp_build_grid(game_state *state)
{
    memset(g_sp_grid, 0, sizeof(g_sp_grid));
    int count = state->sp_entity_count;
    for (int i = 0; i < count; i++) {
        if (!state->sp_active[i]) continue;
        int cx = (int)(state->sp_x[i] / SP_CELL_W);
        int cy = (int)(state->sp_y[i] / SP_CELL_H);
        if (cx < 0) cx = 0;
        if (cx >= SP_GRID_COLS) cx = SP_GRID_COLS - 1;
        if (cy < 0) cy = 0;
        if (cy >= SP_GRID_ROWS) cy = SP_GRID_ROWS - 1;
        sp_grid_cell *cell = &g_sp_grid[cy * SP_GRID_COLS + cx];
        if (cell->count < SP_MAX_PER_CELL) {
            cell->indices[cell->count++] = i;
        }
    }
}

static void sp_build_hash(game_state *state)
{
    memset(g_sp_hash, 0, sizeof(g_sp_hash));
    int count = state->sp_entity_count;
    for (int i = 0; i < count; i++) {
        if (!state->sp_active[i]) continue;
        int ix = (int)(state->sp_x[i] / SP_CELL_W);
        int iy = (int)(state->sp_y[i] / SP_CELL_H);
        unsigned int hash = ((unsigned)(ix * 73856093) ^ (unsigned)(iy * 19349663)) % SP_HASH_BUCKETS;
        sp_hash_bucket *bucket = &g_sp_hash[hash];
        if (bucket->count < SP_MAX_PER_BUCKET) {
            bucket->indices[bucket->count++] = i;
        }
    }
}

static int sp_qt_alloc_node(float x, float y, float w, float h)
{
    if (g_sp_qt_node_count >= SP_QT_MAX_NODES) return -1;
    int idx = g_sp_qt_node_count++;
    sp_qt_node *n = &g_sp_qt_nodes[idx];
    n->x = x; n->y = y; n->w = w; n->h = h;
    n->count = 0;
    n->children[0] = n->children[1] = n->children[2] = n->children[3] = -1;
    n->is_leaf = true;
    return idx;
}

static void sp_qt_subdivide(int node_idx)
{
    sp_qt_node *n = &g_sp_qt_nodes[node_idx];
    float hw = n->w / 2.0f;
    float hh = n->h / 2.0f;
    n->children[0] = sp_qt_alloc_node(n->x,      n->y,      hw, hh); /* NW */
    n->children[1] = sp_qt_alloc_node(n->x + hw,  n->y,      hw, hh); /* NE */
    n->children[2] = sp_qt_alloc_node(n->x,      n->y + hh,  hw, hh); /* SW */
    n->children[3] = sp_qt_alloc_node(n->x + hw,  n->y + hh,  hw, hh); /* SE */
    n->is_leaf = false;
}

static void sp_qt_insert(game_state *state, int entity_idx, int node_idx, int depth)
{
    if (node_idx < 0 || node_idx >= g_sp_qt_node_count) return;
    sp_qt_node *n = &g_sp_qt_nodes[node_idx];

    /* Check if entity is within this node's bounds */
    float ex = state->sp_x[entity_idx];
    float ey = state->sp_y[entity_idx];
    if (ex < n->x || ex >= n->x + n->w || ey < n->y || ey >= n->y + n->h)
        return;

    if (n->is_leaf) {
        if (n->count < SP_QT_SPLIT_THRESHOLD || depth >= SP_QT_MAX_DEPTH) {
            /* Add to this leaf (if room or max depth) */
            if (n->count < SP_QT_SPLIT_THRESHOLD) {
                n->indices[n->count++] = entity_idx;
            }
            return;
        }
        /* Subdivide and redistribute */
        sp_qt_subdivide(node_idx);
        /* Re-fetch pointer after potential reallocation in static array */
        n = &g_sp_qt_nodes[node_idx];
        /* Redistribute existing entities */
        for (int i = 0; i < n->count; i++) {
            for (int c = 0; c < 4; c++) {
                if (n->children[c] >= 0) {
                    sp_qt_insert(state, n->indices[i], n->children[c], depth + 1);
                }
            }
        }
        n->count = 0;
    }

    /* Insert into the appropriate child */
    for (int c = 0; c < 4; c++) {
        if (n->children[c] >= 0) {
            sp_qt_insert(state, entity_idx, n->children[c], depth + 1);
        }
    }
}

static void sp_build_quadtree(game_state *state)
{
    g_sp_qt_node_count = 0;
    sp_qt_alloc_node(0, 0, (float)SCREEN_W, (float)SCREEN_H);
    int count = state->sp_entity_count;
    for (int i = 0; i < count; i++) {
        if (!state->sp_active[i]) continue;
        sp_qt_insert(state, i, 0, 0);
    }
}

/* Recursive quadtree query: check entity i against all entities in overlapping nodes */
static void sp_qt_query(game_state *state, int entity_i, int node_idx)
{
    if (node_idx < 0 || node_idx >= g_sp_qt_node_count) return;
    sp_qt_node *n = &g_sp_qt_nodes[node_idx];

    float ex = state->sp_x[entity_i];
    float ey = state->sp_y[entity_i];
    float esize = state->sp_size[entity_i];

    /* Check if entity's bounding area overlaps this node */
    float max_rad = esize + 6.0f; /* max possible other entity size */
    if (ex + max_rad < n->x || ex - max_rad >= n->x + n->w ||
        ey + max_rad < n->y || ey - max_rad >= n->y + n->h)
        return;

    if (n->is_leaf) {
        for (int b = 0; b < n->count; b++) {
            int j = n->indices[b];
            if (j <= entity_i) continue; /* avoid double-checking */
            state->sp_checks_this_frame++;
            float dx = state->sp_x[entity_i] - state->sp_x[j];
            float dy = state->sp_y[entity_i] - state->sp_y[j];
            float dist2 = dx * dx + dy * dy;
            float rad = state->sp_size[entity_i] + state->sp_size[j];
            if (dist2 < rad * rad) {
                state->sp_hit[entity_i] = 1;
                state->sp_hit[j] = 1;
                state->sp_collisions_this_frame++;
            }
        }
        return;
    }

    for (int c = 0; c < 4; c++) {
        if (n->children[c] >= 0) {
            sp_qt_query(state, entity_i, n->children[c]);
        }
    }
}

/* ── Scene 12 init ── */

static void scene12_init(game_state *state)
{
    state->rng_state = 77777;
    state->sp_entity_count = SP_MAX_ENTITIES;
    state->sp_mode = SP_MODE_NAIVE;
    state->sp_mode_switches = 0;
    state->sp_show_debug = true;
    state->sp_checks_this_frame = 0;
    state->sp_collisions_this_frame = 0;
    state->sp_update_time_us = 0.0f;

    for (int i = 0; i < SP_MAX_ENTITIES; i++) {
        state->sp_x[i] = rng_float(&state->rng_state) * (float)(SCREEN_W - 10) + 5.0f;
        state->sp_y[i] = rng_float(&state->rng_state) * (float)(SCREEN_H - 10) + 5.0f;
        float speed = 30.0f + rng_float(&state->rng_state) * 70.0f;
        float angle = rng_float(&state->rng_state) * 6.2831853f;
        state->sp_dx[i] = cosf(angle) * speed;
        state->sp_dy[i] = sinf(angle) * speed;
        state->sp_size[i] = 3.0f + rng_float(&state->rng_state) * 3.0f;
        /* Random color */
        switch (i % 6) {
            case 0: state->sp_color[i] = 0xFF88CCFF; break;
            case 1: state->sp_color[i] = 0xFFFF8844; break;
            case 2: state->sp_color[i] = 0xFF44FF88; break;
            case 3: state->sp_color[i] = 0xFFFF44FF; break;
            case 4: state->sp_color[i] = 0xFFFFFF44; break;
            default: state->sp_color[i] = 0xFF44FFFF; break;
        }
        state->sp_active[i] = 1;
        state->sp_hit[i] = 0;
    }
}

/* ── Scene 12 update ── */

static void scene12_update(game_state *state, const game_input *input, float dt)
{
    /* Tab toggle: cycle partition mode */
    if (input->tab_pressed) {
        state->sp_mode = (state->sp_mode + 1) % SP_MODE_COUNT;
        state->sp_mode_switches++;
    }

    int count = state->sp_entity_count;

    /* Move all entities — bounce off walls */
    for (int i = 0; i < count; i++) {
        if (!state->sp_active[i]) continue;
        state->sp_x[i] += state->sp_dx[i] * dt;
        state->sp_y[i] += state->sp_dy[i] * dt;
        if (state->sp_x[i] < 0) { state->sp_x[i] = 0; state->sp_dx[i] = -state->sp_dx[i]; }
        if (state->sp_x[i] > SCREEN_W) { state->sp_x[i] = (float)SCREEN_W; state->sp_dx[i] = -state->sp_dx[i]; }
        if (state->sp_y[i] < 0) { state->sp_y[i] = 0; state->sp_dy[i] = -state->sp_dy[i]; }
        if (state->sp_y[i] > SCREEN_H) { state->sp_y[i] = (float)SCREEN_H; state->sp_dy[i] = -state->sp_dy[i]; }
    }

    /* ── Collision queries — timed ── */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    state->sp_checks_this_frame = 0;
    state->sp_collisions_this_frame = 0;
    memset(state->sp_hit, 0, sizeof(state->sp_hit));

    switch (state->sp_mode) {
    case SP_MODE_NAIVE:
        /* O(N^2) all-pairs — intentionally slow */
        for (int i = 0; i < count; i++) {
            if (!state->sp_active[i]) continue;
            for (int j = i + 1; j < count; j++) {
                if (!state->sp_active[j]) continue;
                state->sp_checks_this_frame++;
                float dx = state->sp_x[i] - state->sp_x[j];
                float dy = state->sp_y[i] - state->sp_y[j];
                float dist2 = dx * dx + dy * dy;
                float rad = state->sp_size[i] + state->sp_size[j];
                if (dist2 < rad * rad) {
                    state->sp_hit[i] = 1;
                    state->sp_hit[j] = 1;
                    state->sp_collisions_this_frame++;
                }
            }
        }
        break;

    case SP_MODE_GRID:
        sp_build_grid(state);
        for (int cy = 0; cy < SP_GRID_ROWS; cy++) {
            for (int cx = 0; cx < SP_GRID_COLS; cx++) {
                sp_grid_cell *cell = &g_sp_grid[cy * SP_GRID_COLS + cx];
                /* Check within same cell */
                for (int a = 0; a < cell->count; a++) {
                    for (int b = a + 1; b < cell->count; b++) {
                        int ii = cell->indices[a], jj = cell->indices[b];
                        state->sp_checks_this_frame++;
                        float dx = state->sp_x[ii] - state->sp_x[jj];
                        float dy = state->sp_y[ii] - state->sp_y[jj];
                        float dist2 = dx * dx + dy * dy;
                        float rad = state->sp_size[ii] + state->sp_size[jj];
                        if (dist2 < rad * rad) {
                            state->sp_hit[ii] = 1;
                            state->sp_hit[jj] = 1;
                            state->sp_collisions_this_frame++;
                        }
                    }
                }
                /* Check 4 forward neighbors to avoid double-counting */
                int neighbors[4][2] = {{cx+1,cy}, {cx,cy+1}, {cx+1,cy+1}, {cx-1,cy+1}};
                for (int n = 0; n < 4; n++) {
                    int nx = neighbors[n][0], ny = neighbors[n][1];
                    if (nx < 0 || nx >= SP_GRID_COLS || ny < 0 || ny >= SP_GRID_ROWS) continue;
                    sp_grid_cell *ncell = &g_sp_grid[ny * SP_GRID_COLS + nx];
                    for (int a = 0; a < cell->count; a++) {
                        for (int b = 0; b < ncell->count; b++) {
                            int ii = cell->indices[a], jj = ncell->indices[b];
                            state->sp_checks_this_frame++;
                            float dx = state->sp_x[ii] - state->sp_x[jj];
                            float dy = state->sp_y[ii] - state->sp_y[jj];
                            float dist2 = dx * dx + dy * dy;
                            float rad = state->sp_size[ii] + state->sp_size[jj];
                            if (dist2 < rad * rad) {
                                state->sp_hit[ii] = 1;
                                state->sp_hit[jj] = 1;
                                state->sp_collisions_this_frame++;
                            }
                        }
                    }
                }
            }
        }
        break;

    case SP_MODE_HASH:
        sp_build_hash(state);
        for (int i = 0; i < count; i++) {
            if (!state->sp_active[i]) continue;
            int ix = (int)(state->sp_x[i] / SP_CELL_W);
            int iy = (int)(state->sp_y[i] / SP_CELL_H);
            /* Check 3x3 neighborhood of hash cells */
            for (int dy2 = -1; dy2 <= 1; dy2++) {
                for (int dx2 = -1; dx2 <= 1; dx2++) {
                    int hx = ix + dx2, hy = iy + dy2;
                    unsigned int hash = ((unsigned)(hx * 73856093) ^ (unsigned)(hy * 19349663)) % SP_HASH_BUCKETS;
                    sp_hash_bucket *bucket = &g_sp_hash[hash];
                    for (int b = 0; b < bucket->count; b++) {
                        int j = bucket->indices[b];
                        if (j <= i) continue; /* avoid double-checking */
                        state->sp_checks_this_frame++;
                        float dx = state->sp_x[i] - state->sp_x[j];
                        float dy = state->sp_y[i] - state->sp_y[j];
                        float dist2 = dx * dx + dy * dy;
                        float rad = state->sp_size[i] + state->sp_size[j];
                        if (dist2 < rad * rad) {
                            state->sp_hit[i] = 1;
                            state->sp_hit[j] = 1;
                            state->sp_collisions_this_frame++;
                        }
                    }
                }
            }
        }
        break;

    case SP_MODE_QUADTREE:
        sp_build_quadtree(state);
        for (int i = 0; i < count; i++) {
            if (!state->sp_active[i]) continue;
            sp_qt_query(state, i, 0);
        }
        break;
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    {
        long sec_diff  = t_end.tv_sec  - t_start.tv_sec;
        long nsec_diff = t_end.tv_nsec - t_start.tv_nsec;
        float elapsed_us = (float)(sec_diff * 1000000L + nsec_diff / 1000L);
        /* Exponential moving average for smoother display */
        state->sp_update_time_us = state->sp_update_time_us * 0.9f + elapsed_us * 0.1f;
    }
}

/* ── Scene 12 render ── */

static void scene12_render(const game_state *state, uint32_t *pixels, int width, int height)
{
    int count = state->sp_entity_count;

    /* Debug overlay: draw spatial structure visualization */
    switch (state->sp_mode) {
    case SP_MODE_GRID:
    case SP_MODE_HASH:
        /* Draw grid lines */
        for (int gx = 1; gx < SP_GRID_COLS; gx++) {
            int px = gx * SP_CELL_W;
            if (px >= 0 && px < width) {
                for (int y = 0; y < height; y++) {
                    pixels[y * width + px] = 0xFF222244;
                }
            }
        }
        for (int gy = 1; gy < SP_GRID_ROWS; gy++) {
            int py = gy * SP_CELL_H;
            if (py >= 0 && py < height) {
                for (int x = 0; x < width; x++) {
                    pixels[py * width + x] = 0xFF222244;
                }
            }
        }
        /* Highlight occupied cells */
        if (state->sp_mode == SP_MODE_GRID) {
            for (int cy = 0; cy < SP_GRID_ROWS; cy++) {
                for (int cx = 0; cx < SP_GRID_COLS; cx++) {
                    int cnt = g_sp_grid[cy * SP_GRID_COLS + cx].count;
                    if (cnt > 0) {
                        /* Dim highlight proportional to entity count */
                        int intensity = cnt * 8;
                        if (intensity > 60) intensity = 60;
                        int x0 = cx * SP_CELL_W + 1;
                        int y0 = cy * SP_CELL_H + 1;
                        int x1 = x0 + SP_CELL_W - 2;
                        int y1 = y0 + SP_CELL_H - 2;
                        if (x1 > width) x1 = width;
                        if (y1 > height) y1 = height;
                        for (int y = y0; y < y1; y++) {
                            for (int x = x0; x < x1; x++) {
                                if (x >= 0 && x < width && y >= 0 && y < height) {
                                    uint32_t bg = pixels[y * width + x];
                                    /* Simple additive blend */
                                    int r = (int)((bg >> 16) & 0xFF) + (intensity / 3);
                                    int g = (int)((bg >>  8) & 0xFF) + intensity;
                                    int b = (int)((bg      ) & 0xFF) + (intensity / 2);
                                    if (r > 255) r = 255;
                                    if (g > 255) g = 255;
                                    if (b > 255) b = 255;
                                    pixels[y * width + x] = 0xFF000000 | ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
                                }
                            }
                        }
                    }
                }
            }
        }
        break;

    case SP_MODE_QUADTREE: {
        /* Draw quadtree subdivision boxes */
        for (int ni = 0; ni < g_sp_qt_node_count; ni++) {
            sp_qt_node *n = &g_sp_qt_nodes[ni];
            int bx0 = (int)n->x;
            int by0 = (int)n->y;
            int bx1 = (int)(n->x + n->w);
            int by1 = (int)(n->y + n->h);
            /* Draw top edge */
            if (by0 >= 0 && by0 < height) {
                for (int x = bx0; x < bx1 && x < width; x++) {
                    if (x >= 0) pixels[by0 * width + x] = 0xFF333366;
                }
            }
            /* Draw left edge */
            if (bx0 >= 0 && bx0 < width) {
                for (int y = by0; y < by1 && y < height; y++) {
                    if (y >= 0) pixels[y * width + bx0] = 0xFF333366;
                }
            }
        }
        break;
    }

    default:
        /* Naive mode: no spatial structure to draw */
        break;
    }

    /* Draw all entities */
    for (int i = 0; i < count; i++) {
        if (!state->sp_active[i]) continue;
        int sz = (int)state->sp_size[i];
        if (sz < 1) sz = 1;
        uint32_t color = state->sp_hit[i] ? 0xFFFFFFFF : state->sp_color[i];
        draw_rect(pixels, width, height,
                  (int)state->sp_x[i], (int)state->sp_y[i],
                  sz, sz, color);
    }
}

/* ── Scene 12 HUD ── */

static void scene12_hud(const game_state *state, char *hud, int hud_size)
{
    int naive_expected = state->sp_entity_count * (state->sp_entity_count - 1) / 2;
    float reduction = naive_expected > 0 ? (float)state->sp_checks_this_frame / naive_expected : 0;
    snprintf(hud, hud_size,
             "Spatial Partition Lab  [G] Enter  [Tab] Toggle Mode  [R] Reset\n"
             "Mode: %s | Entities: %d | Checks: %d (%.0f%% of naive %d) | Collisions: %d | Update: %.0fus",
             sp_mode_name(state->sp_mode),
             state->sp_entity_count,
             state->sp_checks_this_frame, reduction * 100.0f, naive_expected,
             state->sp_collisions_this_frame,
             state->sp_update_time_us);
}


/* ══════════════════════════════════════════════════════ */
/*    Scene 13: Memory Arena Laboratory                    */
/* ══════════════════════════════════════════════════════ */

const char *ma_mode_name(int mode)
{
    switch (mode) {
        case MA_MODE_MALLOC:  return "malloc/free";
        case MA_MODE_ARENA:   return "Linear Arena";
        case MA_MODE_POOL:    return "Pool Allocator";
        case MA_MODE_SCRATCH: return "Scratch Arena";
        default:              return "???";
    }
}

/* Helper: find first inactive entity slot */
static int ma_find_inactive_slot(const game_state *state)
{
    for (int i = 0; i < MA_MAX_ENTITIES; i++) {
        if (!state->ma_active[i]) return i;
    }
    return -1;
}

/* Helper: init entity at given slot with random values */
static void ma_init_entity(game_state *state, int slot)
{
    state->ma_x[slot]        = rng_float(&state->rng_state) * (float)(SCREEN_W - 10) + 5.0f;
    state->ma_y[slot]        = rng_float(&state->rng_state) * (float)(SCREEN_H - 50) + 5.0f;
    float speed              = 30.0f + rng_float(&state->rng_state) * 50.0f;
    float angle              = rng_float(&state->rng_state) * 6.2831853f;
    state->ma_dx[slot]       = cosf(angle) * speed;
    state->ma_dy[slot]       = sinf(angle) * speed;
    state->ma_lifetime[slot] = 0.5f + rng_float(&state->rng_state) * 2.5f;
    /* Random color */
    switch (rng_next(&state->rng_state) % 6) {
        case 0: state->ma_color[slot] = 0xFF88CCFF; break;
        case 1: state->ma_color[slot] = 0xFFFF8844; break;
        case 2: state->ma_color[slot] = 0xFF44FF88; break;
        case 3: state->ma_color[slot] = 0xFFFF44FF; break;
        case 4: state->ma_color[slot] = 0xFFFFFF44; break;
        default: state->ma_color[slot] = 0xFF44FFFF; break;
    }
    state->ma_active[slot] = 1;
}

static void scene13_init(game_state *state)
{
    state->rng_state = 99999;

    /* Clear all ma_* fields */
    state->ma_mode = MA_MODE_MALLOC;
    state->ma_allocs_this_frame = 0;
    state->ma_frees_this_frame = 0;
    state->ma_bytes_used = 0;
    state->ma_peak_bytes = 0;
    state->ma_update_time_us = 0.0f;
    memset(state->ma_frame_times, 0, sizeof(state->ma_frame_times));
    state->ma_frame_idx = 0;
    state->ma_entity_count = 0;
    state->ma_spawn_timer = 0.0f;
    state->ma_arena_offset = 0;
    memset(state->ma_arena_mem, 0, sizeof(state->ma_arena_mem));
    memset(state->ma_pool_used, 0, sizeof(state->ma_pool_used));
    memset(state->ma_active, 0, sizeof(state->ma_active));

    /* Spawn initial batch of ~200 entities */
    for (int i = 0; i < 200; i++) {
        int slot = ma_find_inactive_slot(state);
        if (slot >= 0) {
            ma_init_entity(state, slot);
            state->ma_entity_count++;
        }
    }
}

static void scene13_update(game_state *state, const game_input *input, float dt)
{
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /* Tab toggle: cycle allocation mode */
    if (input->tab_pressed) {
        state->ma_mode = (state->ma_mode + 1) % MA_MODE_COUNT;
        /* Reset allocator state when switching */
        state->ma_arena_offset = 0;
        state->ma_bytes_used = 0;
        memset(state->ma_pool_used, 0, sizeof(state->ma_pool_used));
        /* Re-mark active entities in pool mode */
        if (state->ma_mode == MA_MODE_POOL) {
            for (int i = 0; i < MA_MAX_ENTITIES; i++) {
                state->ma_pool_used[i] = state->ma_active[i];
            }
        }
    }

    /* Reset per-frame counters */
    state->ma_allocs_this_frame = 0;
    state->ma_frees_this_frame = 0;

    /* Spawn wave: every 0.1s, spawn 10-30 new entities */
    state->ma_spawn_timer += dt;
    if (state->ma_spawn_timer >= 0.1f) {
        state->ma_spawn_timer -= 0.1f;
        int wave_size = 10 + (int)(rng_float(&state->rng_state) * 20.0f);

        for (int w = 0; w < wave_size; w++) {
            switch (state->ma_mode) {
            case MA_MODE_MALLOC: {
                /* Real malloc -- demonstrates overhead and fragmentation */
                void *dummy = malloc(48);
                if (dummy) { free(dummy); }
                state->ma_allocs_this_frame++;
                state->ma_bytes_used += 48;
                /* Actually store in parallel arrays */
                int slot = ma_find_inactive_slot(state);
                if (slot >= 0) {
                    ma_init_entity(state, slot);
                    state->ma_entity_count++;
                }
                break;
            }
            case MA_MODE_ARENA: {
                /* Bump allocator -- just advance offset */
                if (state->ma_arena_offset + 48 <= MA_ARENA_SIZE) {
                    state->ma_arena_offset += 48;
                    state->ma_allocs_this_frame++;
                    state->ma_bytes_used = state->ma_arena_offset;
                }
                int slot = ma_find_inactive_slot(state);
                if (slot >= 0) {
                    ma_init_entity(state, slot);
                    state->ma_entity_count++;
                }
                break;
            }
            case MA_MODE_POOL: {
                /* Pool: find free slot, mark used */
                for (int i = 0; i < MA_MAX_ENTITIES; i++) {
                    if (!state->ma_pool_used[i]) {
                        state->ma_pool_used[i] = 1;
                        state->ma_allocs_this_frame++;
                        /* Init entity at this slot */
                        ma_init_entity(state, i);
                        state->ma_entity_count++;
                        break;
                    }
                }
                break;
            }
            case MA_MODE_SCRATCH: {
                /* Scratch: bump offset (will be reset at frame end) */
                state->ma_arena_offset += 48;
                state->ma_allocs_this_frame++;
                state->ma_bytes_used = state->ma_arena_offset;
                int slot = ma_find_inactive_slot(state);
                if (slot >= 0) {
                    ma_init_entity(state, slot);
                    state->ma_entity_count++;
                }
                break;
            }
            }
        }
    }

    /* Kill expired entities and move active ones */
    int alive = 0;
    for (int i = 0; i < MA_MAX_ENTITIES; i++) {
        if (!state->ma_active[i]) continue;

        state->ma_lifetime[i] -= dt;
        if (state->ma_lifetime[i] <= 0.0f) {
            /* Entity expired -- handle deallocation per mode */
            switch (state->ma_mode) {
            case MA_MODE_MALLOC: {
                /* Simulate dealloc overhead with real malloc/free */
                void *dummy = malloc(1);
                if (dummy) { free(dummy); }
                state->ma_frees_this_frame++;
                state->ma_bytes_used -= 48;
                if (state->ma_bytes_used < 0) state->ma_bytes_used = 0;
                break;
            }
            case MA_MODE_ARENA:
                /* Can't free individual arena allocs -- no-op */
                break;
            case MA_MODE_POOL:
                state->ma_pool_used[i] = 0;
                state->ma_frees_this_frame++;
                break;
            case MA_MODE_SCRATCH:
                /* Scratch resets each frame -- no-op */
                break;
            }
            state->ma_active[i] = 0;
            continue;
        }

        /* Move active entity -- bounce off walls */
        state->ma_x[i] += state->ma_dx[i] * dt;
        state->ma_y[i] += state->ma_dy[i] * dt;
        if (state->ma_x[i] < 0) { state->ma_x[i] = 0; state->ma_dx[i] = -state->ma_dx[i]; }
        if (state->ma_x[i] > (float)(SCREEN_W - 4)) { state->ma_x[i] = (float)(SCREEN_W - 4); state->ma_dx[i] = -state->ma_dx[i]; }
        if (state->ma_y[i] < 0) { state->ma_y[i] = 0; state->ma_dy[i] = -state->ma_dy[i]; }
        if (state->ma_y[i] > (float)(SCREEN_H - 30)) { state->ma_y[i] = (float)(SCREEN_H - 30); state->ma_dy[i] = -state->ma_dy[i]; }
        alive++;
    }
    state->ma_entity_count = alive;

    /* Scratch mode frame reset: everything allocated this frame is gone */
    if (state->ma_mode == MA_MODE_SCRATCH) {
        state->ma_arena_offset = 0;
        state->ma_bytes_used = 0;
    }

    /* Arena mode periodic reset: if arena is >75% full and no entities alive */
    if (state->ma_mode == MA_MODE_ARENA && alive == 0 &&
        state->ma_arena_offset > (MA_ARENA_SIZE * 3) / 4) {
        state->ma_arena_offset = 0;
        state->ma_bytes_used = 0;
    }

    /* Track peak memory */
    if (state->ma_bytes_used > state->ma_peak_bytes) {
        state->ma_peak_bytes = state->ma_bytes_used;
    }

    /* Frame time history */
    state->ma_frame_times[state->ma_frame_idx % 120] = dt * 1000.0f;
    state->ma_frame_idx++;

    /* Measure update timing */
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    {
        long sec_diff  = t_end.tv_sec  - t_start.tv_sec;
        long nsec_diff = t_end.tv_nsec - t_start.tv_nsec;
        float elapsed_us = (float)(sec_diff * 1000000L + nsec_diff / 1000L);
        state->ma_update_time_us = state->ma_update_time_us * 0.9f + elapsed_us * 0.1f;
    }
}

/* ── Scene 13 render ── */

static void scene13_render(const game_state *state, uint32_t *pixels, int width, int height)
{
    /* Draw all active entities as colored rectangles */
    for (int i = 0; i < MA_MAX_ENTITIES; i++) {
        if (!state->ma_active[i]) continue;
        draw_rect(pixels, width, height,
                  (int)state->ma_x[i], (int)state->ma_y[i],
                  4, 4, state->ma_color[i]);
    }

    /* Memory visualization bar at bottom of screen */
    int bar_y = height - 20;
    int bar_h = 16;

    switch (state->ma_mode) {
    case MA_MODE_MALLOC: {
        /* Fragmentation indicator: pseudo-random filled/empty blocks */
        unsigned int frag_rng = 42;
        for (int bx = 0; bx < width; bx += 4) {
            unsigned int r = rng_next(&frag_rng);
            uint32_t color = (r % 3 == 0) ? 0xFF445566 : 0xFF112233;
            draw_rect(pixels, width, height, bx, bar_y, 3, bar_h, color);
        }
        break;
    }
    case MA_MODE_ARENA: {
        /* Horizontal bar: width proportional to arena usage */
        int fill_w = 0;
        if (MA_ARENA_SIZE > 0) {
            fill_w = (int)((float)state->ma_arena_offset / (float)MA_ARENA_SIZE * (float)width);
        }
        if (fill_w > width) fill_w = width;
        /* Background (empty) */
        draw_rect(pixels, width, height, 0, bar_y, width, bar_h, 0xFF112211);
        /* Filled portion (green) */
        if (fill_w > 0) {
            draw_rect(pixels, width, height, 0, bar_y, fill_w, bar_h, 0xFF22AA44);
        }
        break;
    }
    case MA_MODE_POOL: {
        /* Row of tiny squares showing slot occupancy */
        int cols = width / 3;
        if (cols > MA_MAX_ENTITIES) cols = MA_MAX_ENTITIES;
        for (int i = 0; i < cols; i++) {
            uint32_t color = state->ma_pool_used[i] ? 0xFF22AA44 : 0xFF112211;
            draw_rect(pixels, width, height, i * 3, bar_y, 2, 2, color);
        }
        break;
    }
    case MA_MODE_SCRATCH: {
        /* Same as arena but visually pulses — bar shows current frame's usage */
        /* Since scratch resets each frame, this appears as a brief flash */
        int fill_w = 0;
        if (MA_ARENA_SIZE > 0) {
            fill_w = (int)((float)state->ma_arena_offset / (float)MA_ARENA_SIZE * (float)width);
        }
        if (fill_w > width) fill_w = width;
        draw_rect(pixels, width, height, 0, bar_y, width, bar_h, 0xFF111122);
        if (fill_w > 0) {
            draw_rect(pixels, width, height, 0, bar_y, fill_w, bar_h, 0xFF4488FF);
        }
        break;
    }
    }
}

/* ── Scene 13 HUD ── */

static void scene13_hud(const game_state *state, char *hud, int hud_size)
{
    int capacity_kb = MA_ARENA_SIZE / 1024;
    int pct = MA_ARENA_SIZE > 0 ? (state->ma_bytes_used * 100 / MA_ARENA_SIZE) : 0;
    snprintf(hud, hud_size,
             "Memory Arena Lab  [M] Enter  [Tab] Toggle Mode  [R] Reset\n"
             "Mode: %s | Entities: %d | Allocs: %d  Frees: %d | %dKB/%dKB (%d%%) | Peak: %dKB | %.0fus",
             ma_mode_name(state->ma_mode),
             state->ma_entity_count,
             state->ma_allocs_this_frame,
             state->ma_frees_this_frame,
             state->ma_bytes_used / 1024, capacity_kb, pct,
             state->ma_peak_bytes / 1024,
             state->ma_update_time_us);
}

/* ══════════════════════════════════════════════════════════════════════ */
/*      Scene 14: Infinite Growth Laboratory (Capstone)                 */
/*                                                                      */
/*  10 modes covering the full progression from naive malloc to         */
/*  production-quality engine patterns, including threading.            */
/*                                                                      */
/*  Stage 1 (Allocation): Malloc, Arena, Pool                          */
/*  Stage 2 (Lifetime):   TTL Cleanup, Budget Cap                      */
/*  Stage 3 (Spatial):    Streaming, LOD                               */
/*  Stage 4 (Work Dist.): Amortized, Threaded                          */
/*  Stage 5 (Production): All combined                                 */
/* ══════════════════════════════════════════════════════════════════════ */

const char *ig_mode_name(int mode)
{
    switch (mode) {
        case IG_MODE_MALLOC:     return "0: malloc/realloc (BAD)";
        case IG_MODE_ARENA:      return "1: Arena Pre-alloc";
        case IG_MODE_POOL:       return "2: Pool Recycling";
        case IG_MODE_LIFETIME:   return "3: Lifetime Cleanup";
        case IG_MODE_BUDGET:     return "4: Budget Cap";
        case IG_MODE_STREAMING:  return "5: Spatial Streaming";
        case IG_MODE_LOD:        return "6: Simulation LOD";
        case IG_MODE_AMORTIZED:  return "7: Amortized Work";
        case IG_MODE_THREADED:   return "8: Threaded Update";
        case IG_MODE_PRODUCTION: return "9: Production Combo";
        default:                 return "???";
    }
}

static int ig_mode_stage(int mode)
{
    if (mode <= IG_MODE_POOL)       return 1;
    if (mode <= IG_MODE_BUDGET)     return 2;
    if (mode <= IG_MODE_LOD)        return 3;
    if (mode <= IG_MODE_THREADED)   return 4;
    return 5; /* PRODUCTION */
}

/* ── Helper: draw a circle outline into the pixel buffer ── */
static void draw_circle_outline(uint32_t *pixels, int buf_w, int buf_h,
                                int cx, int cy, int radius, uint32_t color)
{
    if (!pixels) return;
    /* Midpoint circle algorithm */
    int x = radius, y = 0;
    int err = 1 - radius;

    while (x >= y) {
        /* Plot 8 octants */
        int pts[8][2] = {
            {cx + x, cy + y}, {cx - x, cy + y},
            {cx + x, cy - y}, {cx - x, cy - y},
            {cx + y, cy + x}, {cx - y, cy + x},
            {cx + y, cy - x}, {cx - y, cy - x},
        };
        for (int i = 0; i < 8; i++) {
            int px = pts[i][0], py = pts[i][1];
            if (px >= 0 && px < buf_w && py >= 0 && py < buf_h) {
                pixels[py * buf_w + px] = color;
            }
        }
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* ── Arena allocator for Mode 1 (ARENA) and Mode 9 (PRODUCTION) ── */

#define IG_ARENA_SIZE (4 * 1024 * 1024) /* 4 MB */
#define IG_POOL_SIZE  4096              /* fixed pool for pool/production modes */

static uint8_t *g_ig_arena_mem;
static int      g_ig_arena_offset;
static int      g_ig_arena_cap;

static void *ig_arena_push(int size)
{
    if (g_ig_arena_offset + size > g_ig_arena_cap) return NULL;
    void *ptr = g_ig_arena_mem + g_ig_arena_offset;
    g_ig_arena_offset += size;
    return ptr;
}

static void ig_arena_reset(void)
{
    g_ig_arena_offset = 0;
}

/* ── ig_grow: reallocate all 7 parallel arrays to new_cap ── */
static void ig_grow(game_state *state, int new_cap)
{
    state->ig_x        = realloc(state->ig_x,        new_cap * sizeof(float));
    state->ig_y        = realloc(state->ig_y,        new_cap * sizeof(float));
    state->ig_dx       = realloc(state->ig_dx,       new_cap * sizeof(float));
    state->ig_dy       = realloc(state->ig_dy,       new_cap * sizeof(float));
    state->ig_lifetime = realloc(state->ig_lifetime,  new_cap * sizeof(float));
    state->ig_color    = realloc(state->ig_color,     new_cap * sizeof(uint32_t));
    state->ig_active   = realloc(state->ig_active,    new_cap * sizeof(uint8_t));
    /* Zero the NEW slots so they start as inactive */
    if (new_cap > state->ig_capacity) {
        int diff = new_cap - state->ig_capacity;
        memset(state->ig_active + state->ig_capacity, 0, diff * sizeof(uint8_t));
    }
    state->ig_capacity = new_cap;
    state->ig_bytes_allocated = new_cap * (4 * (int)sizeof(float)
                                           + (int)sizeof(float)
                                           + (int)sizeof(uint32_t)
                                           + (int)sizeof(uint8_t));
}

/* ── ig_is_arena_ptr: check if a pointer lives inside the arena block ── */
static bool ig_is_arena_ptr(const void *ptr)
{
    if (!g_ig_arena_mem || !ptr) return false;
    return ((const uint8_t *)ptr >= g_ig_arena_mem &&
            (const uint8_t *)ptr < g_ig_arena_mem + g_ig_arena_cap);
}

/* ── ig_free: release all malloc'd arrays (safe for arena pointers) ── */
static void ig_free(game_state *state)
{
    if (!ig_is_arena_ptr(state->ig_x)) {
        free(state->ig_x);
        free(state->ig_y);
        free(state->ig_dx);
        free(state->ig_dy);
        free(state->ig_lifetime);
        free(state->ig_color);
        free(state->ig_active);
    }
    /* Either way, NULL the pointers */
    state->ig_x        = NULL;
    state->ig_y        = NULL;
    state->ig_dx       = NULL;
    state->ig_dy       = NULL;
    state->ig_lifetime = NULL;
    state->ig_color    = NULL;
    state->ig_active   = NULL;
    state->ig_capacity       = 0;
    state->ig_bytes_allocated = 0;
}

/* ── ig_find_inactive: return first inactive slot, or -1 ── */
static int ig_find_inactive(const game_state *state)
{
    for (int i = 0; i < state->ig_capacity; i++) {
        if (!state->ig_active[i]) return i;
    }
    return -1;
}

/* ── ig_arena_alloc_arrays: allocate entity arrays from arena ── */
static void ig_arena_alloc_arrays(game_state *state, int cap)
{
    ig_arena_reset();
    state->ig_x        = ig_arena_push(cap * (int)sizeof(float));
    state->ig_y        = ig_arena_push(cap * (int)sizeof(float));
    state->ig_dx       = ig_arena_push(cap * (int)sizeof(float));
    state->ig_dy       = ig_arena_push(cap * (int)sizeof(float));
    state->ig_lifetime = ig_arena_push(cap * (int)sizeof(float));
    state->ig_color    = ig_arena_push(cap * (int)sizeof(uint32_t));
    state->ig_active   = ig_arena_push(cap * (int)sizeof(uint8_t));
    if (state->ig_active) {
        memset(state->ig_active, 0, cap * sizeof(uint8_t));
    }
    state->ig_capacity = cap;
    state->ig_bytes_allocated = g_ig_arena_offset;
}

/* ── Threading support for Mode 8 (THREADED) ── */

typedef struct {
    game_state *state;
    int start, end;
    float dt;
} ig_thread_work;

static void *ig_thread_update(void *arg)
{
    ig_thread_work *w = (ig_thread_work *)arg;
    for (int i = w->start; i < w->end; i++) {
        if (!w->state->ig_active[i]) continue;
        w->state->ig_lifetime[i] -= w->dt;
        if (w->state->ig_lifetime[i] <= 0) {
            w->state->ig_active[i] = 0;
            continue;
        }
        w->state->ig_x[i] += w->state->ig_dx[i] * w->dt;
        w->state->ig_y[i] += w->state->ig_dy[i] * w->dt;
        if (w->state->ig_x[i] < 0 || w->state->ig_x[i] > SCREEN_W)
            w->state->ig_dx[i] = -w->state->ig_dx[i];
        if (w->state->ig_y[i] < 0 || w->state->ig_y[i] > SCREEN_H)
            w->state->ig_dy[i] = -w->state->ig_dy[i];
    }
    return NULL;
}

/* ── Spawn helper: fill a slot with random entity data ── */
static void ig_spawn_entity(game_state *state, int slot)
{
    state->ig_x[slot]        = rng_float(&state->rng_state) * SCREEN_W;
    state->ig_y[slot]        = rng_float(&state->rng_state) * SCREEN_H;
    state->ig_dx[slot]       = (rng_float(&state->rng_state) - 0.5f) * 100.0f;
    state->ig_dy[slot]       = (rng_float(&state->rng_state) - 0.5f) * 100.0f;
    state->ig_lifetime[slot] = 1.0f + rng_float(&state->rng_state) * 4.0f;
    state->ig_color[slot]    = 0xFF000000 | (rng_next(&state->rng_state) & 0xFFFFFF);
    state->ig_active[slot]   = 1;
    state->ig_total_created++;
}

/* ── scene14_init_mode: set up arrays for the current mode ── */
static void scene14_init_mode(game_state *state)
{
    /* ig_free is arena-safe: it checks whether pointers live in the
     * arena and skips free() if so. */
    ig_free(state);

    state->ig_active_count    = 0;
    state->ig_peak_active     = 0;
    state->ig_total_created   = 0;
    state->ig_destroyed_count = 0;
    state->ig_spawn_accum     = 0.0f;

    /* Allocate based on mode */
    switch (state->ig_mode) {
    case IG_MODE_MALLOC:
        /* Start small — grows via realloc */
        ig_grow(state, 256);
        break;

    case IG_MODE_ARENA:
        /* Pre-alloc from the arena — fixed capacity, no realloc */
        ig_arena_alloc_arrays(state, IG_POOL_SIZE);
        break;

    case IG_MODE_POOL:
    case IG_MODE_LIFETIME:
    case IG_MODE_BUDGET:
    case IG_MODE_STREAMING:
    case IG_MODE_LOD:
    case IG_MODE_AMORTIZED:
    case IG_MODE_THREADED:
        /* Fixed-size pool via malloc — no growth */
        ig_grow(state, IG_POOL_SIZE);
        break;

    case IG_MODE_PRODUCTION:
        /* Arena pre-alloc for the initial block */
        ig_arena_alloc_arrays(state, IG_POOL_SIZE);
        break;
    }

    /* Spawn 100 initial entities */
    int initial = 100;
    if (initial > state->ig_capacity) initial = state->ig_capacity;
    for (int i = 0; i < initial; i++) {
        ig_spawn_entity(state, i);
    }
}

static void scene14_init(game_state *state)
{
    /* Allocate arena backing memory once (persists across mode switches) */
    if (!g_ig_arena_mem) {
        g_ig_arena_mem = malloc(IG_ARENA_SIZE);
        g_ig_arena_cap = IG_ARENA_SIZE;
    }
    ig_arena_reset();

    /* Init scene-specific state */
    state->rng_state          = 77777;
    state->ig_mode            = IG_MODE_MALLOC;
    state->ig_total_created   = 0;
    state->ig_active_count    = 0;
    state->ig_peak_active     = 0;
    state->ig_destroyed_count = 0;
    state->ig_spawn_rate      = 100.0f;  /* entities/sec — fast, shows growth */
    state->ig_spawn_accum     = 0.0f;
    state->ig_update_time_us  = 0.0f;
    memset(state->ig_frame_times, 0, sizeof(state->ig_frame_times));
    state->ig_frame_idx       = 0;
    state->ig_mode_switches   = 0;
    state->ig_sim_radius      = 200.0f;
    state->ig_budget_max      = 500;

    /* Set up arrays for initial mode (MALLOC) */
    scene14_init_mode(state);
}

static void scene14_update(game_state *state, const game_input *input, float dt)
{
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /* Tab toggles mode */
    if (input->tab_pressed) {
        state->ig_mode = (state->ig_mode + 1) % IG_MODE_COUNT;
        state->ig_mode_switches++;
        scene14_init_mode(state);
    }

    /* ── Spawn loop ── */
    bool uses_arena = (state->ig_mode == IG_MODE_ARENA || state->ig_mode == IG_MODE_PRODUCTION);

    state->ig_spawn_accum += state->ig_spawn_rate * dt;
    while (state->ig_spawn_accum >= 1.0f) {
        state->ig_spawn_accum -= 1.0f;

        /* Budget mode: don't spawn if at cap */
        if (state->ig_mode == IG_MODE_BUDGET || state->ig_mode == IG_MODE_PRODUCTION) {
            if (state->ig_active_count >= state->ig_budget_max) break;
        }

        int slot = ig_find_inactive(state);
        if (slot < 0) {
            if (state->ig_mode == IG_MODE_MALLOC) {
                /* THE KEY MOMENT: double the arrays via realloc.
                 * 256 -> 512 -> 1024 -> 2048 -> 4096 -> ...
                 * Each realloc gets more expensive. Eventually the
                 * update loop itself becomes the bottleneck. */
                ig_grow(state, state->ig_capacity * 2);
                slot = ig_find_inactive(state);
            } else if (uses_arena) {
                /* Arena/production: can't grow, stop spawning */
                break;
            } else {
                break; /* pool modes: cap reached, stop spawning */
            }
        }
        if (slot < 0) break; /* safety */

        ig_spawn_entity(state, slot);
    }

    /* ── Mode-specific behavior ── */
    bool custom_move = false;

    switch (state->ig_mode) {
    case IG_MODE_MALLOC:
        /* Do NOTHING -- entities never die, arrays grow forever.
         * FPS WILL degrade as entity count climbs into the thousands.
         * This is the intentional failure case the student must observe. */
        break;

    case IG_MODE_ARENA:
        /* Arena pre-alloc: entities have lifetimes, expired ones are
         * marked inactive (arena can't free individually). Slots reused
         * by new spawns via ig_find_inactive. */
        for (int i = 0; i < state->ig_capacity; i++) {
            if (!state->ig_active[i]) continue;
            state->ig_lifetime[i] -= dt;
            if (state->ig_lifetime[i] <= 0.0f) {
                state->ig_active[i] = 0;
                state->ig_destroyed_count++;
            }
        }
        break;

    case IG_MODE_POOL:
        /* Fixed capacity (IG_POOL_SIZE). Entities expire, active flag
         * cleared. New entities find inactive slots via linear scan.
         * Never grows. Zero malloc in game loop. */
        for (int i = 0; i < state->ig_capacity; i++) {
            if (!state->ig_active[i]) continue;
            state->ig_lifetime[i] -= dt;
            if (state->ig_lifetime[i] <= 0.0f) {
                state->ig_active[i] = 0;
                state->ig_destroyed_count++;
            }
        }
        break;

    case IG_MODE_LIFETIME:
        /* Same as pool but emphasizes the TTL mechanism. Decrement
         * lifetime, remove expired. Steady-state count. */
        for (int i = 0; i < state->ig_capacity; i++) {
            if (!state->ig_active[i]) continue;
            state->ig_lifetime[i] -= dt;
            if (state->ig_lifetime[i] <= 0.0f) {
                state->ig_active[i] = 0;
                state->ig_destroyed_count++;
            }
        }
        break;

    case IG_MODE_BUDGET:
        /* Hard cap (ig_budget_max). If active > budget, remove oldest
         * (lowest active indices) until under. Spawner pauses at cap. */
        {
            /* First: expire old entities */
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                state->ig_lifetime[i] -= dt;
                if (state->ig_lifetime[i] <= 0.0f) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                }
            }
            /* Then: enforce budget cap */
            int active = 0;
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                active++;
                if (active > state->ig_budget_max) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                }
            }
        }
        break;

    case IG_MODE_STREAMING:
        /* Pool mode + only simulate entities within ig_sim_radius of
         * screen center. Far entities deactivated. */
        {
            float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;
            float r2 = state->ig_sim_radius * state->ig_sim_radius;
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                state->ig_lifetime[i] -= dt;
                if (state->ig_lifetime[i] <= 0.0f) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                    continue;
                }
                float edx = state->ig_x[i] - cx;
                float edy = state->ig_y[i] - cy;
                if (edx * edx + edy * edy > r2) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                }
            }
        }
        break;

    case IG_MODE_LOD:
        /* Pool mode + near entities: full update. Far: update every
         * 4th frame. Lifetime decay on all. */
        {
            float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                float edx = state->ig_x[i] - cx;
                float edy = state->ig_y[i] - cy;
                float dist2 = edx * edx + edy * edy;
                bool is_near = dist2 < 150.0f * 150.0f;
                if (is_near || (i % 4 == state->ig_frame_idx % 4)) {
                    state->ig_x[i] += state->ig_dx[i] * dt;
                    state->ig_y[i] += state->ig_dy[i] * dt;
                    if (state->ig_x[i] < 0 || state->ig_x[i] > SCREEN_W) state->ig_dx[i] = -state->ig_dx[i];
                    if (state->ig_y[i] < 0 || state->ig_y[i] > SCREEN_H) state->ig_dy[i] = -state->ig_dy[i];
                }
                /* Expire old ones */
                state->ig_lifetime[i] -= dt;
                if (state->ig_lifetime[i] <= 0.0f) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                }
            }
        }
        custom_move = true;
        break;

    case IG_MODE_AMORTIZED:
        /* Pool mode + only update 1/4 of entities per frame (staggered
         * by index % 4 == frame % 4). Compensate with 4x dt. */
        {
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                state->ig_lifetime[i] -= dt;
                if (state->ig_lifetime[i] <= 0.0f) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                    continue;
                }
                if (i % 4 == state->ig_frame_idx % 4) {
                    state->ig_x[i] += state->ig_dx[i] * dt * 4.0f; /* compensate 1-in-4 */
                    state->ig_y[i] += state->ig_dy[i] * dt * 4.0f;
                    if (state->ig_x[i] < 0 || state->ig_x[i] > SCREEN_W) state->ig_dx[i] = -state->ig_dx[i];
                    if (state->ig_y[i] < 0 || state->ig_y[i] > SCREEN_H) state->ig_dy[i] = -state->ig_dy[i];
                }
            }
        }
        custom_move = true;
        break;

    case IG_MODE_THREADED:
        /* Pool mode + split the entity array into 2 halves. Update each
         * half on a separate pthread. Join both. Main thread renders. */
        {
            /* Expire lifetimes first (single-threaded for destroyed_count) */
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                state->ig_lifetime[i] -= dt;
                if (state->ig_lifetime[i] <= 0.0f) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                }
            }
            /* Split movement across 2 threads */
            int mid = state->ig_capacity / 2;
            ig_thread_work w1 = {state, 0, mid, dt};
            ig_thread_work w2 = {state, mid, state->ig_capacity, dt};
            pthread_t t1;
            pthread_create(&t1, NULL, ig_thread_update, &w1);
            ig_thread_update(&w2); /* main thread does second half */
            pthread_join(t1, NULL);
        }
        custom_move = true;
        break;

    case IG_MODE_PRODUCTION:
        /* Arena pre-alloc + pool recycling + lifetime TTL + budget cap
         * + amortized (1/4 per frame). Everything combined. */
        {
            /* Lifetime decay on all */
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                state->ig_lifetime[i] -= dt;
                if (state->ig_lifetime[i] <= 0.0f) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                }
            }
            /* Enforce budget cap */
            int active = 0;
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                active++;
                if (active > state->ig_budget_max) {
                    state->ig_active[i] = 0;
                    state->ig_destroyed_count++;
                }
            }
            /* Amortized movement: only 1/4 per frame */
            for (int i = 0; i < state->ig_capacity; i++) {
                if (!state->ig_active[i]) continue;
                if (i % 4 == state->ig_frame_idx % 4) {
                    state->ig_x[i] += state->ig_dx[i] * dt * 4.0f;
                    state->ig_y[i] += state->ig_dy[i] * dt * 4.0f;
                    if (state->ig_x[i] < 0 || state->ig_x[i] > SCREEN_W) state->ig_dx[i] = -state->ig_dx[i];
                    if (state->ig_y[i] < 0 || state->ig_y[i] > SCREEN_H) state->ig_dy[i] = -state->ig_dy[i];
                }
            }
        }
        custom_move = true;
        break;
    }

    /* Default bounce movement for modes that don't handle it above */
    if (!custom_move) {
        for (int i = 0; i < state->ig_capacity; i++) {
            if (!state->ig_active[i]) continue;
            state->ig_x[i] += state->ig_dx[i] * dt;
            state->ig_y[i] += state->ig_dy[i] * dt;
            if (state->ig_x[i] < 0 || state->ig_x[i] > SCREEN_W) state->ig_dx[i] = -state->ig_dx[i];
            if (state->ig_y[i] < 0 || state->ig_y[i] > SCREEN_H) state->ig_dy[i] = -state->ig_dy[i];
        }
    }

    /* Count active entities */
    state->ig_active_count = 0;
    for (int i = 0; i < state->ig_capacity; i++) {
        if (state->ig_active[i]) state->ig_active_count++;
    }
    if (state->ig_active_count > state->ig_peak_active) {
        state->ig_peak_active = state->ig_active_count;
    }

    /* Frame time history */
    state->ig_frame_times[state->ig_frame_idx % 120] = dt * 1000.0f;
    state->ig_frame_idx++;

    /* Measure update timing */
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    {
        long sec_diff  = t_end.tv_sec  - t_start.tv_sec;
        long nsec_diff = t_end.tv_nsec - t_start.tv_nsec;
        float elapsed_us = (float)(sec_diff * 1000000L + nsec_diff / 1000L);
        state->ig_update_time_us = state->ig_update_time_us * 0.9f + elapsed_us * 0.1f;
    }
}

/* ── Scene 14 render ── */

static void scene14_render(const game_state *state, uint32_t *pixels, int width, int height)
{
    if (!pixels) return;

    float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;
    float near_r2 = 150.0f * 150.0f;

    /* Draw all active entities. For large counts (10K+) this loop itself
     * becomes a bottleneck — that IS part of the lesson. */
    for (int i = 0; i < state->ig_capacity; i++) {
        if (!state->ig_active[i]) continue;

        uint32_t color = state->ig_color[i];

        if (state->ig_mode == IG_MODE_LOD) {
            /* LOD mode: dim far entities */
            float edx = state->ig_x[i] - cx;
            float edy = state->ig_y[i] - cy;
            if (edx * edx + edy * edy > near_r2) {
                uint32_t r = (color >> 16) & 0xFF;
                uint32_t g = (color >> 8)  & 0xFF;
                uint32_t b = (color)       & 0xFF;
                color = 0xFF000000 | ((r / 3) << 16) | ((g / 3) << 8) | (b / 3);
            }
        }

        draw_rect(pixels, width, height,
                  (int)state->ig_x[i], (int)state->ig_y[i], 4, 4, color);
    }

    /* STREAMING mode: draw sim_radius circle */
    if (state->ig_mode == IG_MODE_STREAMING) {
        draw_circle_outline(pixels, width, height,
                            SCREEN_W / 2, SCREEN_H / 2,
                            (int)state->ig_sim_radius,
                            0xFF44FF44);
    }

    /* LOD mode: draw near-radius circle */
    if (state->ig_mode == IG_MODE_LOD) {
        draw_circle_outline(pixels, width, height,
                            SCREEN_W / 2, SCREEN_H / 2,
                            150, 0xFF4488FF);
    }

    /* THREADED mode: draw dividing line at midpoint */
    if (state->ig_mode == IG_MODE_THREADED) {
        int mid_y = height / 2;
        for (int x = 0; x < width; x += 4) {
            if (mid_y >= 0 && mid_y < height)
                pixels[mid_y * width + x] = 0xFF888888;
        }
    }
}

/* ── Scene 14 HUD ── */

static void scene14_hud(const game_state *state, char *hud, int hud_size)
{
    int cap_pct = (state->ig_capacity > 0)
        ? (state->ig_active_count * 100 / state->ig_capacity) : 0;
    int mem_kb = state->ig_bytes_allocated / 1024;
    int stage = ig_mode_stage(state->ig_mode);

    const char *status = "";
    if (state->ig_mode == IG_MODE_MALLOC && state->ig_active_count > 5000)
        status = " ** DEGRADING **";

    /* Mode-specific extra info */
    char extra[64] = "";
    switch (state->ig_mode) {
    case IG_MODE_BUDGET:
    case IG_MODE_PRODUCTION:
        snprintf(extra, sizeof(extra), " | Budget: %d", state->ig_budget_max);
        break;
    case IG_MODE_STREAMING:
        snprintf(extra, sizeof(extra), " | Radius: %.0f", state->ig_sim_radius);
        break;
    case IG_MODE_THREADED:
        snprintf(extra, sizeof(extra), " | Threads: 2");
        break;
    default:
        break;
    }

    snprintf(hud, hud_size,
             "Infinite Growth Lab  [I] Enter  [Tab] Toggle Mode  [R] Reset\n"
             "%s (Stage %d) | Active: %d/%d (%d%%) | Peak: %d | Mem: %dKB | "
             "Created: %d | Destroyed: %d | %.0fus%s%s",
             ig_mode_name(state->ig_mode), stage,
             state->ig_active_count, state->ig_capacity, cap_pct,
             state->ig_peak_active, mem_kb,
             state->ig_total_created, state->ig_destroyed_count,
             state->ig_update_time_us, extra, status);
}

/* ══════════════════════════════════════════════════════ */
/*           Scene dispatch + Game API                     */
/* ══════════════════════════════════════════════════════ */

static void scene_init_dispatch(game_state *state)
{
    switch (state->current_scene) {
        case 1: scene1_init(state); break;
        case 2: scene2_init(state); break;
        case 3: scene3_init(state); break;
        case 4: scene4_init(state); break;
        case 5: scene5_init(state); break;
        case 6: scene6_init(state); break;
        case 7: scene7_init(state); break;
        case 8: scene8_init(state); break;
        case 9: scene9_init(state); break;
        case 10: scene10_init(state); break;
        case 11: scene11_init(state); break;
        case 12: scene12_init(state); break;
        case 13: scene13_init(state); break;
        case 14: scene14_init(state); break;
        default: scene8_init(state); break;
    }
}

void game_init(game_state *state)
{
    int saved_scene = state->current_scene;
    /* Free scene 14 dynamic arrays before memset zeroes the pointers */
    ig_free(state);
    memset(state, 0, sizeof(*state));
    state->current_scene = (saved_scene >= 1 && saved_scene <= NUM_SCENES) ? saved_scene : 8;
    scene_init_dispatch(state);
}

void game_update(game_state *state, const game_input *input, float dt)
{
    /* Check for scene switch */
    if (input->scene_key >= 1 && input->scene_key <= NUM_SCENES) {
        state->current_scene = input->scene_key;
        /* Free scene 14 dynamic arrays before memset zeroes the pointers */
        ig_free(state);
        /* Reinitialize for the new scene */
        int scene = state->current_scene;
        memset(state, 0, sizeof(*state));
        state->current_scene = scene;
        scene_init_dispatch(state);
        return; /* skip update on the switch frame */
    }

    /* Dispatch to scene-specific update */
    switch (state->current_scene) {
        case 1: scene1_update(state, input, dt); break;
        case 2: scene2_update(state, input, dt); break;
        case 3: scene3_update(state, input, dt); break;
        case 4: scene4_update(state, input, dt); break;
        case 5: scene5_update(state, input, dt); break;
        case 6: scene6_update(state, input, dt); break;
        case 7: scene7_update(state, input, dt); break;
        case 8: scene8_update(state, input, dt); break;
        case 9: scene9_update(state, input, dt); break;
        case 10: scene10_update(state, input, dt); break;
        case 11: scene11_update(state, input, dt); break;
        case 12: scene12_update(state, input, dt); break;
        case 13: scene13_update(state, input, dt); break;
        case 14: scene14_update(state, input, dt); break;
        default: scene8_update(state, input, dt); break;
    }
}

void game_render(const game_state *state, uint32_t *pixels, int width, int height)
{
    /* Clear to dark background */
    uint32_t bg = 0xFF1A1A2E;
    for (int i = 0; i < width * height; i++) {
        pixels[i] = bg;
    }

    /* Scene-specific render (HUD text is handled separately by game_hud) */
    char dummy[4] = "";
    switch (state->current_scene) {
        case 1: scene1_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 2: scene2_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 3: scene3_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 4: scene4_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 5: scene5_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 6: scene6_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 7: scene7_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 8: scene8_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 9: scene9_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
        case 10: scene10_render(state, pixels, width, height); break;
        case 11: scene11_render(state, pixels, width, height); break;
        case 12: scene12_render(state, pixels, width, height); break;
        case 13: scene13_render(state, pixels, width, height); break;
        case 14: scene14_render(state, pixels, width, height); break;
        default: scene8_render(state, pixels, width, height, dummy, sizeof(dummy)); break;
    }

    /* ── Draw HUD text onto pixel buffer ── */
    char hud_text[512];
    game_hud(state, hud_text, sizeof(hud_text));

    /* Draw ALL lines — split on every \n, 10px per line */
    {
        int y = 4;
        char *line = hud_text;
        while (*line) {
            char *end = line;
            while (*end && *end != '\n') end++;
            char saved = *end;
            *end = '\0';
            uint32_t color = (y == 4) ? 0xFFFFFFFF : 0xFFCCCCCC;
            draw_text_buf(pixels, width, height, 4, y, line, color);
            *end = saved;
            if (saved == '\n') end++;
            line = end;
            y += 10;
        }
    }
}

void game_hud(const game_state *state, char *buf, int buf_size)
{
    /* Scene-specific stats line */
    char stats[256] = "";

    switch (state->current_scene) {
        case 1: scene1_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 2: scene2_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 3: scene3_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 4: scene4_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 5: scene5_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 6: scene6_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 7: scene7_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 8: scene8_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 9: scene9_render(state, NULL, 0, 0, stats, sizeof(stats)); break;
        case 10: scene10_hud(state, stats, sizeof(stats)); break;
        case 11: scene11_hud(state, stats, sizeof(stats)); break;
        case 12: scene12_hud(state, stats, sizeof(stats)); break;
        case 13: scene13_hud(state, stats, sizeof(stats)); break;
        case 14: scene14_hud(state, stats, sizeof(stats)); break;
        default: snprintf(stats, sizeof(stats), "Unknown scene"); break;
    }

    /* All scenes and labs are always available */
    const char *keys = "[1-9]Scene [P]Part [L]Layout [G]Spatial [M]Memory [I]Growth [Tab]Mode [R]Reset";
    snprintf(buf, buf_size,
             "Scene %d: %s  %s\n%s",
             state->current_scene, scene_name(state->current_scene), keys, stats);
}
