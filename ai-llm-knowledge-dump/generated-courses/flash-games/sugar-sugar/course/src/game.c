/*
 * game.c  —  Sugar, Sugar | Core Game Logic
 *
 * This file contains everything that is NOT platform-specific:
 *   - Physics simulation (grain gravity, collision, slide)
 *   - Level management  (load, reset, win detection)
 *   - Game FSM          (TITLE → PLAYING → LEVEL_COMPLETE → ...)
 *   - Rendering         (draw_pixel, draw_rect, draw_text, draw grains)
 *
 * Zero X11.  Zero Raylib.  Zero OS calls.
 * Compile it with any platform backend and it just works.
 */

#include "game.h"
#include "utils/font.h"

#include <math.h>   /* fabsf()         */
#include <stdlib.h> /* rand()          */
#include <string.h> /* memset, strlen  */

/* ===================================================================
 * PHYSICS CONSTANTS
 * =================================================================== */
#define GRAVITY        280.0f   /* pixels / second² — lower than default 9.8×scale
                                 * to give grains a "light sugar" feel: slow
                                 * enough to follow drawn lines comfortably       */
#define MAX_VY         600.0f   /* terminal velocity (px/s)                   */
#define MAX_VX         200.0f   /* max horizontal slide speed (px/s)          */
#define BRUSH_RADIUS   2        /* px — player brush (slim line)              */
#define EMITTER_JITTER  2.0f   /* random vx spread at spawn (±2 px/s velocity);
                                 * tiny — keeps the pour as a tight focused stream.
                                 * Large jitter fans grains over 40+ px, giving
                                 * <5% pixel coverage and looking like sparse
                                 * trickles rather than one solid pour.          */
#define EMITTER_SPREAD  3      /* random x position offset at spawn (±1 px);
                                * grains start at different pixels across the
                                * nozzle width so they never all pile on the same
                                * column, eliminating the "3 discrete streams"
                                * artifact caused by all grains colliding at
                                * the exact same (em->x, em->y) pixel           */

/* ===================================================================
 * GRAIN COLOR → PIXEL COLOR  lookup table
 * =================================================================== */
static const uint32_t g_grain_colors[GRAIN_COLOR_COUNT] = {
    [GRAIN_WHITE] = COLOR_CREAM,
    [GRAIN_RED] = COLOR_RED,
    [GRAIN_GREEN] = COLOR_GREEN,
    [GRAIN_ORANGE] = COLOR_ORANGE,
};

/* ===================================================================
 * FORWARD DECLARATIONS  (internal helpers)
 * =================================================================== */
static void change_phase(GameState *state, GAME_PHASE next);
static void level_load(GameState *state, int index);
static void update_title(GameState *state, GameInput *input);
static void update_playing(GameState *state, GameInput *input, float dt);
static void update_level_complete(GameState *state, GameInput *input, float dt);
static void update_freeplay(GameState *state, GameInput *input, float dt);

static void spawn_grains(GameState *state, float dt);
static void update_grains(GameState *state, float dt);
static int check_win(GameState *state);

static void stamp_circle(LineBitmap *lb, int cx, int cy, int r, uint8_t val);
static void draw_brush_line(LineBitmap *lb, int x0, int y0, int x1, int y1,
                            int radius);
static void stamp_obstacles(GameState *state);
static void stamp_cups(GameState *state);

/* rendering helpers */
static void draw_pixel(GameBackbuffer *bb, int x, int y, uint32_t color);
static void draw_rect(GameBackbuffer *bb, int x, int y, int w, int h,
                      uint32_t color);
static void draw_rect_blend(GameBackbuffer *bb, int x, int y, int w, int h,
                            uint32_t color, int alpha_0_255);
static void draw_rect_outline(GameBackbuffer *bb, int x, int y, int w, int h,
                              uint32_t color);
static void draw_circle(GameBackbuffer *bb, int cx, int cy, int r,
                        uint32_t color);
static void draw_circle_outline(GameBackbuffer *bb, int cx, int cy, int r,
                                uint32_t color);
static void draw_char(GameBackbuffer *bb, int x, int y, char c, uint32_t color);
static void draw_text(GameBackbuffer *bb, int x, int y, const char *str,
                      uint32_t color);
static void draw_text_scaled(GameBackbuffer *bb, int x, int y, const char *str,
                             uint32_t color, int scale);
static void draw_int(GameBackbuffer *bb, int x, int y, int n, uint32_t color);

static void render_title(const GameState *state, GameBackbuffer *bb);
static void render_playing(const GameState *state, GameBackbuffer *bb);
static void render_level_complete(const GameState *state, GameBackbuffer *bb);
static void render_freeplay(const GameState *state, GameBackbuffer *bb);

/* ===================================================================
 * PUBLIC API
 * =================================================================== */

void prepare_input_frame(GameInput *input) {
  /* Reset the "how many times did this change?" counter each frame.
   * We do NOT reset ended_down — that would lose the current hold state. */
  input->mouse.left.half_transition_count = 0;
  input->mouse.right.half_transition_count = 0;
  input->escape.half_transition_count = 0;
  input->reset.half_transition_count = 0;
  input->gravity.half_transition_count = 0;
}

void game_init(GameState *state, GameBackbuffer *backbuffer) {
  memset(state, 0, sizeof(*state));

  /* Set the backbuffer dimensions (pixels ptr is already set by the
   * platform before calling game_init). */
  backbuffer->width = CANVAS_W;
  backbuffer->height = CANVAS_H;
  backbuffer->pitch = CANVAS_W * 4;

  state->unlocked_count = 1; /* only level 1 unlocked at start */
  state->gravity_sign = 1;   /* gravity pulls downward          */
  state->title_hover = -1;

  change_phase(state, PHASE_TITLE);
}

void game_update(GameState *state, GameInput *input, float dt) {
  /* Cap delta-time so a debugger pause doesn't explode the physics. */
  if (dt > 0.1f)
    dt = 0.1f;

  switch (state->phase) {
  case PHASE_TITLE:
    update_title(state, input);
    break;
  case PHASE_PLAYING:
    update_playing(state, input, dt);
    break;
  case PHASE_LEVEL_COMPLETE:
    update_level_complete(state, input, dt);
    break;
  case PHASE_FREEPLAY:
    update_freeplay(state, input, dt);
    break;
  case PHASE_COUNT:
    break; /* unreachable — satisfies the compiler */
  }

  /* Advance the music sequencer each frame */
  game_audio_update(&state->audio, dt);
}

void game_render(const GameState *state, GameBackbuffer *bb) {
  switch (state->phase) {
  case PHASE_TITLE:
    render_title(state, bb);
    break;
  case PHASE_PLAYING:
    render_playing(state, bb);
    break;
  case PHASE_LEVEL_COMPLETE:
    render_level_complete(state, bb);
    break;
  case PHASE_FREEPLAY:
    render_freeplay(state, bb);
    break;
  case PHASE_COUNT:
    break;
  }
}

/* ===================================================================
 * STATE MACHINE
 * =================================================================== */

