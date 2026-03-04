#include "draw-shapes.h"
#include "backbuffer.h"
#include "math.h"

/* =============================================================================
 * draw-shapes.c — Rectangle Drawing Implementation
 * =============================================================================
 *
 * This file implements the two rectangle-drawing functions declared in
 * draw-shapes.h. Both follow the same pattern:
 *   1. Clip the requested rectangle to the backbuffer bounds.
 *   2. Loop over each row, then each column within that row.
 *   3. Write the color value (or blended color) into the pixel array.
 * =============================================================================
 */

/* ══════ draw_rect ══════ */

void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {

  /* ── Step 1: Clipping ───────────────────────────────────────────────────
   *
   * WHY DO WE CLIP?
   *   If we tried to write to pixels outside the array (e.g. x = -5 or
   *   x + w = 900 when width = 800) we would be writing to memory that
   *   doesn't belong to our pixel array. In the best case this corrupts some
   *   other variable; in the worst case the OS kills the program with a
   *   segmentation fault (SIGSEGV). C does NOT do automatic bounds checking —
   *   you are responsible for staying within the array.
   *
   *   JS equivalent: the Canvas 2D API clips silently for you. In C we must
   *   do it ourselves.
   *
   * HOW CLIPPING WORKS:
   *   We compute the clamped start (x0, y0) and exclusive end (x1, y1).
   *   MAX(x, 0)           → if x is negative, start drawing from column 0
   *   MIN(x + w, bb->width) → if the rect extends past the right edge, stop
   *                           at the right edge of the backbuffer
   *
   * JS equivalent:
   *   const x0 = Math.max(x, 0);
   *   const y0 = Math.max(y, 0);
   *   const x1 = Math.min(x + w, bb.width);
   *   const y1 = Math.min(y + h, bb.height);
   */
  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, bb->width);  /* exclusive end column */
  int y1 = MIN(y + h, bb->height); /* exclusive end row    */

  /* ── Step 2: Pixel Loop ─────────────────────────────────────────────────
   *
   * We loop row-by-row (outer loop = py, i.e. "pixel y").
   */
  for (int py = y0; py < y1; py++) {

    /* Compute a pointer to the first pixel of this row.
     *
     * JS: const rowStart = py * (bb.pitch / 4);
     * C:  uint32_t *row = bb->pixels + py * (bb->pitch / 4);
     *
     * bb->pixels is a uint32_t* — a pointer to the first pixel.
     * Adding an integer N to a pointer advances it by N * sizeof(uint32_t)
     * bytes (pointer arithmetic in C always uses the element size).
     *
     * WHY pitch / 4?
     *   `pitch` is in BYTES (e.g. 3200 for a 800-pixel-wide 4-bpp buffer).
     *   `bb->pixels` is a uint32_t pointer, so each step advances 4 bytes.
     *   To move `py` rows forward we need `py * (pitch_in_bytes / 4)` steps.
     *   If pitch = width * 4 (no padding), this simplifies to py * width.
     *   Using pitch / 4 ensures correctness even when there IS row padding.
     */
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);

    /* Inner loop: iterate over each column in the clipped range. */
    for (int px = x0; px < x1; px++) {
      /* Write the color directly. row[px] is pointer arithmetic:
       * row[px] == *(row + px) — the pixel at column px in this row.
       *
       * JS equivalent: pixels[py * width + px] = color;
       */
      row[px] = color;
    }
  }
}

/* ══════ draw_rect_blend ══════ */

