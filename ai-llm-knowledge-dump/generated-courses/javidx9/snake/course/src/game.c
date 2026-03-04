/* ═══════════════════════════════════════════════════════════════════════════
 * game.c  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * The entire platform-independent game layer.  No X11.  No Raylib.  No OS calls.
 * All game update logic, all collision detection, and all rendering live here.
 * The platform (main_x11.c / main_raylib.c) calls snake_update() and
 * snake_render() each frame and uploads the resulting pixels to the GPU.
 *
 * READING ORDER
 * ─────────────
 * 1. Direction deltas (DX/DY)
 * 2. Drawing helpers (draw_cell)
 * 3. prepare_input_frame
 * 4. snake_spawn_food / snake_init
 * 5. snake_update (the most important function — game logic + GAME_PHASE dispatch)
 * 6. snake_render (layer-by-layer rendering)
 *
 * COURSE NOTE (draw utilities placement):
 *   In lessons 01–10, the drawing utilities (draw_rect, draw_char, etc.) live
 *   inline in this file for simplicity.  Lesson 12 refactors them into utils/.
 *   The final game.c (this file) uses #include "utils/draw-*.h" and calls
 *   those utilities — no inline drawing code here except draw_cell.
 *   See COURSE-BUILDER-IMPROVEMENTS.md.
 */

#include "game.h"              /* GameState, GameInput, SnakeBackbuffer, GAME_PHASE … */
#include "utils/draw-shapes.h" /* draw_rect, draw_rect_blend                         */
#include "utils/draw-text.h"   /* draw_char, draw_text, GLYPH_STRIDE                 */
#include "utils/math.h"        /* MIN, MAX, CLAMP                                    */

/* ══════ Direction Deltas ═══════════════════════════════════════════════════
 *
 * Indexed by SNAKE_DIR: UP=0, RIGHT=1, DOWN=2, LEFT=3.
 * DX[dir] = how much head.x changes each move step.
 * DY[dir] = how much head.y changes each move step.
 *
 * In screen coordinates: y increases DOWNWARD.
 * So SNAKE_DIR_UP means the snake moves toward y=0 (decreasing y).
 *
 * JS equivalent: const DELTAS = [{dx:0,dy:-1}, {dx:1,dy:0}, {dx:0,dy:1}, {dx:-1,dy:0}]
 */
static const int DX[4] = { 0,  1,  0, -1 };  /* UP, RIGHT, DOWN, LEFT */
static const int DY[4] = {-1,  0,  1,  0 };  /* UP, RIGHT, DOWN, LEFT */

/* ══════ draw_cell ══════════════════════════════════════════════════════════
 *
 * Draw one grid cell (inset by 1 px on all sides) at grid coordinates (col, row).
 *
 * WHY INSET BY 1 PX?
 *   The 1-pixel gap between cells makes the grid structure visible.  Without
 *   it, adjacent cells merge into an undifferentiated block.
 *
 * FIELD_Y_OFFSET — why add HEADER_ROWS * CELL_SIZE?
 *   The top HEADER_ROWS rows of the window are reserved for the score panel.
 *   Grid row 0 starts below that offset.  Without the offset, grid row 0
 *   would overlap the score panel.
 *
 * JS equivalent: ctx.fillRect(col*CELL_SIZE+1, offset+row*CELL_SIZE+1,
 *                              CELL_SIZE-2, CELL_SIZE-2)
 */
static void draw_cell(SnakeBackbuffer *bb, int col, int row, uint32_t color) {
    int field_y_offset = HEADER_ROWS * CELL_SIZE;
    draw_rect(bb,
              col  * CELL_SIZE + 1,
              field_y_offset + row * CELL_SIZE + 1,
              CELL_SIZE - 2,
              CELL_SIZE - 2,
              color);
}