static void change_phase(GameState *state, GAME_PHASE next) {
  state->phase = next;
  state->phase_timer = 0.0f;

  /* Audio: trigger sounds / music appropriate for the new phase */
  switch (next) {
    case PHASE_TITLE:
      game_music_play_title(&state->audio);
      break;
    case PHASE_PLAYING:
      game_music_play_gameplay(&state->audio);
      game_play_sound(&state->audio, SOUND_TITLE_SELECT);
      break;
    case PHASE_LEVEL_COMPLETE:
      game_music_stop(&state->audio);
      game_play_sound(&state->audio, SOUND_LEVEL_COMPLETE);
      break;
    case PHASE_FREEPLAY:
      game_music_play_gameplay(&state->audio);
      break;
    case PHASE_COUNT:
      break;
  }
}

/* ===================================================================
 * LEVEL MANAGEMENT
 * =================================================================== */

static void level_load(GameState *state, int index) {
  /* Copy the level definition from the global read-only array. */
  state->level = g_levels[index];
  state->current_level = index;

  /* Clear all grains and drawn lines. */
  memset(&state->grains, 0, sizeof(state->grains));
  memset(&state->lines, 0, sizeof(state->lines));

  /* Reset emitter timers inside the copied level. */
  for (int i = 0; i < state->level.emitter_count; i++)
    state->level.emitters[i].spawn_timer = 0.0f;

  /* Reset cup counters. */
  for (int i = 0; i < state->level.cup_count; i++)
    state->level.cups[i].collected = 0;

  /* Gravity is reset to normal on each level load. */
  state->gravity_sign = 1;

  /* Stamp pre-set obstacles and cup walls into the line bitmap. */
  stamp_obstacles(state);
  stamp_cups(state);
}

static void stamp_obstacles(GameState *state) {
  /* Obstacles are solid rectangles baked into the line bitmap.
   * From the physics engine's view they are indistinguishable from
   * player-drawn lines — the bitmap doesn't care who put the pixels there. */
  LevelDef *lv = &state->level;
  for (int i = 0; i < lv->obstacle_count; i++) {
    Obstacle *o = &lv->obstacles[i];
    for (int py = o->y; py < o->y + o->h; py++) {
      for (int px = o->x; px < o->x + o->w; px++) {
        if (px >= 0 && px < CANVAS_W && py >= 0 && py < CANVAS_H)
          state->lines.pixels[py * CANVAS_W + px] = 1;
      }
    }
  }
}

static void stamp_cups(GameState *state) {
  /* Stamp the physical walls of each cup into the line bitmap so that
   * grains collide with the cup edges realistically.
   *
   * Cup geometry (x, y, w, h):
   *
   *     x         x+w-1
   *     |  (open)  |    ← y  (top is open — grains fall in)
   *     |          |
   *     |__________|    ← y+h-1 (solid bottom)
   *
   * The cup AABB absorption check uses (x+1 .. x+w-2) so that it covers
   * only the interior — the wall pixels themselves are outside the absorb
   * zone and act purely as physics barriers.
   *
   * Overflow behaviour: when a cup is full, incoming grains are not
   * absorbed; they hit the solid bottom wall and pile up inside.  The pile
   * grows upward until grains can slide off the open top rim. */
  LevelDef *lv = &state->level;
  LineBitmap *lb = &state->lines;

  for (int c = 0; c < lv->cup_count; c++) {
    Cup *cup = &lv->cups[c];
    int cx = cup->x, cy = cup->y, cw = cup->w, ch = cup->h;

    /* Left wall: x = cx, rows [cy .. cy+ch) */
    for (int py = cy; py < cy + ch; py++)
      if (cx >= 0 && cx < CANVAS_W && py >= 0 && py < CANVAS_H)
        lb->pixels[py * CANVAS_W + cx] = 1;

    /* Right wall: x = cx+cw-1 */
    for (int py = cy; py < cy + ch; py++) {
      int rx = cx + cw - 1;
      if (rx >= 0 && rx < CANVAS_W && py >= 0 && py < CANVAS_H)
        lb->pixels[py * CANVAS_W + rx] = 1;
    }

    /* Bottom wall: y = cy+ch-1, all columns */
    for (int px = cx; px < cx + cw; px++) {
      int by = cy + ch - 1;
      if (px >= 0 && px < CANVAS_W && by >= 0 && by < CANVAS_H)
        lb->pixels[by * CANVAS_W + px] = 1;
    }
  }
}

/* ===================================================================
 * UPDATE FUNCTIONS  (one per phase)
 * =================================================================== */

/* Title screen layout constants — shared by update_title() and render_title()
 * so that button positions in logic always match what's drawn on screen. */
#define TITLE_BTN_W 70
#define TITLE_BTN_H 40
#define TITLE_BTN_GAP 8
#define TITLE_COLS 6
#define TITLE_GRID_X                                                           \
  ((CANVAS_W - (TITLE_COLS * (TITLE_BTN_W + TITLE_BTN_GAP) - TITLE_BTN_GAP)) / \
   2)
#define TITLE_GRID_Y 160

static void update_title(GameState *state, GameInput *input) {
  int mx = input->mouse.x;
  int my = input->mouse.y;
  state->title_hover = -1;

  /* Use the shared layout macros — render_title uses the same values
   * so hover detection always matches what's drawn on screen. */
  int grid_x = TITLE_GRID_X;
  int grid_y = TITLE_GRID_Y;

  for (int i = 0; i < TOTAL_LEVELS; i++) {
    int col = i % TITLE_COLS, row = i / TITLE_COLS;
    int bx = grid_x + col * (TITLE_BTN_W + TITLE_BTN_GAP);
    int by = grid_y + row * (TITLE_BTN_H + TITLE_BTN_GAP);
    if (mx >= bx && mx < bx + TITLE_BTN_W && my >= by &&
        my < by + TITLE_BTN_H) {
      state->title_hover = i;
      if (BUTTON_PRESSED(input->mouse.left) && i < state->unlocked_count) {
        level_load(state, i);
        change_phase(state, PHASE_PLAYING);
      }
    }
  }

  if (BUTTON_PRESSED(input->escape))
    state->should_quit = 1;
}

/* Layout constants shared by update_playing and render_ui_buttons.
 * ONE definition prevents the hover-detection / rendering mismatch. */
#define UI_BTN_Y (CANVAS_H - 38)
#define UI_BTN_H 28
#define UI_RESET_X 10
#define UI_RESET_W 70
#define UI_GRAV_X 88
#define UI_GRAV_W 100

