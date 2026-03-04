# Lesson 14: Full Game Polish — Complete Render Pipeline and Phase Flow

## What we're building

Wire every entity and phase together into the complete game.  By the end of this
lesson the game looks and behaves exactly like the finished Sugar Sugar — all render
functions are connected, every keyboard shortcut works, the level-complete overlay
transitions correctly, and freeplay mode is available after the last level.

## What you'll learn

- The full `render_playing` draw order (background → obstacles → filters → portals → cups → lines → grains → emitters → HUD)
- `render_level_complete` — semi-transparent overlay on top of the playing frame
- `render_freeplay` — identical to playing but with a "FREEPLAY" banner
- Complete keyboard shortcuts: R = reset, G = gravity, Escape = back to title, Enter = advance level
- Freeplay mode — how `PHASE_FREEPLAY` differs from `PHASE_PLAYING`
- Level-complete auto-advance (2 s timeout)
- Cyclic levels — grains that wrap bottom→top

## Prerequisites

- Lesson 13 complete (font system and HUD text working)

---

## Step 1: `render_playing` — full draw order

Order matters: entities drawn later appear on top of earlier ones.

```c
/* src/game.c  —  render_playing (full function) */

static void render_playing(const GameState *state, GameBackbuffer *bb) {
  /* 1. Background — clear the whole canvas */
  draw_rect(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_BG);

  /* 2. Obstacles (rectangles from LevelDef, drawn into LineBitmap and then
   *    rendered as solid rects so the player can see the walls clearly) */
  render_obstacles(&state->level, bb, &state->lines);

  /* 3. Color filter zones — semi-transparent tinted rectangles.
   *    Draw BEFORE cups so cups appear on top of filter zones. */
  render_filters(&state->level, bb);

  /* 4. Teleporter portals — circles with a slight glow ring */
  render_teleporters(&state->level, bb, state->phase_timer);

  /* 5. Cups — fill bars, borders, percentage labels.
   *    Draw AFTER filters so cup borders show clearly against filter tint. */
  render_cups(&state->level, bb);

  /* 6. Player-drawn lines + baked obstacle pixels from LineBitmap.
   *    These are ON TOP of cups/obstacles so the player can always see their
   *    drawing. */
  render_lines(&state->lines, bb);

  /* 7. Sugar grains — drawn ABOVE everything else.
   *    A grain that lands in a cup has already been removed by this point, so
   *    it won't appear floating above a full cup. */
  render_grains(&state->grains, bb);

  /* 8. Emitter spout indicators — small triangles or chevrons showing where
   *    sugar spawns.  On top of lines so they're always visible. */
  render_emitters(&state->level, bb);

  /* 9. HUD — drawn last so buttons are always readable above all entities. */
  render_ui_buttons(state, bb);
}
```

---

## Step 2: `render_level_complete` — overlay

```c
/* src/game.c  —  render_level_complete */

static void render_level_complete(const GameState *state, GameBackbuffer *bb) {
  /* Show the last playing frame as the background. */
  render_playing(state, bb);

  /* Semi-transparent green wash over the whole canvas (alpha ≈ 80/255 ≈ 31%) */
  draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_COMPLETE, 80);

  /* "LEVEL COMPLETE!" at 2× scale.
   * Width = 15 chars × 9 px/char × 2 scale = 270 px. */
  int tx = (CANVAS_W - 15 * 9 * 2) / 2;
  int ty = CANVAS_H / 2 - 16;

  /* White shadow shifted 2 px right + 20 px down, then coloured text on top */
  draw_text_scaled(bb, tx,     ty,      "LEVEL COMPLETE!", GAME_RGB(255, 255, 255), 2);
  draw_text_scaled(bb, tx + 2, ty + 20, "LEVEL COMPLETE!", COLOR_COMPLETE,          2);

  /* "Click or press Enter" hint (1× scale, centred) */
  const char *hint = "CLICK or press ENTER for next level";
  int hlen = (int)strlen(hint);
  int hx   = (CANVAS_W - hlen * 9) / 2;
  draw_text(bb, hx, ty + 50, hint, GAME_RGB(255, 255, 255));
}
```

---

## Step 3: `update_level_complete` — advance or auto-advance

```c
/* src/game.c  —  update_level_complete */

static void update_level_complete(GameState *state, GameInput *input,
                                  float dt) {
  state->phase_timer += dt;

  /* Advance on left-click, Enter key, OR after 2 seconds. */
  int advance = (state->phase_timer > 2.0f)        ||
                BUTTON_PRESSED(input->mouse.left)   ||
                BUTTON_PRESSED(input->enter);

  if (advance) {
    int next = state->current_level + 1;
    if (next >= TOTAL_LEVELS) {
      /* All levels done — enter freeplay on the last level canvas */
      change_phase(state, PHASE_FREEPLAY);
    } else {
      level_load(state, next);
      change_phase(state, PHASE_PLAYING);
    }
  }
}
```

---

## Step 4: Freeplay mode

Freeplay runs after all 30 levels are complete.  The simulation still runs — grains
still fall, the player can still draw — but the win condition is never checked.

