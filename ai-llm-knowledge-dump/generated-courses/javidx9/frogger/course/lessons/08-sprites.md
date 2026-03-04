# Lesson 08: Sprites (SpriteBank + draw_sprite_partial)

## What we're building

Replace the coloured rectangle placeholders with pixel-art sprites loaded from
`.spr` binary files.  We introduce the `SpriteBank` fixed pool (no heap
allocation after `game_init`), the `.spr` file format, and `draw_sprite_partial`
— a UV-crop blit function that renders any rectangular region of a sprite sheet.

## What you'll learn

- Binary file reading with `fread` (no JSON, no libraries)
- `SpriteBank` fixed pool: all sprite data in arrays inside `GameState`
- `.spr` file format: `int32_t w, h; int16_t colors[w*h]; int16_t glyphs[w*h]`
- `glyph == 0x0020` (space) = transparent cell — skip in the blitter
- `draw_sprite_partial` UV-crop: source in CELL units, dest in pixels
- `CONSOLE_PALETTE[16][3]` — Windows console 4-bit color → `GAME_RGBA`

## Prerequisites

Lessons 01–07 complete and building.  Danger buffer and death detection working.

---

## Step 1 — The `.spr` file format

`.spr` files are raw binary structures written by the original OLC Pixel Game
Engine tool.  There is no header magic number or version field.

```
struct SprFile {
    int32_t  width;             // tile columns (e.g. 8 for an 8×8 sprite)
    int32_t  height;            // tile rows
    int16_t  colors[w * h];     // low nibble = FG color index (0-15)
    int16_t  glyphs[w * h];     // 0x2588 = solid block, 0x0020 = transparent
};
```

**JS analogy** — loading a `.spr` is like:

```js
const buf   = await fetch('frog.spr').then(r => r.arrayBuffer());
const view  = new DataView(buf);
const w     = view.getInt32(0, true);   // little-endian
const h     = view.getInt32(4, true);
const colors = new Int16Array(buf, 8,          w * h);
const glyphs  = new Int16Array(buf, 8 + w*h*2, w * h);
```

In C we use `fread()` — the binary format is identical:

```c
FILE *f = fopen(path, "rb");
int32_t w, h;
fread(&w, sizeof(w), 1, f);
fread(&h, sizeof(h), 1, f);
fread(&bank->colors[off], sizeof(int16_t), (size_t)(w * h), f);
fread(&bank->glyphs[off], sizeof(int16_t), (size_t)(w * h), f);
fclose(f);
```

> **Course Note:** The reference frogger source loads sprites the same way.
> The course version wraps the loading logic inside `game_init()` and stores
> all sprite data in the `SpriteBank` embedded in `GameState` — no dynamic
> allocation after startup.

---

## Step 2 — SpriteBank: no heap after init

```c
/* From game.h */
#define SPR_POOL_CELLS  (9 * 32 * 8)  /* generous pool — checked at load */

typedef struct {
    int16_t colors[SPR_POOL_CELLS];
    int16_t glyphs[SPR_POOL_CELLS];
    int     widths [NUM_SPRITES];
    int     heights[NUM_SPRITES];
    int     offsets[NUM_SPRITES];   /* start index in colors[]/glyphs[] */
} SpriteBank;
```

All sprite data lives on the stack (inside `GameState`).  No `malloc` / `free`
after `game_init()`.  This is idiomatic C for games: allocate everything
upfront, avoid fragmentation, make memory layout explicit.

**JS analogy:** pre-loading all images into a single `ArrayBuffer` and using
byte offsets to address each one — one allocation, no per-image fetch.

Loading loop in `game_init()`:

```c
static const struct { int id; const char *filename; } spr_files[] = {
    {SPR_FROG,     "frog.spr"},
    {SPR_WATER,    "water.spr"},
    {SPR_PAVEMENT, "pavement.spr"},
    {SPR_WALL,     "wall.spr"},
    {SPR_HOME,     "home.spr"},
    {SPR_LOG,      "log.spr"},
    {SPR_CAR1,     "car1.spr"},
    {SPR_CAR2,     "car2.spr"},
    {SPR_BUS,      "bus.spr"},
};

int pool_cursor = 0;
for (int i = 0; i < NUM_SPRITES; i++) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s",
             assets_dir, spr_files[i].filename);
    FILE *f = fopen(path, "rb");
    int32_t w = 0, h = 0;
    fread(&w, sizeof(w), 1, f);
    fread(&h, sizeof(h), 1, f);
    bank->widths [id] = (int)w;
    bank->heights[id] = (int)h;
    bank->offsets[id] = pool_cursor;
    fread(&bank->colors[pool_cursor], sizeof(int16_t), (size_t)(w*h), f);
    fread(&bank->glyphs[pool_cursor], sizeof(int16_t), (size_t)(w*h), f);
    pool_cursor += (int)(w * h);
    fclose(f);
}
```

---

## Step 3 — CONSOLE_PALETTE: 4-bit color → GAME_RGBA

The `.spr` format stores color as a 4-bit Windows console color index (0–15).
We convert these to `GAME_RGBA` using a static lookup table:

```c
/* From game.h — static const so each TU gets its own copy (no linker symbol) */
static const uint8_t CONSOLE_PALETTE[16][3] = {
    {  0,   0,   0},  /* 0  Black        */
    {  0,   0, 128},  /* 1  Dark Blue    */
    {  0, 128,   0},  /* 2  Dark Green   */
    /* ... 13 more entries ... */
    {255, 255, 255},  /* 15 White        */
};
```

Usage in the blitter:

```c
int color_idx = colors[src_i] & 0x0F;  /* low nibble = FG color */
uint8_t r = CONSOLE_PALETTE[color_idx][0];
uint8_t g = CONSOLE_PALETTE[color_idx][1];
uint8_t b = CONSOLE_PALETTE[color_idx][2];
uint32_t pixel = GAME_RGBA(r, g, b, 255);
```

> **Course Note:** `CONSOLE_PALETTE` is declared `static const` in `game.h`,
> which means every `.c` file that includes `game.h` gets its own copy.
> This is fine for a read-only table (no link-time collision), but wastes
> ~48 bytes per compilation unit.  A production codebase would declare it
> `extern` in the header and define it once in `game.c`.  The course uses
> `static const` for simplicity — documented in `COURSE-BUILDER-IMPROVEMENTS.md`.

---

## Step 4 — draw_sprite_partial: UV-crop blitter

`draw_sprite_partial` renders a rectangular sub-region (in CELL units) of a
sprite sheet into the pixel backbuffer at a destination position (in pixels).

```c
/* From utils/draw-shapes.h */
void draw_sprite_partial(
    Backbuffer      *bb,
    const int16_t   *colors,   /* sprite colors array     */
    const int16_t   *glyphs,   /* sprite glyphs array     */
    const uint8_t    palette[16][3],
    int              sheet_w,  /* sprite width in cells   */
    int src_x, int src_y,      /* top-left of crop region (cells) */
    int src_w, int src_h,      /* crop size (cells)               */
    int dest_px_x, int dest_px_y  /* dest top-left (pixels)       */
);
```

**Transparency:** cells with `glyph == 0x0020` (ASCII space) are transparent:

```c
for (int sy = 0; sy < src_h; sy++) {
    for (int sx = 0; sx < src_w; sx++) {
        int src_i = (src_y + sy) * sheet_w + (src_x + sx);
        if (glyphs[src_i] == 0x0020) continue;  /* transparent — skip */

        /* Expand one cell to CELL_PX × CELL_PX pixels */
        int color_idx = colors[src_i] & 0x0F;
        uint32_t px = GAME_RGBA(CONSOLE_PALETTE[color_idx][0],
                                CONSOLE_PALETTE[color_idx][1],
                                CONSOLE_PALETTE[color_idx][2], 255);
        for (int py = 0; py < CELL_PX; py++) {
            for (int px2 = 0; px2 < CELL_PX; px2++) {
                int dx = dest_px_x + sx * CELL_PX + px2;
                int dy = dest_px_y + sy * CELL_PX + py;
                if (dx >= 0 && dx < bb->width &&
                    dy >= 0 && dy < bb->height)
                    bb->pixels[dy * bb->width + dx] = px;
            }
        }
    }
}
```

> **New technique: UV-crop sprite rendering**
>
> **What:** `draw_sprite_partial` draws only a rectangular sub-region of a
> sprite sheet — like `ctx.drawImage(img, sx, sy, sw, sh, dx, dy, dw, dh)`.
> This allows a single large sprite sheet to contain multiple animation frames
> or tile variants without loading separate files.
>
> **Why:** Frogger's logs come in different sizes (2, 3, 4 tiles wide) but
> share the same `log.spr` texture.  We render the correct-width sub-crop
> depending on the pattern entry.
>
> **JS equivalent:**
> ```js
> ctx.drawImage(sprite, srcX*8, srcY*8, srcW*8, srcH*8,
>               dstX, dstY, dstW, dstH);
> ```
>
> **C idiom:** Source coordinates in CELL units; destination in pixels.
> Multiply source cell by `CELL_PX` (8) to get pixel offset within the sheet.
>
> **Gotcha:** Source clip not destination clip — the crop constrains which
> cells of the *source* we read, not where we clip on screen.  Add explicit
> bounds checks at the destination pixel level to avoid writing out of the
> backbuffer.

---

## Build and run

```bash
# X11 backend
./build-dev.sh --backend=x11 -r

# Raylib backend
./build-dev.sh --backend=raylib -r
```

---

## Expected result

All lanes and the frog render as pixel-art sprites from the `.spr` files.
The frog sprite has a transparent background (the black background shows
through rather than a white rectangle).

## Exercises

1. Add a `#ifdef DEBUG` print in the sprite loader showing `w, h, pool_cursor`
   for each sprite — verify you're loading the expected dimensions.
2. Change `glyph == 0x0020` to `glyph == 0x2588` — what breaks and why?
3. Try drawing the same sprite at two different `src_x` offsets in the same
   frame to see the UV-crop in action.

## What's next

Lesson 09 adds the five home cells, the win condition, and the game restart
flow — completing the core gameplay loop.