static void update_playing(GameState *state, GameInput *input, float dt) {
  state->phase_timer += dt;

  int mx = input->mouse.x, my = input->mouse.y;

  /* ---- On-screen button hover (updated every frame) ---- */
  state->reset_hover = (mx >= UI_RESET_X && mx < UI_RESET_X + UI_RESET_W &&
                        my >= UI_BTN_Y && my < UI_BTN_Y + UI_BTN_H);

  state->gravity_hover = state->level.has_gravity_switch &&
                         (mx >= UI_GRAV_X && mx < UI_GRAV_X + UI_GRAV_W &&
                          my >= UI_BTN_Y && my < UI_BTN_Y + UI_BTN_H);

  /* ---- On-screen button clicks (mouse left press this frame) ---- */
  if (BUTTON_PRESSED(input->mouse.left)) {
    if (state->reset_hover) {
      game_play_sound(&state->audio, SOUND_RESET);
      level_load(state, state->current_level);
      return;
    }
    if (state->gravity_hover) {
      state->gravity_sign = -state->gravity_sign;
      game_play_sound(&state->audio, SOUND_GRAVITY_FLIP);
      /* Don't return — other logic can still run this frame */
    }
  }

  /* ---- Reset (R key shortcut) ---- */
  if (BUTTON_PRESSED(input->reset)) {
    game_play_sound(&state->audio, SOUND_RESET);
    level_load(state, state->current_level);
    return;
  }

  /* ---- Escape → back to title ---- */
  if (BUTTON_PRESSED(input->escape)) {
    change_phase(state, PHASE_TITLE);
    return;
  }

  /* ---- Gravity flip (G key shortcut) ---- */
  if (state->level.has_gravity_switch && BUTTON_PRESSED(input->gravity)) {
    state->gravity_sign = -state->gravity_sign;
    game_play_sound(&state->audio, SOUND_GRAVITY_FLIP);
  }

  /* ---- Mouse drawing ----
   * Guard: do not draw into the bottom UI strip (buttons live there). */
  int over_ui = (my >= UI_BTN_Y - 4);
  if (input->mouse.left.ended_down && !over_ui) {
    draw_brush_line(&state->lines, input->mouse.prev_x, input->mouse.prev_y,
                    input->mouse.x, input->mouse.y, BRUSH_RADIUS);
  }

  /* ---- Simulation ---- */
  spawn_grains(state, dt);
  update_grains(state, dt);

  /* ---- Win check ---- */
  if (check_win(state)) {
    /* Unlock the next level if not already unlocked. */
    int next = state->current_level + 1;
    if (next < TOTAL_LEVELS && next >= state->unlocked_count)
      state->unlocked_count = next + 1;
    change_phase(state, PHASE_LEVEL_COMPLETE);
  }
}

static void update_level_complete(GameState *state, GameInput *input, float dt) {
  state->phase_timer += dt;

  /* Advance immediately on left-click or ENTER key press;
   * also auto-advance after 2 seconds so players don't get stuck. */
  int advance = (state->phase_timer > 2.0f) ||
                BUTTON_PRESSED(input->mouse.left) ||
                BUTTON_PRESSED(input->enter);
  if (advance) {
    int next = state->current_level + 1;
    if (next >= TOTAL_LEVELS) {
      change_phase(state, PHASE_FREEPLAY);
    } else {
      level_load(state, next);
      change_phase(state, PHASE_PLAYING);
    }
  }
}

static void update_freeplay(GameState *state, GameInput *input, float dt) {
  /* Freeplay is the same as PLAYING but never triggers win. */
  state->phase_timer += dt;

  int mx = input->mouse.x, my = input->mouse.y;

  /* Hover + click for Reset button (same as PLAYING) */
  state->reset_hover = (mx >= UI_RESET_X && mx < UI_RESET_X + UI_RESET_W &&
                        my >= UI_BTN_Y && my < UI_BTN_Y + UI_BTN_H);

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
  /* No win check in freeplay. */
}

/* ===================================================================
 * GRAIN SIMULATION
 * =================================================================== */

static int grain_alloc(GrainPool *p) {
  /* Scan for a free slot.  We keep a "high watermark" (count) so the scan
   * stays short during the early frames when most slots are empty. */
  for (int i = 0; i < p->count; i++) {
    if (!p->active[i])
      return i;
  }
  if (p->count < MAX_GRAINS) {
    return p->count++;
  }
  return -1; /* pool full */
}

static void spawn_grains(GameState *state, float dt) {
  LevelDef *lv = &state->level;
  GrainPool *p = &state->grains;

  /* Sugar falls continuously — no artificial cap.  The pool stays healthy
   * because settled grains are baked into the line bitmap and freed (see
   * update_grains).  A free slot is almost always available. */

  for (int e = 0; e < lv->emitter_count; e++) {
    Emitter *em = &lv->emitters[e];
    em->spawn_timer += dt;
    float interval = 1.0f / (float)em->grains_per_second;
    while (em->spawn_timer >= interval) {
      em->spawn_timer -= interval;
      int slot = grain_alloc(&state->grains);
      if (slot < 0)
        break; /* pool full */

      p->active[slot] = 1;
      p->color[slot] = GRAIN_WHITE;
      p->tpcd[slot] = 0;
      p->still[slot] = 0;

      /* Velocity jitter: small random vx so grains fan out as they fall.  */
      float jitter =
          ((float)(rand() % 1000) / 1000.0f - 0.5f) * EMITTER_JITTER * 2.0f;

      /* Position spread: each grain starts at a slightly different x pixel
       * within ±EMITTER_SPREAD of the emitter.  Without this, ALL grains
       * begin at the exact same pixel.  The first grain that stalls there
       * occupies that pixel in the occupancy bitmap — every subsequent grain
       * collides immediately and slides to ±1 or ±2 pixels, forming three
       * discrete "satellite streams" rather than one smooth pour.
       * A small x spread distributes the impact across several pixels so the
       * pile grows uniformly from the start. */
      int x_spread = (rand() % (2 * EMITTER_SPREAD + 1)) - EMITTER_SPREAD;

      p->x[slot] = (float)(em->x + x_spread);
      p->y[slot] = (float)em->y;
      p->vx[slot] = jitter;
      p->vy[slot] = (state->gravity_sign > 0) ? 40.0f : -40.0f;
    }
  }
}