```c
/* src/game.c  —  update_freeplay */

static void update_freeplay(GameState *state, GameInput *input, float dt) {
  /* All the same input handling as PLAYING: */
  state->phase_timer += dt;
  int mx = input->mouse.x, my = input->mouse.y;

  state->reset_hover = (mx >= UI_RESET_X && mx < UI_RESET_X + UI_RESET_W &&
                        my >= UI_BTN_Y   && my < UI_BTN_Y   + UI_BTN_H);

  if (BUTTON_PRESSED(input->mouse.left) && state->reset_hover) {
    level_load(state, state->current_level);
    return;
  }
  if (BUTTON_PRESSED(input->escape)) {
    change_phase(state, PHASE_TITLE);
    return;
  }
  if (BUTTON_PRESSED(input->reset)) {
    level_load(state, state->current_level);
    return;
  }
  if (BUTTON_PRESSED(input->gravity))
    state->gravity_sign = -state->gravity_sign;

  int over_ui = (my >= UI_BTN_Y - 4);
  if (input->mouse.left.ended_down && !over_ui) {
    draw_brush_line(&state->lines, input->mouse.prev_x, input->mouse.prev_y,
                    input->mouse.x, input->mouse.y, BRUSH_RADIUS);
  }

  spawn_grains(state, dt);
  update_grains(state, dt);
  /* No check_win() call — that's the only difference from PLAYING. */
}

/* src/game.c  —  render_freeplay */

static void render_freeplay(const GameState *state, GameBackbuffer *bb) {
  render_playing(state, bb);  /* identical visuals */
  /* Small banner reminds the player they are in freeplay */
  draw_text(bb, 10, 8, "FREEPLAY - ESC for menu", COLOR_UI_TEXT);
}
```

---

## Step 5: Cyclic levels

Some levels have `is_cyclic = 1` in their `LevelDef`.  Grains that fall off the
bottom edge wrap around and re-enter from the top — sugar never disappears.

```c
/* src/game.c  —  update_grains: floor / ceiling handling */

/* After sub-step: check final y against canvas edges */
if (iy >= H) {
  if (lv->is_cyclic) {
    /* Wrap to top — same x position, y = 1 to avoid the ceiling wall */
    p->y[i] = 1.0f;
    p->x[i] = nx;
    continue;
  } else {
    /* Grain has fallen off screen — remove it */
    p->active[i] = 0;
    goto next_grain;
  }
}

/* Ceiling (when gravity is flipped): */
if (iy < 0) {
  if (lv->is_cyclic) {
    p->y[i] = (float)(H - 2);
    p->x[i] = nx;
    continue;
  } else {
    p->active[i] = 0;
    goto next_grain;
  }
}
```

---

## Step 6: Complete phase dispatch

The main update and render dispatchers wire every phase to its function:

```c
/* src/game.c  —  game_update: full phase dispatch */

void game_update(GameState *state, GameInput *input, float dt) {
  switch (state->phase) {
    case PHASE_TITLE:          update_title(state, input);         break;
    case PHASE_PLAYING:        update_playing(state, input, dt);   break;
    case PHASE_LEVEL_COMPLETE: update_level_complete(state, input, dt); break;
    case PHASE_FREEPLAY:       update_freeplay(state, input, dt);  break;
  }
}

/* src/game.c  —  game_render: full phase dispatch */

void game_render(const GameState *state, GameBackbuffer *bb) {
  switch (state->phase) {
    case PHASE_TITLE:          render_title(state, bb);          break;
    case PHASE_PLAYING:        render_playing(state, bb);        break;
    case PHASE_LEVEL_COMPLETE: render_level_complete(state, bb); break;
    case PHASE_FREEPLAY:       render_freeplay(state, bb);       break;
  }
}
```

---

## Step 7: Unlock progression in `update_playing`

When the player completes a level, unlock the next one before transitioning:

```c
/* src/game.c  —  inside update_playing, after check_win returns true */

if (check_win(state)) {
  int next = state->current_level + 1;
  /* Unlock next level (only if it isn't already unlocked) */
  if (next < TOTAL_LEVELS && next >= state->unlocked_count)
    state->unlocked_count = next + 1;
  change_phase(state, PHASE_LEVEL_COMPLETE);
}
```

`unlocked_count` starts at `1` (Level 1 is always available) and grows as the
player progresses.  The title screen reads this value to grey out locked buttons.

---

## Build and run (both backends)

```bash
# Raylib
./build-dev.sh --backend=raylib -r

# X11
./build-dev.sh --backend=x11 -r
```

**Expected behavior — both backends:**
- Completing a level shows the green overlay, then automatically advances after 2 seconds (or on click/Enter).
- After Level 30, entering freeplay shows the "FREEPLAY" banner; Escape returns to title.
- R key resets the level; G key flips gravity (on eligible levels); Escape goes back to title from PLAYING.
- On a cyclic level, grains wrap from bottom to top indefinitely.

---

## Exercises

1. The level-complete overlay currently auto-advances after exactly 2 seconds.  Change the timeout to 3 seconds and observe the difference.  What constant controls this?
2. In `render_level_complete`, the shadow text is drawn 2 px to the right and 20 px below.  Experiment with different offsets to achieve a drop-shadow vs. a neon-glow effect.
3. Add a `PHASE_PAUSE` state: pressing Escape from PLAYING goes to PAUSE (showing a dim overlay with a "PAUSED" message), and pressing Escape again resumes.

---

## What's next

Lesson 15 is the final lesson — the entire audio system, from procedural sound-effect
synthesis and the music sequencer to the Raylib and X11/ALSA platform audio backends.
