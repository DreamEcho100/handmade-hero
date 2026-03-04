# Lesson 12 — Polish and Utils Refactor

## What We're Building

Complete the final rendering layer — header panel, score text, game-over overlay — and extract reusable drawing code into `utils/` so `game.c` only contains game-specific logic.

---

## The Concept

### JS Analogy: Canvas Draw Order + Module Extraction Refactor

**Painter's algorithm:** In a `<canvas>` context, whatever you draw last appears on top. `snake_render` uses exactly this principle:

```js
// JS canvas equivalent of snake_render layer order:
ctx.fillRect(0, 0, width, height);           // 1. background (bottom)
drawHeaderBackground();                       // 2. header panel
drawHeaderText();                             // 3. header text
drawWalls();                                  // 4. walls
drawFood();                                   // 5. food
drawSnakeBody();                              // 6. body
drawSnakeHead();                              // 7. head (on top of body)
if (gameOver) drawGameOverOverlay();          // 8. overlay (topmost)
```

**Module extraction:** The `utils/` folder is a *refactor* of code that started life inline in `game.c`. In JavaScript terms, this is the same as:

```
// Before: everything in game.js
function drawRect(ctx, x, y, w, h, color) { ... }
function drawText(ctx, x, y, str, color) { ... }
function updateSnake(state, input, dt) { ... }

// After: extracted to utils/
// utils/draw-shapes.js  → drawRect, drawRectBlend
// utils/draw-text.js    → drawChar, drawText
// game.js               → updateSnake (game-specific only)
```

The rule: if a function depends on game logic (e.g., `draw_cell` uses `HEADER_ROWS` and `CELL_SIZE`), it stays in `game.c`. If it is purely a drawing primitive (e.g., `draw_rect`), it moves to `utils/`.

---

### `snprintf` — Safe Formatted Strings

In C, `sprintf(buf, "SCORE:%d", score)` is dangerous — it will write past the end of `buf` if the formatted string is longer than the buffer. `snprintf` adds a maximum-length argument:

```c
snprintf(buf, sizeof(buf), "SCORE:%d", s->score);
//            ^^^^^^^^^^^^
//            Maximum bytes to write (including null terminator).
//            sizeof(buf) is computed at compile time — always correct.
```

JS analogy: `String(score)` — JavaScript strings are always bounds-safe. In C you must manage buffer size yourself. Always use `snprintf`, never `sprintf`.

---

## The Code

### Full Layer Order — File: `src/game.c`, `snake_render`

```c
void snake_render(const GameState *s, SnakeBackbuffer *bb) {
    char buf[64];
    int col, row, idx, rem;
    uint32_t body_color, head_color;

    /* ── 1. Clear to black — background ──────────────────────────────── */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* ── 2. Header background and separator ──────────────────────────── */
    draw_rect(bb, 0, 0, bb->width, HEADER_ROWS * CELL_SIZE, COLOR_DARK_GRAY);
    /* Separator line: 2px green line between header and play field */
    draw_rect(bb, 0, (HEADER_ROWS - 1) * CELL_SIZE, bb->width, 2, COLOR_GREEN);

    /* ── 3. Header text ───────────────────────────────────────────────── */
    {
        /* Centre "SNAKE" title at scale 2 */
        int title_chars = 5;  /* strlen("SNAKE") */
        int title_w     = title_chars * GLYPH_STRIDE * 2;
        int title_x     = (bb->width - title_w) / 2;
        int text_y      = (HEADER_ROWS * CELL_SIZE - GLYPH_HEIGHT * 2) / 2;

        draw_text(bb, title_x, text_y, "SNAKE", COLOR_GREEN, 2);

        /* Score on the left */
        snprintf(buf, sizeof(buf), "SCORE:%d", s->score);
        draw_text(bb, 8, text_y, buf, COLOR_WHITE, 2);

        /* Best score on the right — right-aligned */
        snprintf(buf, sizeof(buf), "BEST:%d", s->best_score);
        {
            int bw = (int)strlen(buf) * GLYPH_STRIDE * 2;
            draw_text(bb, bb->width - bw - 8, text_y, buf, COLOR_YELLOW, 2);
        }
    }

    /* ── 4. Border walls ──────────────────────────────────────────────── */
    for (row = 0; row < GRID_HEIGHT; row++) {
        draw_cell(bb, 0,            row, COLOR_GREEN);
        draw_cell(bb, GRID_WIDTH-1, row, COLOR_GREEN);
    }
    for (col = 0; col < GRID_WIDTH; col++) {
        draw_cell(bb, col, GRID_HEIGHT-1, COLOR_GREEN);
    }

    /* ── 5. Food ──────────────────────────────────────────────────────── */
    draw_cell(bb, s->food_x, s->food_y, COLOR_RED);

    /* ── 6 & 7. Snake body and head ───────────────────────────────────── */
    /* Dead snake: whole body turns dark red */
    body_color = (s->phase == GAME_PHASE_GAME_OVER) ? COLOR_DARK_RED : COLOR_YELLOW;
    head_color = (s->phase == GAME_PHASE_GAME_OVER) ? COLOR_DARK_RED : COLOR_WHITE;

    /* Body: tail..head-1 */
    idx = s->tail;
    rem = s->length - 1;
    while (rem > 0) {
        draw_cell(bb, s->segments[idx].x, s->segments[idx].y, body_color);
        idx = (idx + 1) % MAX_SNAKE;
        rem--;
    }
    /* Head: always last → always on top */
    draw_cell(bb, s->segments[s->head].x, s->segments[s->head].y, head_color);

    /* ── 8. Game-over overlay (conditional) ──────────────────────────── */
    if (s->phase == GAME_PHASE_GAME_OVER) {
        /* ... see Lesson 10 for full overlay code */
    }
}
```