static void update_grains(GameState *state, float dt) {
  GrainPool *p = &state->grains;
  LineBitmap *lb = &state->lines;
  LevelDef *lv = &state->level;
  int W = CANVAS_W, H = CANVAS_H;
  float grav = GRAVITY * (float)state->gravity_sign;

  /* ---- Grain-occupancy bitmap ----
   * This lets grains collide with *each other*, not just with drawn lines.
   * The bitmap is declared static so it lives in BSS (not 307 KB on the stack).
   * We rebuild it from scratch every frame to reflect the latest positions. */
  static uint8_t s_occ[CANVAS_W * CANVAS_H];
  __builtin_memset(s_occ, 0, sizeof(s_occ));
  for (int i = 0; i < p->count; i++) {
    if (!p->active[i])
      continue;
    int ix = (int)p->x[i], iy = (int)p->y[i];
    if (ix >= 0 && ix < W && iy >= 0 && iy < H)
      s_occ[iy * W + ix] = 1;
  }

/* Solid = player-drawn / obstacle line pixel  OR  another settled grain.
 * Out-of-bounds in X is treated as solid (wall); out-of-bounds in Y is
 * handled separately (ceiling / floor logic). */
#define IS_SOLID(xx, yy)                                                       \
  ((xx) < 0 || (xx) >= W || lb->pixels[(yy) * W + (xx)] ||                     \
   s_occ[(yy) * W + (xx)])

  for (int i = 0; i < p->count; i++) {
    if (!p->active[i])
      continue;

    /* Unmark this grain's current position so it doesn't block itself. */
    {
      int cx = (int)p->x[i], cy = (int)p->y[i];
      if (cx >= 0 && cx < W && cy >= 0 && cy < H)
        s_occ[cy * W + cx] = 0;
    }

    /* ---- Apply gravity ---- */
    p->vy[i] += grav * dt;
    if (p->vy[i] > MAX_VY)
      p->vy[i] = MAX_VY;
    if (p->vy[i] < -MAX_VY)
      p->vy[i] = -MAX_VY;
    if (p->vx[i] > MAX_VX)
      p->vx[i] = MAX_VX;
    if (p->vx[i] < -MAX_VX)
      p->vx[i] = -MAX_VX;

    /* ---- Horizontal drag ----
     * Multiplied every frame so grains naturally decelerate after sliding
     * off the end of a ramp.  GRAIN_HORIZ_DRAG = 0.98 ≈ 30% per second. */
    p->vx[i] *= GRAIN_HORIZ_DRAG;

    /* Capture position before sub-steps for the displacement-based settle
     * check at the end of this grain's update. */
    float pre_x = p->x[i], pre_y = p->y[i];

    /* ---- Sub-step integration ----
     * Split the total displacement into steps of at most 1 px each so
     * that a fast grain cannot "tunnel" through a 1-px thick line. */
    float total_dx = p->vx[i] * dt;
    float total_dy = p->vy[i] * dt;
    float abs_dx = total_dx < 0 ? -total_dx : total_dx;
    float abs_dy = total_dy < 0 ? -total_dy : total_dy;
    int steps = (int)(abs_dx + abs_dy) + 1;
    if (steps > 32)
      steps = 32; /* cap for performance */
    float sdx = total_dx / (float)steps;
    float sdy = total_dy / (float)steps;

    for (int s = 0; s < steps; s++) {
      float nx = p->x[i] + sdx;
      float ny = p->y[i] + sdy;
      int ix = (int)nx;
      int iy = (int)ny;

      /* ---- Screen boundary: left/right walls ---- */
      if (ix < 0) {
        p->x[i] = 0.f;
        p->vx[i] = 0.f;
        break;
      }
      if (ix >= W) {
        p->x[i] = (float)(W - 1);
        p->vx[i] = 0.f;
        break;
      }

      /* ---- Screen boundary: ceiling (includes reversed gravity) ---- */
      if (iy < 0) {
        p->y[i] = 0.f;
        p->vy[i] = 0.f;
        break;
      }

      /* ---- Screen boundary: floor → wrap or remove ---- */
      if (iy >= H) {
        if (lv->is_cyclic) {
          p->y[i] = 1.f;
          p->x[i] = nx;
          continue; /* restart sub-step from top of screen */
        } else {
          p->active[i] = 0;
          goto next_grain;
        }
      }

      /* ---- Free pixel? → move there and continue ---- */
      if (!IS_SOLID(ix, iy)) {
        p->x[i] = nx;
        p->y[i] = ny;
        continue;
      }

      /* ================================================================
       * COLLISION — grain cannot enter (ix, iy).
       *
       * Sugar Sugar "follow-the-line" rule:
       *
       *   If the grain has a VERTICAL velocity component (iy != oy) it
       *   is hitting a surface from above (or below in reversed gravity).
       *   Like sand on a slope, it tries to slide to the adjacent free
       *   pixel at the same target row (diagonal).
       *   • Preferred direction = current vx sign (keep momentum).
       *   • Slide vx is proportional to |vy| so faster-falling grains
       *     slide faster — this creates natural slope-following.
       *   • After any collision we BREAK out of sub-steps so the new
       *     velocity takes effect cleanly from the next frame.
       *
       *   If the grain was moving ONLY HORIZONTALLY (iy == oy) it hit a
       *   vertical wall → just kill vx and let gravity resume.
       * ================================================================ */
      {
        int oy = (int)p->y[i];
        int ox = (int)p->x[i];

        if (iy != oy) {
          /* Vertical component → try diagonal slide along surface.
           *
           * We check up to GRAIN_SLIDE_REACH pixels to each side (preferred
           * direction = vx sign, fallback = opposite).  Checking ±2 instead
           * of ±1 reduces the natural angle of repose from 45° to ~27°,
           * matching the more fluid look of the original Sugar, Sugar game.
           *
           * A ±1 check only allows a grain to slide if the immediately
           * adjacent pixel is free — any 2-wide pile blocks it.  With ±2,
           * grains can skip over a 1-wide neighbour and settle one pixel
           * further out, creating a shallower, more natural heap. */
          int d1 = (p->vx[i] >= 0.0f) ? 1 : -1;
          int d2 = -d1;
          int slid = 0;

          for (int dist = 1; dist <= GRAIN_SLIDE_REACH && !slid; dist++) {
            for (int attempt = 0; attempt < 2 && !slid; attempt++) {
              int d = (attempt == 0) ? d1 : d2;
              int sx = ox + d * dist;
              if (sx < 0 || sx >= W)
                continue;
              /* All pixels between ox and sx at the same row must be free
               * — we don't want the grain to teleport through a wall. */
              int path_clear = 1;
              for (int px = ox + d; px != sx + d; px += d) {
                if (IS_SOLID(px, iy)) { path_clear = 0; break; }
              }
              if (!path_clear)
                continue;
              if (!IS_SOLID(sx, iy)) {
                /* Slide speed: use max(|vy|, GRAIN_SLIDE_MIN) so grains
                 * always move fast enough to register as "moving" by the
                 * displacement-based settle check — even on shallow
                 * re-contacts where gravity has only barely re-accumulated.
                 * The minimum only fires here (free diagonal found); when
                 * all diagonals are blocked the !slid path zeros velocity
                 * and the grain settles normally. */
                float impact_vy = fabsf(p->vy[i]);
                float spd = impact_vy;
                if (spd < GRAIN_SLIDE_MIN) spd = GRAIN_SLIDE_MIN;
                if (spd > MAX_VX) spd = MAX_VX;
                p->vx[i] = (float)d * spd;
                /* Tiny bounce: grain pops up slightly after landing on a
                 * slope, giving it a lively feel before sliding away. */
                p->vy[i] = (impact_vy > GRAIN_BOUNCE_MIN)
                            ? -impact_vy * GRAIN_BOUNCE
                            : 0.0f;
                p->x[i] = (float)sx;
                p->y[i] = (float)iy;
                slid = 1;
              }
            }
          }

          if (!slid) {
            /* Completely blocked (e.g. piled up on a flat surface).
             * On a hard impact, bounce lightly upward before settling.
             * Once |vy| < GRAIN_BOUNCE_MIN the bounce is dropped and
             * the grain comes to rest normally. */
            float impact_vy = fabsf(p->vy[i]);
            p->vy[i] = (impact_vy > GRAIN_BOUNCE_MIN)
                        ? -impact_vy * GRAIN_BOUNCE
                        : 0.0f;
            p->vx[i] = 0.0f;
          }
        } else {
          /* Pure horizontal collision → vertical surface (wall).
           * Kill horizontal velocity; grain continues falling. */
          p->vx[i] = 0.0f;
        }
      }
      break; /* always stop sub-stepping after a collision */

    } /* end sub-steps */

    /* Mark final position so subsequent grains treat this as solid. */
    {
      int fx = (int)p->x[i], fy = (int)p->y[i];
      if (fx >= 0 && fx < W && fy >= 0 && fy < H)
        s_occ[fy * W + fx] = 1;
    }

    /* ---- Teleport cooldown ---- */
    if (p->tpcd[i] > 0)
      p->tpcd[i]--;

    /* ---- Cup check (AABB — interior only, walls excluded) ----
     * We test against the *interior* of the cup (x+1 .. x+w-2, y .. y+h-2)
     * because stamp_cups() placed solid wall pixels at the exact edges.
     * Checking the interior avoids double-processing a grain that is still
     * physically colliding with the wall.
     *
     * When the cup is full the grain is NOT absorbed — it will rest on
     * the solid bottom wall or the pile of earlier overflow grains and
     * eventually spill back out over the open top rim. */
    {
      int gx = (int)p->x[i], gy = (int)p->y[i];
      for (int c = 0; c < lv->cup_count; c++) {
        Cup *cup = &lv->cups[c];
        /* Interior bounds: one pixel inside each wall. */
        int ix0 = cup->x + 1, ix1 = cup->x + cup->w - 1;
        int iy0 = cup->y, iy1 = cup->y + cup->h - 1;
        if (gx >= ix0 && gx < ix1 && gy >= iy0 && gy < iy1) {
          int right_color = (cup->required_color == GRAIN_WHITE ||
                             p->color[i] == (uint8_t)cup->required_color);
          if (right_color && cup->collected < cup->required_count) {
            /* Absorb: count this grain, clear its occupancy, remove from sim. */
            int was_full_before = (cup->collected + 1 >= cup->required_count);
            cup->collected++;
            if (was_full_before) {
              /* Cup just reached 100% — play chime */
              game_play_sound(&state->audio, SOUND_CUP_FILL);
            }
            s_occ[gy * W + gx] = 0; /* clear occupancy — grain is gone now */
            p->active[i] = 0;
            goto next_grain;
          } else if (!right_color) {
            /* Wrong color: discard silently (no penalty). */
            s_occ[gy * W + gx] = 0;
            p->active[i] = 0;
            goto next_grain;
          }
          /* Cup is full with correct color: grain stays active.
           * The solid walls/bottom will cause it to pile up and
           * eventually spill over the open top rim. */
        }
      }
    }

    /* ---- Color filter check (AABB) ---- */
    {
      int gx = (int)p->x[i], gy = (int)p->y[i];
      for (int f = 0; f < lv->filter_count; f++) {
        ColorFilter *flt = &lv->filters[f];
        if (gx >= flt->x && gx < flt->x + flt->w && gy >= flt->y &&
            gy < flt->y + flt->h) {
          p->color[i] = (uint8_t)flt->output_color;
        }
      }
    }

    /* ---- Teleporter check (circle distance) ---- */
    if (p->tpcd[i] == 0) {
      int gx = (int)p->x[i], gy = (int)p->y[i];
      for (int t = 0; t < lv->teleporter_count; t++) {
        Teleporter *tp = &lv->teleporters[t];
        int da = (gx - tp->ax) * (gx - tp->ax) + (gy - tp->ay) * (gy - tp->ay);
        int db = (gx - tp->bx) * (gx - tp->bx) + (gy - tp->by) * (gy - tp->by);
        int r2 = tp->radius * tp->radius;
        if (da <= r2) {
          p->x[i] = (float)tp->bx;
          p->y[i] = (float)tp->by;
          p->tpcd[i] = 6;
          break;
        }
        if (db <= r2) {
          p->x[i] = (float)tp->ax;
          p->y[i] = (float)tp->ay;
          p->tpcd[i] = 6;
          break;
        }
      }
    }

    /* ---- Settled grain: bake into line bitmap and free slot ---- */
    {
      /* ---- Displacement-based settle detection (frame-rate independent) ----
       * Compare displacement this frame vs. a threshold that scales with dt.
       *
       * A grain is "still" if its average speed this frame < GRAIN_SETTLE_SPEED.
       *   speed ≈ dist/dt  →  dist < GRAIN_SETTLE_SPEED * dt
       *   dist² < (GRAIN_SETTLE_SPEED * dt)²
       *
       * This fixes the original velocity-based check (broken because gravity
       * re-adds ~8 px/s every frame) AND the fixed-distance check (broken
       * because at uncapped FPS, dt is tiny and dist per frame is microscopic
       * — causing all grains to immediately be marked "still"). */
      float settle_sq = GRAIN_SETTLE_SPEED * GRAIN_SETTLE_SPEED * dt * dt;
      float ddx = p->x[i] - pre_x;
      float ddy = p->y[i] - pre_y;
      if (ddx * ddx + ddy * ddy < settle_sq) {
        if (p->still[i] < 255) p->still[i]++;
      } else {
        p->still[i] = 0;
      }
      if (p->still[i] >= GRAIN_SETTLE_FRAMES) {
        int bx = (int)p->x[i], by = (int)p->y[i];
        if (bx >= 0 && bx < W && by >= 0 && by < H)
          lb->pixels[by * W + bx] = (uint8_t)(p->color[i] + 2);
        p->active[i] = 0;
        goto next_grain;
      }
    }

  next_grain:;
  }

#undef IS_SOLID
}