/* ══════ prepare_input_frame ════════════════════════════════════════════════
 *
 * WHAT THIS FUNCTION DOES (two-arg upgrade over reference source):
 *
 *   Step 1: Copy ended_down from old_input → current_input.
 *           This preserves the "key is held" state across frames.
 *           Without this copy, a held key would appear "up" every frame after
 *           the initial press event — because X11 only sends one press event,
 *           not a continuous stream while the key is held.
 *
 *   Step 2: Reset half_transition_count in current_input to 0.
 *           This gives each frame a clean event count.
 *           Transitions that happened in the PREVIOUS frame don't carry over —
 *           only the current key state (ended_down) carries over.
 *
 * CALLING ORDER each frame:
 *   prepare_input_frame(old_input, current_input);  // ← call this first
 *   platform_get_input(current_input);              // ← then fill new events
 *
 * COURSE NOTE: The reference source uses a single-arg version:
 *   prepare_input_frame(GameInput *input) — only resets transitions, does NOT
 *   copy ended_down.  This works on Raylib (poll-based) but breaks on X11
 *   (event-based) for held keys.  Our two-arg version is correct for both.
 * See Lesson 03 and COURSE-BUILDER-IMPROVEMENTS.md.
 *
 * JS equivalent: copying the previous frame's keyState before clearing
 *   the per-frame event flags in a manual input manager.
 */
void prepare_input_frame(const GameInput *old_input, GameInput *current_input) {
    int i;
    for (i = 0; i < BUTTON_COUNT; i++) {
        /* Step 1: preserve key-held state from previous frame */
        current_input->buttons[i].ended_down = old_input->buttons[i].ended_down;
        /* Step 2: fresh event count for this frame */
        current_input->buttons[i].half_transition_count = 0;
    }
    /* restart and quit are edge-triggered (fire once per press), NOT held buttons.
     * They are reset to 0 here; platform_get_input() sets them to 1 when detected. */
    current_input->restart = 0;
    current_input->quit    = 0;
}

/* ══════ snake_spawn_food ════════════════════════════════════════════════════
 *
 * Place food at a random cell NOT occupied by the snake.
 *
 * ALGORITHM: retry loop.
 *   Pick random (food_x, food_y) inside the playable area (inside the walls).
 *   Scan all snake segments. If any segment is at that position, pick again.
 *   Repeat until a free cell is found.
 *
 * WHY SCAN EVERY SEGMENT?
 *   An O(N) scan on a 1200-element array is still microseconds — fast enough
 *   that there's no need for a more complex spatial data structure.
 *   This is the Handmade Hero principle: don't optimize prematurely.
 *
 * RANDOM SEEDING:
 *   srand() is called in snake_init() to seed the PRNG.
 *   rand() % N produces a value in [0, N-1].  Adding 1 to the lower bound
 *   and subtracting 2 from the width keeps food 1 cell away from walls.
 *
 * JS equivalent: Math.floor(Math.random() * (GRID_WIDTH - 2)) + 1
 */