---

### `GLYPH_STRIDE` for Text Centering — File: `src/utils/draw-text.h`

```c
/* GLYPH_STRIDE — horizontal advance per character at scale 1.
 * = 8 px glyph + 1 px gap between characters. */
#define GLYPH_STRIDE  9
```

**Centering formula:**

```c
/* Centred text at pixel coordinate (cx):
 *   text_pixel_width = strlen(str) * GLYPH_STRIDE * scale
 *   text_x = cx - text_pixel_width / 2
 *
 * Example: "GAME OVER" (9 chars), scale 3:
 *   text_w = 9 * 9 * 3 = 243 px
 *   text_x = 600 - 243/2 = 600 - 121 = 479
 *
 * JS equivalent: ctx.textAlign = 'center'; ctx.fillText(str, cx, y);
 * In C we compute it manually because draw_text() takes a left-edge x. */
int str_len = (int)strlen(str);
int str_w   = str_len * GLYPH_STRIDE * scale;
int text_x  = cx - str_w / 2;
draw_text(bb, text_x, y, str, color, scale);
```

---

### The `utils/` Refactor — What Lives Where

| Function | Location | Reason |
|---|---|---|
| `draw_rect` | `utils/draw-shapes.c` | Pure pixel primitive — no game knowledge |
| `draw_rect_blend` | `utils/draw-shapes.c` | Pure pixel primitive |
| `draw_char` | `utils/draw-text.c` | Pure font renderer |
| `draw_text` | `utils/draw-text.c` | Pure font renderer |
| `draw_cell` | `game.c` (static) | Knows `HEADER_ROWS`, `CELL_SIZE`, `CELL_SIZE-2` |

`draw_cell` is `static` (file-private) in `game.c` because it embeds game constants. No other file needs it:

```c
/* draw_cell: game-specific — knows HEADER_ROWS and CELL_SIZE.
 * Lives in game.c, NOT in utils/, because it is not reusable outside this game. */
static void draw_cell(SnakeBackbuffer *bb, int col, int row, uint32_t color) {
    int field_y_offset = HEADER_ROWS * CELL_SIZE;
    draw_rect(bb,
              col  * CELL_SIZE + 1,
              field_y_offset + row * CELL_SIZE + 1,
              CELL_SIZE - 2,
              CELL_SIZE - 2,
              color);
}
```

---

### Course Source vs Reference Source — 10 Upgrades Summary

This course source (`src/`) differs from the javidx9 reference in these ways:

| # | Area | Reference source | Course source |
|---|---|---|---|
| 1 | Audio | None | Full procedural SFX engine (`audio.c`, `utils/audio.h`) |
| 2 | Game phase | `int game_over` boolean | `GAME_PHASE` typed enum with switch dispatch |
| 3 | Input | Single-buffer, transitions only | Double-buffered: preserves `ended_down` across frames |
| 4 | Debug assertions | None | `ASSERT(expr)` / `DEBUG_TRAP()` macros in `game.h` |
| 5 | Drawing utilities | Inline in game file | Extracted to `utils/draw-shapes.c`, `utils/draw-text.c` |
| 6 | Font | Separate digit/letter/special tables + linear scan | Single `FONT_8X8[128]` indexed by ASCII value |
| 7 | Text centering | Manual fixed offsets | `GLYPH_STRIDE` macro + `strlen` formula |
| 8 | Score display | Single score | `score` + `best_score` with colour-coded header text |
| 9 | Game-over overlay | Simple text | Semi-transparent blend + red border + hints |
| 10 | Platform targets | Raylib only | X11 native backend (`main_x11.c`) + Raylib (`main_raylib.c`) |

The lesson series (01–12) builds the reference feature set, then layers in the course upgrades at the point where they make the most didactic sense.

---

## What To Notice

- **Painter's algorithm is enforced by code order, not data.** There is no Z-buffer. The overlay draws last and wins simply because it is the last call in `snake_render`. Reorder the calls and the visual layering changes.
- **`draw_cell` is `static`.** The `static` keyword at file scope in C means "this function is private to this translation unit". JS equivalent: an unexported function in a module. Without `static`, it would be visible to every `.c` file in the project.
- **`snprintf` takes `sizeof(buf)`, not `sizeof(buf) - 1`.** `snprintf` writes at most `n` bytes *including* the null terminator. `sizeof(buf)` is correct. Using `sizeof(buf) - 1` would waste one byte.
- **The utils/ split is a refactor, not initial design.** In a real course, you would write `draw_rect` inline in `game.c` in lesson 02, then move it to `utils/` in lesson 12 as the project grows. That movement is the lesson: refactoring is a normal part of software development, not a sign of initial failure.

---

## Exercises

1. **Add a "HIGH SCORE!" flash.** When `s->score > s->best_score` at the moment of death, render an additional line in the overlay: "NEW BEST!" in `COLOR_YELLOW` at scale 3. Use `snprintf` with `s->score` to fill a buffer.

2. **Right-align the BEST score using `GLYPH_STRIDE`.** Confirm the current formula by adding 1 to the scale and checking that the text is still flush to the right edge of the window.

3. **Extract `draw_cell` to a separate utils file.** Create `utils/draw-grid.h` and `utils/draw-grid.c`. Move `draw_cell` there. What parameters must it receive that it currently captures from `game.c` constants? How does this change the function signature?