static int check_win(GameState *state) {
  LevelDef *lv = &state->level;
  for (int c = 0; c < lv->cup_count; c++) {
    if (lv->cups[c].collected < lv->cups[c].required_count)
      return 0; /* at least one cup not full yet */
  }
  return 1; /* all cups filled */
}

/* ===================================================================
 * LINE BITMAP DRAWING
 * =================================================================== */

static void stamp_circle(LineBitmap *lb, int cx, int cy, int r, uint8_t val) {
  int W = CANVAS_W, H = CANVAS_H;
  for (int dy = -r; dy <= r; dy++) {
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy <= r * r) {
        int px = cx + dx, py = cy + dy;
        if (px >= 0 && px < W && py >= 0 && py < H)
          lb->pixels[py * W + px] = val;
      }
    }
  }
}

static void draw_brush_line(LineBitmap *lb, int x0, int y0, int x1, int y1,
                            int radius) {
  /* Walk every pixel along the line using the Bresenham algorithm,
   * then stamp a filled circle at each point.
   * This ensures thick, gapless lines even when the mouse moves fast. */
  int dx = (x1 > x0 ? x1 - x0 : x0 - x1);
  int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
  int sx = (x0 < x1 ? 1 : -1);
  int sy = (y0 < y1 ? 1 : -1);
  int err = dx + dy;
  int x = x0, y = y0;

  for (;;) {
    stamp_circle(lb, x, y, radius, 255);
    if (x == x1 && y == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y += sy;
    }
  }
}

/* ===================================================================
 * RENDERING HELPERS
 * =================================================================== */

static inline void draw_pixel(GameBackbuffer *bb, int x, int y, uint32_t c) {
  if ((unsigned)x < (unsigned)bb->width && (unsigned)y < (unsigned)bb->height)
    bb->pixels[y * bb->width + x] = c;
}