void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h,
                     uint32_t color) {

  /* ── Step 1: Extract Alpha ──────────────────────────────────────────────
   *
   * The alpha value is stored in bits 31-24 of the packed color.
   * We extract it by:
   *   1. Shifting the 32-bit value RIGHT by 24 bits so alpha moves to bits 7-0.
   *   2. Masking with 0xFF (binary: 11111111) to keep only the lowest 8 bits,
   *      discarding any upper bits that might remain after the shift.
   *
   * JS equivalent:
   *   const alpha = (color >>> 24) & 0xFF;  // >>> is unsigned right-shift in JS
   *
   * C equivalent:
   *   uint8_t alpha = (color >> 24) & 0xFF;
   *
   * uint8_t ensures alpha fits in one byte (0–255).
   */
  uint8_t alpha = (color >> 24) & 0xFF;

  /* Optimisation: if fully opaque, skip blending — just overwrite the pixel. */
  if (alpha == 255) {
    draw_rect(bb, x, y, w, h, color);
    return;
  }

  /* Optimisation: if fully transparent, nothing to draw. */
  if (alpha == 0)
    return;

  /* ── Step 2: Extract Source RGB ─────────────────────────────────────────
   *
   * Same shift-and-mask pattern for red, green, blue.
   *
   * JS: const src_r = (color >>> 16) & 0xFF;
   * JS: const src_g = (color >>>  8) & 0xFF;
   * JS: const src_b =  color         & 0xFF;
   */
  uint8_t src_r = (color >> 16) & 0xFF; /* bits 23-16 */
  uint8_t src_g = (color >>  8) & 0xFF; /* bits 15-8  */
  uint8_t src_b =  color        & 0xFF; /* bits  7-0  */

  /* ── Step 3: Clip ───────────────────────────────────────────────────────
   * Same clipping logic as draw_rect — see comments there. */
  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, bb->width);
  int y1 = MIN(y + h, bb->height);

  /* ── Step 4: Blend Loop ─────────────────────────────────────────────────
   *
   * For each pixel: read the existing "destination" color, blend it with the
   * "source" color using the alpha value, write the result back.
   */
  for (int py = y0; py < y1; py++) {
    /* Row pointer — same trick as draw_rect (pitch / 4 for uint32_t steps). */
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);

    for (int px = x0; px < x1; px++) {

      /* Read the existing destination pixel. */
      uint32_t dst = row[px];

      /* Extract destination RGB channels using the same shift-and-mask. */
      uint8_t dst_r = (dst >> 16) & 0xFF;
      uint8_t dst_g = (dst >>  8) & 0xFF;
      uint8_t dst_b =  dst        & 0xFF;

      /* ── Alpha Blending Formula ────────────────────────────────────────
       *
       * Standard "source over" alpha composite (Porter-Duff SRC_OVER):
       *
       *   out = (src * alpha + dst * (255 - alpha)) / 255
       *
       * HOW IT WORKS:
       *   - alpha = 255  (fully opaque): out = (src*255 + dst*0) / 255 = src
       *   - alpha = 0    (transparent):  out = (src*0 + dst*255) / 255 = dst
       *   - alpha = 128  (50% blend):    out = (src*128 + dst*127) / 255
       *     → roughly 50% source + 50% destination
       *   - alpha = 64   (75% transparent):
       *     out = (src*64 + dst*191) / 255 → ~25% src, ~75% dst
       *
       * WHY DIVIDE BY 255 (not 256)?
       *   We work in the 0-255 range. Dividing by 255 keeps the result in
       *   0-255. If we divided by 256 (bit-shift >>8) we'd slightly darken
       *   fully opaque colors (255*255/256 = 254 instead of 255).
       *
       * JS equivalent:
       *   const outR = Math.floor((srcR * alpha + dstR * (255 - alpha)) / 255);
       *
       * NOTE: The intermediate products (e.g. src_r * alpha) can be up to
       * 255 * 255 = 65025, which fits in a uint32_t (max ~4.3 billion) but
       * NOT in a uint8_t. C automatically promotes uint8_t operands to int in
       * arithmetic expressions — this is safe here but worth knowing.
       */
      uint8_t out_r = (uint8_t)((src_r * alpha + dst_r * (255 - alpha)) / 255);
      uint8_t out_g = (uint8_t)((src_g * alpha + dst_g * (255 - alpha)) / 255);
      uint8_t out_b = (uint8_t)((src_b * alpha + dst_b * (255 - alpha)) / 255);

      /* Pack the blended channels back into a uint32 and write to the buffer.
       * Alpha is forced to 255 (fully opaque) since we've already applied the
       * transparency — the resulting pixel in the backbuffer is always opaque. */
      row[px] = GAME_RGB(out_r, out_g, out_b);
    }
  }
}