void snake_spawn_food(GameState *s) {
    int ok, idx, rem;
    do {
        /* Pick a random cell inside the wall border */
        s->food_x = 1 + rand() % (GRID_WIDTH  - 2);
        s->food_y = 1 + rand() % (GRID_HEIGHT - 2);

        /* Walk ring buffer from tail to head, checking for collision */
        ok  = 1;
        idx = s->tail;
        rem = s->length;
        while (rem > 0) {
            if (s->segments[idx].x == s->food_x &&
                s->segments[idx].y == s->food_y) {
                ok = 0;  /* occupied — retry */
                break;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }
    } while (!ok);
}

/* ══════ snake_init ══════════════════════════════════════════════════════════
 *
 * Reset the game to a fresh starting state.
 * Called once at startup and again each time the player restarts.
 *
 * PRESERVE BEST SCORE AND AUDIO VOLUMES: save before memset, restore after.
 *   memset clears the ENTIRE struct to zero.  If we didn't save best_score
 *   first, the player's high score would be wiped on every restart.
 *   If we didn't save master_volume and sfx_volume, game_get_audio_samples
 *   would compute scale = 0.0 * 16000.0f = 0.0 → pure silence after restart.
 *   This pattern is the C equivalent of:
 *     JS: const best = state.best_score; state = {...initialState}; state.best_score = best;
 *
 * INITIAL SNAKE: 10 segments in a horizontal row, centred vertically.
 *   head = 9 (most-recent segment = rightmost)
 *   tail = 0 (oldest segment = leftmost)
 *   This matches how the ring buffer works: tail is the oldest element.
 */
void snake_init(GameState *s) {
    int   saved_best   = s->best_score;
    int   saved_sps    = s->audio.samples_per_second;
    float saved_master = s->audio.master_volume;
    float saved_sfx    = s->audio.sfx_volume;
    int i;

    /* Zero the entire struct — clears all fields cleanly.
     * JS equivalent: s = { ...defaultState }  (spread into new object) */
    memset(s, 0, sizeof(*s));

    /* Restore fields that must survive a restart.
     *
     * BUG NOTE: if master_volume / sfx_volume are NOT restored here, the
     * memset leaves them at 0.0 → scale = master_volume * 16000.0f = 0.0 →
     * game_get_audio_samples outputs silence every frame.  Always preserve
     * every volume field when resetting game state.
     */
    s->best_score               = saved_best;
    s->audio.samples_per_second = saved_sps;
    s->audio.master_volume      = saved_master;
    s->audio.sfx_volume         = saved_sfx;

    /* Place the initial 10-segment snake horizontally in the centre */
    s->head   = 9;
    s->tail   = 0;
    s->length = 10;
    for (i = 0; i < 10; i++) {
        s->segments[i].x = 10 + i;  /* columns 10..19 */
        s->segments[i].y = GRID_HEIGHT / 2;
    }

    s->direction      = SNAKE_DIR_RIGHT;
    s->next_direction = SNAKE_DIR_RIGHT;
    s->move_timer     = 0.0f;
    s->move_interval  = 0.15f;  /* 150 ms per step — comfortable starting speed */
    s->grow_pending   = 0;
    s->score          = 0;
    s->phase          = GAME_PHASE_PLAYING;

    /* Seed the PRNG for food placement.
     * JS: Math.random() is pre-seeded by the runtime; in C we must seed manually.
     * time(NULL) returns the current Unix timestamp — different each run. */
    srand((unsigned)time(NULL));
    snake_spawn_food(s);

    /* Start background music from the top of the melody. */
    game_music_start(&s->audio);
}

/* ══════ snake_update ════════════════════════════════════════════════════════
 *
 * Advance game logic by delta_time seconds.
 *
 * GAME_PHASE DISPATCH — the state machine
 * ─────────────────────────────────────────
 * Instead of a single monolithic function, we dispatch on state->phase.
 * Each case is a separate mode with its own input handling:
 *
 *   GAME_PHASE_PLAYING:   process turns, accumulate timer, move, check collisions
 *   GAME_PHASE_GAME_OVER: only check for restart input
 *
 * JS equivalent:
 *   switch(state.phase) {
 *     case GAME_PHASE.PLAYING:   updatePlaying(state, input, dt); break;
 *     case GAME_PHASE.GAME_OVER: updateGameOver(state, input);    break;
 *   }
 *
 * DELTA-TIME ACCUMULATOR — why subtract instead of zero?
 *   s->move_timer -= s->move_interval  (not: s->move_timer = 0)
 *
 *   If we zeroed the timer, any overshoot (the frame took 2.1 × move_interval)
 *   would be discarded — the snake skips a step.  Subtracting preserves the
 *   overshoot so the NEXT move fires proportionally sooner.
 *   Result: movement speed is accurate independent of frame rate.
 */
void snake_update(GameState *s, const GameInput *input, float delta_time) {
    int new_x, new_y, idx, rem;
    SNAKE_DIR turned;

    switch (s->phase) {

    /* ── GAME_PHASE_PLAYING ─────────────────────────────────────────────── */
    case GAME_PHASE_PLAYING: {

        /* ── Process direction input ──────────────────────────────────────
         * "Just pressed this frame" = ended_down=1 AND half_transition_count>0.
         * Using just ended_down would fire every frame while held (not desirable).
         * Using half_transition_count>0 ensures each turn fires exactly once.
         *
         * JS: button.isDown && button.justPressed  (manual event tracking) */
        if (input->turn_right.ended_down &&
            input->turn_right.half_transition_count > 0) {
            turned = (SNAKE_DIR)((s->direction + 1) % 4);  /* CW */
            /* U-turn guard: block 180° reversal.
             * Opposite direction = (dir + 2) % 4.
             * Applying the opposite would move the head into the second segment. */
            if (turned != (SNAKE_DIR)((s->direction + 2) % 4))
                s->next_direction = turned;
        }
        if (input->turn_left.ended_down &&
            input->turn_left.half_transition_count > 0) {
            turned = (SNAKE_DIR)((s->direction + 3) % 4);  /* CCW */
            if (turned != (SNAKE_DIR)((s->direction + 2) % 4))
                s->next_direction = turned;
        }

        /* ── Accumulate move timer ────────────────────────────────────────
         * We move only when enough time has accumulated.
         * This decouples game speed from frame rate. */
        s->move_timer += delta_time;
        if (s->move_timer < s->move_interval) break;  /* not yet time to move */
        s->move_timer -= s->move_interval;             /* keep overshoot */

        /* Commit the queued direction */
        s->direction = s->next_direction;

        /* Compute the new head position */
        new_x = s->segments[s->head].x + DX[s->direction];
        new_y = s->segments[s->head].y + DY[s->direction];

        /* ── Wall collision ───────────────────────────────────────────────
         * Column 0 and GRID_WIDTH-1 are wall cells; same for row 0 and GRID_HEIGHT-1.
         * The snake is not allowed to enter those cells. */
        if (new_x < 1 || new_x >= GRID_WIDTH - 1 ||
            new_y < 0 || new_y >= GRID_HEIGHT - 1) {
            if (s->score > s->best_score) s->best_score = s->score;
            s->phase = GAME_PHASE_GAME_OVER;
            game_music_stop(&s->audio);
            game_play_sound(&s->audio, SOUND_GAME_OVER);
            break;
        }

        /* ── Self-collision ───────────────────────────────────────────────
         * Walk ring buffer from tail to head.  If new head position matches
         * any existing segment, it's a self-collision → game over.
         *
         * COMMON BUG: starting at head (not tail) and including all `length`
         * elements would scan the OLD head position (which is fine) but could
         * also scan one slot PAST the valid data.  Scan tail..head is correct. */
        idx = s->tail;
        rem = s->length;
        while (rem > 0) {
            if (s->segments[idx].x == new_x && s->segments[idx].y == new_y) {
                if (s->score > s->best_score) s->best_score = s->score;
                s->phase = GAME_PHASE_GAME_OVER;
                game_music_stop(&s->audio);
                game_play_sound(&s->audio, SOUND_GAME_OVER);
                return;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }

        /* ── Advance head ─────────────────────────────────────────────────
         * Write the new position at (head+1)%MAX_SNAKE and update head.
         * Length increases by 1 here; we may remove a tail segment below. */
        s->head = (s->head + 1) % MAX_SNAKE;
        s->segments[s->head].x = new_x;
        s->segments[s->head].y = new_y;
        s->length++;

        /* ── Food eaten ───────────────────────────────────────────────────
         * Check AFTER advancing head — the new head position is `new_x/new_y`. */
        if (new_x == s->food_x && new_y == s->food_y) {
            s->score++;
            s->grow_pending += 5;  /* defer tail advance for 5 ticks */

            /* Speed up every 3 points, but don't go below floor */
            if (s->score % 3 == 0 && s->move_interval > 0.05f)
                s->move_interval -= 0.01f;

            /* Spatial audio pan: map food_x in [1, GRID_WIDTH-2] to [-1, +1].
             * food_x = 1           → pan ≈ -1.0  (full left)
             * food_x = GRID_WIDTH/2 → pan ≈  0.0  (centre)
             * food_x = GRID_WIDTH-2 → pan ≈ +1.0  (full right)
             *
             * Formula: pan = (food_x - 1) / (GRID_WIDTH - 3) * 2 - 1
             * We use a slightly simplified version mapped over the full grid width.
             * The difference is imperceptible; the spatial cue is clearly audible. */
            {
                float pan = ((float)s->food_x / (float)(GRID_WIDTH - 1)) * 2.0f - 1.0f;
                game_play_sound_at(&s->audio, SOUND_FOOD_EATEN, pan);
            }

            snake_spawn_food(s);
        }

        /* ── Tail advance (unless growing) ───────────────────────────────
         * grow_pending > 0: skip tail advance this tick (snake grows by 1).
         * grow_pending == 0: advance tail and decrease length (constant length). */
        if (s->grow_pending > 0) {
            s->grow_pending--;
            /* Subtle tactile feedback: one short blip per segment added.
             * Plays on each of the 5 growth steps after eating — gives the
             * player a sense of the snake's body extending. */
            game_play_sound(&s->audio, SOUND_GROW);
        } else {
            s->tail = (s->tail + 1) % MAX_SNAKE;
            s->length--;
        }
        break;
    }

    /* ── GAME_PHASE_GAME_OVER ───────────────────────────────────────────── */
    case GAME_PHASE_GAME_OVER: {
        /* Only restart input is accepted in game-over state */
        if (input->restart) {
            /* snake_init resets phase to GAME_PHASE_PLAYING, starts music.
             * SOUND_RESTART is queued AFTER snake_init so the audio state is
             * fully initialised before we add the jingle to the SFX pool.
             * (Queuing it before snake_init would have it wiped by the memset.) */
            snake_init(s);
            game_play_sound(&s->audio, SOUND_RESTART);
        }
        break;
    }

    } /* end switch (s->phase) */
}

/* ══════ snake_render ════════════════════════════════════════════════════════
 *
 * Draw the full frame into the backbuffer.
 * Layer order (painter's algorithm — later layers paint over earlier ones):
 *
 *   1. Clear to black                 (background)
 *   2. Header background + separator  (score panel)
 *   3. Header text (SNAKE, SCORE, BEST)
 *   4. Wall cells                     (play-field border)
 *   5. Food cell
 *   6. Snake body                     (all segments except head)
 *   7. Snake head                     (drawn last so it's always on top)
 *   8. Game-over overlay              (conditional: semi-transparent + text)
 *
 * WHY THIS ORDER?
 *   If food were drawn after the snake, a snake segment on the food cell would
 *   be hidden.  The wall cells are drawn before the snake so wall/body overlap
 *   is visible (the snake physically can't overlap walls, but the order matters
 *   for the food-on-wall edge case during spawning).
 *
 * JS equivalent: canvas drawing operations — whatever you draw last appears on top.
 */
void snake_render(const GameState *s, SnakeBackbuffer *bb) {
    char buf[64];
    int col, row, idx, rem;
    uint32_t body_color, head_color;

    /* ── 1. Clear to black ────────────────────────────────────────────── */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* ── 2. Header background ─────────────────────────────────────────── */
    draw_rect(bb, 0, 0, bb->width, HEADER_ROWS * CELL_SIZE, COLOR_DARK_GRAY);
    /* Separator line between header and play field */
    draw_rect(bb, 0, (HEADER_ROWS - 1) * CELL_SIZE, bb->width, 2, COLOR_GREEN);

    /* ── 3. Header text ───────────────────────────────────────────────── */
    {
        /* Centre "SNAKE" title at scale 2 (each glyph is 16×16 px at scale 2) */
        int title_chars = 5;  /* strlen("SNAKE") */
        int title_w     = title_chars * GLYPH_STRIDE * 2;
        int title_x     = (bb->width - title_w) / 2;
        int text_y      = (HEADER_ROWS * CELL_SIZE - GLYPH_HEIGHT * 2) / 2;

        draw_text(bb, title_x, text_y, "SNAKE", COLOR_GREEN, 2);

        /* Score on the left */
        snprintf(buf, sizeof(buf), "SCORE:%d", s->score);
        draw_text(bb, 8, text_y, buf, COLOR_WHITE, 2);

        /* Best score on the right */
        snprintf(buf, sizeof(buf), "BEST:%d", s->best_score);
        {
            int bw = (int)strlen(buf) * GLYPH_STRIDE * 2;
            draw_text(bb, bb->width - bw - 8, text_y, buf, COLOR_YELLOW, 2);
        }
    }

    /* ── 4. Border walls ──────────────────────────────────────────────── */
    for (row = 0; row < GRID_HEIGHT; row++) {
        draw_cell(bb, 0,            row, COLOR_GREEN);  /* left wall  */
        draw_cell(bb, GRID_WIDTH-1, row, COLOR_GREEN);  /* right wall */
    }
    for (col = 0; col < GRID_WIDTH; col++) {
        draw_cell(bb, col, GRID_HEIGHT-1, COLOR_GREEN); /* bottom wall */
    }

    /* ── 5. Food ──────────────────────────────────────────────────────── */
    draw_cell(bb, s->food_x, s->food_y, COLOR_RED);

    /* ── 6 & 7. Snake body and head ───────────────────────────────────── */
    /* Dead snake: whole body turns dark red so the player knows it's over */
    body_color = (s->phase == GAME_PHASE_GAME_OVER) ? COLOR_DARK_RED : COLOR_YELLOW;
    head_color = (s->phase == GAME_PHASE_GAME_OVER) ? COLOR_DARK_RED : COLOR_WHITE;

    /* Draw body: tail..head-1 (all segments except head) */
    idx = s->tail;
    rem = s->length - 1;
    while (rem > 0) {
        draw_cell(bb, s->segments[idx].x, s->segments[idx].y, body_color);
        idx = (idx + 1) % MAX_SNAKE;
        rem--;
    }
    /* Draw head: always last so it's always on top */
    draw_cell(bb, s->segments[s->head].x, s->segments[s->head].y, head_color);

    /* ── 8. Game-over overlay ─────────────────────────────────────────── */
    if (s->phase == GAME_PHASE_GAME_OVER) {
        int field_y = HEADER_ROWS * CELL_SIZE;
        int field_w = GRID_WIDTH  * CELL_SIZE;
        int field_h = GRID_HEIGHT * CELL_SIZE;
        int cx      = field_w / 2;
        int cy      = field_y + field_h / 2;

        /* Semi-transparent black overlay — alpha=180 means ~70% opaque.
         * draw_rect_blend applies the alpha blend per-pixel. */
        draw_rect_blend(bb, 0, field_y, field_w, field_h, GAME_RGBA(0,0,0,180));

        /* Red border: 4 thin rectangles forming a frame */
        draw_rect(bb, 0,           field_y,              field_w, 4, COLOR_RED);
        draw_rect(bb, 0,           field_y + field_h - 4, field_w, 4, COLOR_RED);
        draw_rect(bb, 0,           field_y,              4, field_h, COLOR_RED);
        draw_rect(bb, field_w - 4, field_y,              4, field_h, COLOR_RED);

        /* "GAME OVER" at scale 3 (24×24 px glyphs) */
        {
            int scale   = 3;
            int str_len = 9;  /* strlen("GAME OVER") */
            int str_w   = str_len * GLYPH_STRIDE * scale;
            draw_text(bb, cx - str_w / 2, cy - 36, "GAME OVER", COLOR_RED, scale);
        }

        /* Score at scale 2 */
        snprintf(buf, sizeof(buf), "SCORE %d", s->score);
        {
            int sw = (int)strlen(buf) * GLYPH_STRIDE * 2;
            draw_text(bb, cx - sw / 2, cy + 6, buf, COLOR_WHITE, 2);
        }

        /* Restart hint */
        {
            int hw = 9 * GLYPH_STRIDE * 2;  /* "R RESTART" = 9 chars */
            draw_text(bb, cx - hw / 2, cy + 30, "R RESTART", COLOR_WHITE, 2);
        }

        /* Quit hint */
        {
            int qw = 6 * GLYPH_STRIDE * 2;  /* "Q QUIT" = 6 chars */
            draw_text(bb, cx - qw / 2, cy + 54, "Q QUIT", COLOR_GRAY, 2);
        }
    }
}