static void draw_rect(GameBackbuffer *bb, int x, int y, int w, int h,
                      uint32_t color) {
  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = (x + w) > bb->width ? bb->width : (x + w);
  int y1 = (y + h) > bb->height ? bb->height : (y + h);
  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * bb->width;
    for (int px = x0; px < x1; px++)
      row[px] = color;
  }
}

static void draw_rect_blend(GameBackbuffer *bb, int x, int y, int w, int h,
                            uint32_t color, int alpha) {
  /* Alpha-blend: out = (src * a + dst * (255-a)) >> 8 */
  uint32_t sr = (color >> 16) & 0xFF;
  uint32_t sg = (color >> 8) & 0xFF;
  uint32_t sb = color & 0xFF;
  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = (x + w) > bb->width ? bb->width : (x + w);
  int y1 = (y + h) > bb->height ? bb->height : (y + h);
  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * bb->width;
    for (int px = x0; px < x1; px++) {
      uint32_t dst = row[px];
      uint32_t dr = (dst >> 16) & 0xFF;
      uint32_t dg = (dst >> 8) & 0xFF;
      uint32_t db = dst & 0xFF;
      uint32_t or_ = (sr * alpha + dr * (255 - alpha)) >> 8;
      uint32_t og = (sg * alpha + dg * (255 - alpha)) >> 8;
      uint32_t ob = (sb * alpha + db * (255 - alpha)) >> 8;
      row[px] = GAME_RGB(or_, og, ob);
    }
  }
}

static void draw_rect_outline(GameBackbuffer *bb, int x, int y, int w, int h,
                              uint32_t color) {
  draw_rect(bb, x, y, w, 1, color);         /* top    */
  draw_rect(bb, x, y + h - 1, w, 1, color); /* bottom */
  draw_rect(bb, x, y, 1, h, color);         /* left   */
  draw_rect(bb, x + w - 1, y, 1, h, color); /* right  */
}

static void draw_circle(GameBackbuffer *bb, int cx, int cy, int r,
                        uint32_t color) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++)
      if (dx * dx + dy * dy <= r * r)
        draw_pixel(bb, cx + dx, cy + dy, color);
}

static void draw_circle_outline(GameBackbuffer *bb, int cx, int cy, int r,
                                uint32_t color) {
  /* Draw only the pixels that lie on the circumference (within 1 px). */
  int r2_outer = (r + 1) * (r + 1);
  int r2_inner = (r - 1) * (r - 1);
  for (int dy = -r - 1; dy <= r + 1; dy++)
    for (int dx = -r - 1; dx <= r + 1; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 >= r2_inner && d2 <= r2_outer)
        draw_pixel(bb, cx + dx, cy + dy, color);
    }
}

static void draw_char(GameBackbuffer *bb, int x, int y, char c,
                      uint32_t color) {
  if (c < 0x20 || c > 0x7E)
    return;
  const uint8_t *glyph = g_font8x8[(uint8_t)c - 0x20];
  for (int row = 0; row < 8; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < 8; col++)
      /* BIT 0 = leftmost pixel (the convention used by our font data).
       * Using (1<<col) tests bit 0 first → col 0 is leftmost. ✓
       * The previous (0x80>>col) tested bit 7 first → chars were mirrored. */
      if (bits & (1 << col))
        draw_pixel(bb, x + col, y + row, color);
  }
}

/* Scaled variant: each pixel becomes a scale×scale block.
 * Use scale=2 for readable UI labels (16px tall), scale=3 for big titles (24px
 * tall). */
static void draw_text_scaled(GameBackbuffer *bb, int x, int y, const char *str,
                             uint32_t color, int scale) {
  int cx = x;
  while (*str) {
    if (*str == '\n') {
      cx = x;
      y += (8 + 1) * scale;
    } else {
      if (*str >= 0x20 && *str <= 0x7E) {
        const uint8_t *glyph = g_font8x8[(uint8_t)*str - 0x20];
        for (int row = 0; row < 8; row++) {
          uint8_t bits = glyph[row];
          for (int col = 0; col < 8; col++) {
            if (bits & (1 << col))
              draw_rect(bb, cx + col * scale, y + row * scale, scale, scale,
                        color);
          }
        }
      }
      cx += 9 * scale;
    }
    str++;
  }
}

static void draw_text(GameBackbuffer *bb, int x, int y, const char *str,
                      uint32_t color) {
  int cx = x;
  while (*str) {
    if (*str == '\n') {
      cx = x;
      y += 12;
    } else {
      draw_char(bb, cx, y, *str, color);
      cx += 9;
    }
    str++;
  }
}

/* Draw an integer without printf (no dependencies on stdio in the hot path). */
/* draw_int: convenience wrapper kept for course reference — use
 * draw_text_scaled in production code for readable UI text. */
static void draw_int(GameBackbuffer *bb, int x, int y, int n, uint32_t color)
    __attribute__((unused));
static void draw_int(GameBackbuffer *bb, int x, int y, int n, uint32_t color) {
  char buf[12];
  int i = 0;
  if (n == 0) {
    buf[i++] = '0';
  } else {
    if (n < 0) {
      buf[i++] = '-';
      n = -n;
    }
    char tmp[10];
    int ti = 0;
    while (n > 0) {
      tmp[ti++] = (char)('0' + (n % 10));
      n /= 10;
    }
    while (ti > 0)
      buf[i++] = tmp[--ti];
  }
  buf[i] = '\0';
  draw_text(bb, x, y, buf, color);
}

/* ===================================================================
 * RENDER HELPERS: cups, filters, portals, lines, grains
 * =================================================================== */

static void render_cups(const LevelDef *lv, GameBackbuffer *bb) {
  for (int c = 0; c < lv->cup_count; c++) {
    const Cup *cup = &lv->cups[c];
    int full = (cup->collected >= cup->required_count);

    /* ---- Cup interior fill (progress bar rising from bottom) ---- */
    draw_rect(bb, cup->x + 1, cup->y, cup->w - 2, cup->h - 1, COLOR_CUP_EMPTY);

    if (cup->required_count > 0) {
      int filled_h =
          full ? (cup->h - 1)
               : (cup->collected * (cup->h - 1)) / cup->required_count;
      draw_rect(bb, cup->x + 1, cup->y + (cup->h - 1) - filled_h, cup->w - 2,
                filled_h, full ? COLOR_CUP_FILL_FULL : COLOR_CUP_FILL);
    }

    /* ---- Cup walls (draw on top of fill to show the frame) ---- */
    /* Left wall */
    draw_rect(bb, cup->x, cup->y, 1, cup->h, COLOR_CUP_BORDER);
    /* Right wall */
    draw_rect(bb, cup->x + cup->w - 1, cup->y, 1, cup->h, COLOR_CUP_BORDER);
    /* Bottom */
    draw_rect(bb, cup->x, cup->y + cup->h - 1, cup->w, 1, COLOR_CUP_BORDER);

    /* ---- Numeric counter centred above the cup ----
     * Show remaining needed, at 2× scale so it's legible.
     * When full, show "OK" instead. */
    if (full) {
      int tx = cup->x + cup->w / 2 - 9 * 2; /* "OK" = 2 chars × 9 × 2 */
      draw_text_scaled(bb, tx, cup->y - 20, "OK", COLOR_CUP_FILL_FULL, 2);
    } else {
      int remaining = cup->required_count - cup->collected;
      /* Build the number string to measure its width. */
      char buf[8];
      int bi = 0;
      int tmp = remaining;
      if (tmp == 0) {
        buf[bi++] = '0';
      } else {
        char t[8];
        int ti = 0;
        while (tmp > 0) {
          t[ti++] = (char)('0' + tmp % 10);
          tmp /= 10;
        }
        while (ti > 0)
          buf[bi++] = t[--ti];
      }
      buf[bi] = '\0';
      /* Centre: each char is 9*2=18px wide */
      int tw = bi * 18;
      int tx = cup->x + (cup->w - tw) / 2;
      draw_text_scaled(bb, tx, cup->y - 20, buf, COLOR_UI_TEXT, 2);
    }

    /* ---- Required-color dot inside (for colored cups only) ---- */
    if (cup->required_color != GRAIN_WHITE) {
      draw_circle(bb, cup->x + cup->w / 2, cup->y + (cup->h / 2) + 4, 6,
                  g_grain_colors[cup->required_color]);
    }
  }
}

