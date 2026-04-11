# Lesson 13 — HUD Rendering

## What you'll learn
- Two RenderContexts: world vs HUD
- `HUD_TOP_Y` for top-anchored elements
- Score and best-score display with `TextCursor`
- Game-over screen and centred text

---

## Two RenderContexts recap

```c
void asteroids_render(const GameState *state, Backbuffer *bb,
                      GameWorldConfig world_config)
{
    // World context: camera applied (for future camera support)
    RenderContext world_ctx = make_render_context(bb, world_config);

    // HUD context: strip camera so overlay stays screen-fixed
    GameWorldConfig hud_cfg = world_config;
    hud_cfg.camera_x    = 0.0f;
    hud_cfg.camera_y    = 0.0f;
    hud_cfg.camera_zoom = 1.0f;
    RenderContext hud_ctx = make_render_context(bb, hud_cfg);

    // ...
}
```

If you ever add camera zoom (zoom into an asteroid explosion), the HUD
stays at the same size because `hud_ctx` always has zoom=1.

---

## HUD_TOP_Y

```c
// utils/base.h
#define HUD_TOP_Y(offset_from_top) ((float)GAME_UNITS_H - (float)(offset_from_top))
```

In y-up world space:
- `y = 0`             → bottom of screen
- `y = GAME_UNITS_H`  → top of screen
- `HUD_TOP_Y(0.25)`   → `15 - 0.25 = 14.75` → 0.25 units below the top edge

`make_cursor(&hud_ctx, wx, wy)` converts this world coordinate to pixels.

---

## Score display

```c
char buf[32];
int sc = 2;  // font scale: 2 = 16×16px glyphs

// Left-aligned score
TextCursor score_cur = make_cursor(&hud_ctx, 0.25f, HUD_TOP_Y(0.25f));
snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
draw_text(bb, score_cur.x, score_cur.y, sc, buf, COLOR_WHITE);

// Right-aligned best score (right edge at GAME_UNITS_W - 0.25)
snprintf(buf, sizeof(buf), "BEST: %d", state->best_score);
int best_w = measure_text_px(buf, sc);
float best_px = world_x(&hud_ctx, GAME_UNITS_W - 0.25f) - (float)best_w;
float best_py = world_y(&hud_ctx, HUD_TOP_Y(0.25f));
draw_text(bb, best_px, best_py, sc, buf, COLOR_WHITE);
```

Right-aligned text: measure the string's pixel width, then subtract it
from the right-edge pixel position.

---

## Game-over screen

When `state->phase == PHASE_DEAD`:

1. Render world (asteroids still visible, no ship).
2. Dim the entire screen with a semi-transparent overlay.
3. Draw "GAME OVER" centred.
4. After `dead_timer ≤ 0`, draw "PRESS FIRE TO RESTART".

```c
if (state->phase == PHASE_DEAD) {
    // Dim overlay (50% alpha black)
    draw_rect(bb, 0.f, 0.f, (float)bb->width, (float)bb->height,
              0.f, 0.f, 0.f, 0.5f);

    int sc = 2;
    const char *msg1 = "GAME OVER";
    const char *msg2 = "PRESS FIRE TO RESTART";

    float cx = world_x(&hud_ctx, GAME_UNITS_W * 0.5f);  // screen centre x
    float cy = world_y(&hud_ctx, GAME_UNITS_H * 0.5f);  // screen centre y

    int w1 = measure_text_px(msg1, sc);
    draw_text(bb, cx - (float)(w1/2), cy - (float)(8*sc*2), sc,
              msg1, COLOR_RED);

    if (state->dead_timer <= 0.0f) {
        int w2 = measure_text_px(msg2, sc);
        draw_text(bb, cx - (float)(w2/2), cy + (float)(8*sc), sc,
                  msg2, COLOR_WHITE);
    }
}
```

`cy - (float)(8*sc*2)` positions "GAME OVER" above centre.
`cy + (float)(8*sc)` positions the restart prompt below centre.

---

## Screen clear

At the start of `asteroids_render()`, clear the backbuffer:

```c
// Clear to black — full buffer in pixel space
draw_rect(bb, 0.f, 0.f, (float)bb->width, (float)bb->height,
          0.f, 0.f, 0.f, 1.f);
```

Note: `bb->width` and `bb->height` are in pixels, not game units.
`draw_rect` takes pixel coordinates directly — no `world_x/y` conversion.

---

## Render order

```
1. Clear (black)
2. Asteroids
3. Bullets
4. Ship (if alive)
5. HUD: score / best score
6. Game-over overlay (if PHASE_DEAD)
```

Objects drawn later appear on top.  The dim overlay must be drawn after
all world objects so it covers them, but the "GAME OVER" text must be
drawn after the overlay so it's visible.

---

## Key takeaways

1. HUD context strips camera so it stays screen-fixed even with camera zoom.
2. `HUD_TOP_Y(offset)` = `GAME_UNITS_H - offset` — top-anchored in y-up space.
3. `make_cursor(&hud_ctx, wx, wy)` converts world coords to pixel cursor.
4. Right-align: `right_px - measure_text_px(text, scale)`.
5. Dim overlay: `draw_rect(bb, 0, 0, w, h, 0,0,0, 0.5f)` — 50% blend.
6. Render order matters: overlay dims world objects, HUD text appears above.