static void render_filters(const LevelDef *lv, GameBackbuffer *bb) {
  static const uint32_t filter_colors[GRAIN_COLOR_COUNT] = {
      [GRAIN_WHITE] = COLOR_CREAM,
      [GRAIN_RED] = COLOR_RED,
      [GRAIN_GREEN] = COLOR_GREEN,
      [GRAIN_ORANGE] = COLOR_ORANGE,
  };
  for (int f = 0; f < lv->filter_count; f++) {
    const ColorFilter *flt = &lv->filters[f];
    /* Draw a semi-transparent colored band */
    draw_rect_blend(bb, flt->x, flt->y, flt->w, flt->h,
                    filter_colors[flt->output_color], 140);
    draw_rect_outline(bb, flt->x, flt->y, flt->w, flt->h,
                      filter_colors[flt->output_color]);
  }
}

static void render_teleporters(const LevelDef *lv, GameBackbuffer *bb,
                               float phase_timer) {
  /* Portals pulse in size using the phase timer as an oscillator. */
  int pulse = (int)(4.0f + 3.0f * (float)((int)(phase_timer * 3.0f) %
                                          2)); /* simple 0/1 toggle */
  (void)pulse; /* used for visual effect below */

  for (int t = 0; t < lv->teleporter_count; t++) {
    const Teleporter *tp = &lv->teleporters[t];
    draw_circle(bb, tp->ax, tp->ay, tp->radius, COLOR_PORTAL_A);
    draw_circle_outline(bb, tp->ax, tp->ay, tp->radius + 2, COLOR_PORTAL_A);
    draw_circle(bb, tp->bx, tp->by, tp->radius, COLOR_PORTAL_B);
    draw_circle_outline(bb, tp->bx, tp->by, tp->radius + 2, COLOR_PORTAL_B);
  }
}

static void render_lines(const LineBitmap *lb, GameBackbuffer *bb) {
  /* Walk every set pixel and paint it.
   *
   * LineBitmap pixel value encoding:
   *   0        → empty (skip)
   *   1        → obstacle / cup wall  → COLOR_LINE
   *   255      → player-drawn brush   → COLOR_LINE
   *   2 .. 5   → baked settled grain, color index = value − 2
   *              GRAIN_WHITE(0)+2=2 → g_grain_colors[0] = COLOR_CREAM
   *              GRAIN_RED  (1)+2=3 → g_grain_colors[1] = COLOR_RED
   *              GRAIN_GREEN(2)+2=4 → g_grain_colors[2] = COLOR_GREEN
   *              GRAIN_ORANGE(3)+2=5→ g_grain_colors[3] = COLOR_ORANGE
   *
   * Previously everything non-zero rendered as COLOR_LINE, which made
   * settled grains appear as black blobs.  Now they keep their original color.
   *
   * Cost: one extra compare per non-zero pixel — negligible at 307,200 pixels.
   */
  int total = CANVAS_W * CANVAS_H;
  for (int i = 0; i < total; i++) {
    uint8_t v = lb->pixels[i];
    if (!v) continue;
    bb->pixels[i] = (v >= 2 && v <= 5)
                        ? g_grain_colors[v - 2]  /* baked grain: original color */
                        : COLOR_LINE;            /* drawn line or obstacle wall  */
  }
}

static void render_obstacles(const LevelDef *lv, GameBackbuffer *bb,
                             const LineBitmap *lb) {
  /* Obstacles were stamped into the same line bitmap.
   * We need to draw them with a different color to distinguish them from
   * player-drawn lines.  Walk each obstacle rect and check the bitmap. */
  for (int o = 0; o < lv->obstacle_count; o++) {
    const Obstacle *obs = &lv->obstacles[o];
    draw_rect(bb, obs->x, obs->y, obs->w, obs->h, COLOR_OBSTACLE);
  }
  (void)lb; /* used implicitly via render_lines */
}

static void render_grains(const GrainPool *p, GameBackbuffer *bb) {
  /* Each grain is rendered as a 2×2 pixel block to match the original
   * Sugar Sugar appearance (~2px circles).
   * The physics bitmap still uses 1-pixel precision for collision — the
   * larger visual size is purely cosmetic and does not affect gameplay. */
  for (int i = 0; i < p->count; i++) {
    if (!p->active[i])
      continue;
    int x = (int)p->x[i];
    int y = (int)p->y[i];
    uint32_t c = g_grain_colors[p->color[i]];
    /* Draw a 2×2 block; draw_pixel clips against canvas bounds. */
    draw_pixel(bb, x, y, c);
    draw_pixel(bb, x + 1, y, c);
    draw_pixel(bb, x, y + 1, c);
    draw_pixel(bb, x + 1, y + 1, c);
  }
}

static void render_emitters(const LevelDef *lv, GameBackbuffer *bb) {
  for (int e = 0; e < lv->emitter_count; e++) {
    /* Draw a small triangle / wedge indicating the spout. */
    int ex = lv->emitters[e].x;
    int ey = lv->emitters[e].y;
    draw_circle(bb, ex, ey, 5, COLOR_UI_TEXT);
  }
}

static void render_ui_buttons(const GameState *state, GameBackbuffer *bb) {
  /* ---- RESET button ----
   * Coordinates MUST match UI_RESET_X/W/BTN_Y/H defined above. */
  {
    int rx = UI_RESET_X, ry = UI_BTN_Y, rw = UI_RESET_W, rh = UI_BTN_H;
    uint32_t bg = state->reset_hover ? COLOR_BTN_HOVER : COLOR_BTN_NORMAL;
    draw_rect(bb, rx, ry, rw, rh, bg);
    draw_rect_outline(bb, rx, ry, rw, rh, COLOR_BTN_BORDER);
    /* Centre text vertically: (rh - 8) / 2 = (28-8)/2 = 10 */
    draw_text(bb, rx + 8, ry + (rh - 8) / 2, "RESET", COLOR_UI_TEXT);
  }

  /* ---- GRAVITY button (only when the level supports it) ---- */
  if (state->level.has_gravity_switch) {
    int gx = UI_GRAV_X, gy = UI_BTN_Y, gw = UI_GRAV_W, gh = UI_BTN_H;
    uint32_t g_bg = state->gravity_hover ? COLOR_BTN_HOVER : COLOR_BTN_NORMAL;
    draw_rect(bb, gx, gy, gw, gh, g_bg);
    draw_rect_outline(bb, gx, gy, gw, gh, COLOR_BTN_BORDER);
    const char *label = (state->gravity_sign > 0) ? "FLIP v" : "FLIP ^";
    draw_text(bb, gx + 8, gy + (gh - 8) / 2, label, COLOR_UI_TEXT);
  }

  /* ---- Level indicator (top-right corner, 2x scale) ---- */
  draw_text_scaled(bb, CANVAS_W - 120, 8, "LEVEL", COLOR_UI_TEXT, 2);
  draw_text_scaled(bb, CANVAS_W - 36, 8, "", COLOR_UI_TEXT,
                   2); /* placeholder */
  /* Draw level number separately so we can measure its width */
  {
    int lvl = state->current_level + 1;
    /* 2-digit number = 2 * 9 * 2 = 36px; 1-digit = 18px */
    int digits = (lvl >= 10) ? 2 : 1;
    int nx = CANVAS_W - digits * 18 - 8;
    /* draw_int uses draw_text internally at 1x; we call scaled directly */
    char buf[4];
    int i = 0;
    int tmp = lvl;
    if (tmp == 0) {
      buf[i++] = '0';
    } else {
      char t[4];
      int ti = 0;
      while (tmp > 0) {
        t[ti++] = (char)('0' + (tmp % 10));
        tmp /= 10;
      }
      while (ti > 0)
        buf[i++] = t[--ti];
    }
    buf[i] = '\0';
    draw_text_scaled(bb, nx, 8, buf, COLOR_UI_TEXT, 2);
  }
}

/* ===================================================================
 * PHASE RENDERERS
 * =================================================================== */

static void render_title(const GameState *state, GameBackbuffer *bb) {
  draw_rect(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_BG);

  /* ---- Title "SUGAR, SUGAR" at 3x scale (24px tall) ---- */
  {
    /* Measure: "SUGAR, SUGAR" = 12 chars × 9 × 3 = 324px wide */
    int tx = (CANVAS_W - 12 * 9 * 3) / 2;
    draw_text_scaled(bb, tx, 20, "SUGAR, SUGAR", COLOR_UI_TEXT, 3);
  }

  /* ---- Subtitle ---- */
  draw_text(bb, (CANVAS_W - 15 * 9) / 2, 76, "Select a level:", COLOR_UI_TEXT);

  /* ---- Thin separator line ---- */
  draw_rect(bb, 40, 95, CANVAS_W - 80, 1, COLOR_BTN_BORDER);

  /* ---- Level select grid ---- */
  int grid_x = TITLE_GRID_X;
  int grid_y = TITLE_GRID_Y;

  for (int i = 0; i < TOTAL_LEVELS; i++) {
    int col = i % TITLE_COLS, row = i / TITLE_COLS;
    int bx = grid_x + col * (TITLE_BTN_W + TITLE_BTN_GAP);
    int by = grid_y + row * (TITLE_BTN_H + TITLE_BTN_GAP);

    int locked = (i >= state->unlocked_count);
    uint32_t bg = locked                      ? GAME_RGB(200, 195, 190)
                  : (state->title_hover == i) ? COLOR_BTN_HOVER
                                              : COLOR_BTN_NORMAL;
    uint32_t text_col = locked ? GAME_RGB(155, 150, 145) : COLOR_UI_TEXT;

    draw_rect(bb, bx, by, TITLE_BTN_W, TITLE_BTN_H, bg);
    draw_rect_outline(bb, bx, by, TITLE_BTN_W, TITLE_BTN_H, COLOR_BTN_BORDER);

    /* Level number at 2× scale, centred in button */
    {
      int lvl = i + 1;
      int digits = (lvl >= 10) ? 2 : 1;
      int tx = bx + (TITLE_BTN_W - digits * 9 * 2) / 2;
      int ty = by + (TITLE_BTN_H - 8 * 2) / 2;
      char buf[4];
      int bi = 0;
      int tmp = lvl;
      {
        char t[4];
        int ti = 0;
        while (tmp > 0) {
          t[ti++] = (char)('0' + (tmp % 10));
          tmp /= 10;
        }
        while (ti > 0)
          buf[bi++] = t[--ti];
      }
      buf[bi] = '\0';
      draw_text_scaled(bb, tx, ty, buf, text_col, 2);
    }
  }

  /* ---- Footer ---- */
  draw_text(bb, 20, CANVAS_H - 18, "ESC: quit", COLOR_UI_TEXT);
  draw_text(bb, CANVAS_W - 18 * 9, CANVAS_H - 18, "Click a number to play",
            COLOR_UI_TEXT);
}

static void render_playing(const GameState *state, GameBackbuffer *bb) {
  /* 1. Background */
  draw_rect(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_BG);

  /* 2. Obstacles (baked rects, drawn over the background) */
  render_obstacles(&state->level, bb, &state->lines);

  /* 3. Color filter zones */
  render_filters(&state->level, bb);

  /* 4. Teleporter portals */
  render_teleporters(&state->level, bb, state->phase_timer);

  /* 5. Cups */
  render_cups(&state->level, bb);

  /* 6. Player-drawn + obstacle lines (overlay on top of background) */
  render_lines(&state->lines, bb);

  /* 7. Sugar grains */
  render_grains(&state->grains, bb);

  /* 8. Emitter spout indicators */
  render_emitters(&state->level, bb);

  /* 9. UI buttons */
  render_ui_buttons(state, bb);
}

static void render_level_complete(const GameState *state, GameBackbuffer *bb) {
  /* Show the last playing frame underneath the overlay. */
  render_playing(state, bb);

  /* Semi-transparent green overlay */
  draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_COMPLETE, 80);

  /* "LEVEL COMPLETE!" at 2× scale (16px tall).
   * Measure: 15 chars × 9 × 2 = 270px wide */
  int tx = (CANVAS_W - 15 * 9 * 2) / 2;
  int ty = CANVAS_H / 2 - 16;
  draw_text_scaled(bb, tx, ty, "LEVEL COMPLETE!", GAME_RGB(255, 255, 255), 2);
  draw_text_scaled(bb, tx + 2, ty + 20, "LEVEL COMPLETE!", COLOR_COMPLETE, 2);

  /* Hint (1× scale) */
  const char *hint = "CLICK or press ENTER for next level";
  int hx = (CANVAS_W -
            (int)(sizeof("CLICK or press ENTER for next level") - 1) * 9) /
           2;
  draw_text(bb, hx, ty + 50, hint, GAME_RGB(255, 255, 255));
}

static void render_freeplay(const GameState *state, GameBackbuffer *bb) {
  render_playing(state, bb);
  draw_text(bb, 10, 8, "FREEPLAY - ESC for menu", COLOR_UI_TEXT);
}
