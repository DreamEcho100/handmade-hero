/* =============================================================================
 * game.c — Frogger Game Logic + Rendering
 * =============================================================================
 *
 * ALL game logic AND rendering live here — no X11, no Raylib, no OS calls.
 * Reads from GameInput, writes to GameState (game_update) or Backbuffer
 * (game_render).
 *
 * DOD PRINCIPLE:
 *   Lane data is split into two separate arrays:
 *     lane_speeds[]     — floats only; 10 × 4 = 40 bytes (one cache line).
 *     lane_patterns[][] — char grids; touched only during rendering pass.
 *   game_update reads lane_speeds for river drift and danger writes.
 *   game_render reads both for drawing.  Keeping them separate avoids
 *   cache pollution between the two hot inner loops.
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"

#include <math.h>    /* fabsf */

/* ═══════════════════════════════════════════════════════════════════════════
 * LANE DATA (DOD layout)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Two parallel arrays — NOT a Lane struct.  This is Data-Oriented Design:
 *   lane_speeds[]     — only the floats; one cache line, read every tick.
 *   lane_patterns[][] — only the chars; read only during render.
 *
 * JS analogy: { ids: [], names: [] } vs [{ id, name }]
 *   The struct-of-arrays form lets the CPU work on each field independently.
 *
 * Lane map (top = row 0):
 *   0  home row  (wall + homes, stationary)
 *   1  river     (logs + water, moves LEFT  at 3)
 *   2  river     (logs + water, moves RIGHT at 3)
 *   3  river     (logs + water, moves RIGHT at 2)
 *   4  pavement  (safe middle strip, stationary)
 *   5  road      (bus,  moves LEFT  at 3)
 *   6  road      (car2, moves RIGHT at 3)
 *   7  road      (car1, moves LEFT  at 4)
 *   8  road      (car2, moves RIGHT at 2)
 *   9  pavement  (safe start row, stationary)
 */
static const float lane_speeds[NUM_LANES] = {
     0.0f,  /* 0 home row    */
    -3.0f,  /* 1 river left  */
     3.0f,  /* 2 river right */
     2.0f,  /* 3 river right */
     0.0f,  /* 4 pavement    */
    -3.0f,  /* 5 road left   */
     3.0f,  /* 6 road right  */
    -4.0f,  /* 7 road left   */
     2.0f,  /* 8 road right  */
     0.0f,  /* 9 pavement    */
};

/* Each row is exactly LANE_PATTERN_LEN (64) characters.
 * Tile characters:
 *   w = wall (deadly)           h = home (safe goal)
 *   , = water (deadly)          j/l/k = log start/mid/end (safe)
 *   p = pavement (safe)
 *   . = road (safe ground)      a/s/d/f = bus segments (deadly)
 *   z/x = car1 back/front       t/y = car2 back/front  (all deadly)    */
static const char lane_patterns[NUM_LANES][LANE_PATTERN_LEN + 1] = {
    "wwwhhwwwhhwwwhhwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww",
    ",,,jllk,,jllllk,,,,,,,jllk,,,,,jk,,,jlllk,,,,jllllk,,,,jlllk,,,,",
    ",,,,jllk,,,,,jllk,,,,jllk,,,,,,,,,jllk,,,,,jk,,,,,,jllllk,,,,,,,",
    ",,jlk,,,,,jlk,,,,,jk,,,,,jlk,,,jlk,,,,jk,,,,jllk,,,,jk,,,,,,jk,,",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
    "....asdf.......asdf....asdf..........asdf........asdf....asdf....",
    ".....ty..ty....ty....ty.....ty........ty..ty.ty......ty.......ty.",
    "..zx.....zx.........zx..zx........zx...zx...zx....zx...zx...zx..",
    "..ty.....ty.......ty.....ty......ty..ty.ty.......ty....ty........",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
};

/* ─── Tile helpers ─────────────────────────────────────────────────────── */

/* Returns 1 if the tile character is safe to stand/ride on. */
static int tile_is_safe(char c) {
    return (c == '.' || c == 'j' || c == 'l' || c == 'k' ||
            c == 'p' || c == 'h');
}

/* Maps a tile character to a sprite ID and sets *src_x to the
   appropriate cell offset within that sprite sheet.                      */
static int tile_to_sprite(char c, int *src_x) {
    *src_x = 0;
    switch (c) {
        case 'w': return SPR_WALL;
        case 'h': return SPR_HOME;
        case ',': return SPR_WATER;
        case 'p': return SPR_PAVEMENT;
        case 'j': *src_x =  0; return SPR_LOG;
        case 'l': *src_x =  8; return SPR_LOG;
        case 'k': *src_x = 16; return SPR_LOG;
        case 'z': *src_x =  0; return SPR_CAR1;
        case 'x': *src_x =  8; return SPR_CAR1;
        case 't': *src_x =  0; return SPR_CAR2;
        case 'y': *src_x =  8; return SPR_CAR2;
        case 'a': *src_x =  0; return SPR_BUS;
        case 's': *src_x =  8; return SPR_BUS;
        case 'd': *src_x = 16; return SPR_BUS;
        case 'f': *src_x = 24; return SPR_BUS;
        default:  return -1;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SPRITE LOADER
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * .spr binary layout:
 *   int32_t width;
 *   int32_t height;
 *   int16_t colors[width * height];   FG in low nibble, BG in high nibble
 *   int16_t glyphs[width * height];   0x2588 = solid, 0x0020 = transparent
 *
 * JS analogy:  fetch('frog.spr').then(r => r.arrayBuffer()).then(buf => {
 *   const view = new DataView(buf);
 *   const w = view.getInt32(0, true);
 *   const h = view.getInt32(4, true);
 *   // colors start at byte 8, each int16_t = 2 bytes
 * });
 */
static int sprite_load(SpriteBank *bank, int spr_id, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FATAL: cannot open sprite '%s'\n", path);
        return 0;
    }
    int32_t w, h;
    fread(&w, sizeof(int32_t), 1, f);
    fread(&h, sizeof(int32_t), 1, f);
    int offset = bank->offsets[spr_id];
    int count  = (int)(w * h);
    if (offset + count > SPR_POOL_CELLS) {
        fprintf(stderr, "FATAL: sprite pool overflow for '%s'\n", path);
        fclose(f);
        return 0;
    }
    fread(bank->colors + offset, sizeof(int16_t), (size_t)count, f);
    fread(bank->glyphs + offset, sizeof(int16_t), (size_t)count, f);
    bank->widths [spr_id] = (int)w;
    bank->heights[spr_id] = (int)h;
    fclose(f);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * prepare_input_frame
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Called at the start of each frame BEFORE platform_get_input().
 *
 * WHY TWO ARGUMENTS?
 *   old_input holds last frame's ended_down states.  Copying them into
 *   current_input before clearing half_transition_count ensures that a
 *   key held across frames still reads as held (ended_down stays 1) even
 *   if no new event arrives this frame (important for X11 event-based input).
 *
 * JS analogy:
 *   // Preserve "is held" state; clear "was just pressed" flag.
 *   Object.assign(current, old);
 *   current.justPressed = false;
 */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
    int btn;
    for (btn = 0; btn < BUTTON_COUNT; btn++) {
        current_input->buttons[btn].ended_down =
            old_input->buttons[btn].ended_down;
        current_input->buttons[btn].half_transition_count = 0;
    }
    current_input->quit    = 0;
    current_input->restart = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_init
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Load sprites and set initial game state.
 * Preserves best_score and audio config (samples_per_second, volumes)
 * across the memset — those must survive game restart.
 */
void game_init(GameState *state, const char *assets_dir) {
    /* Save fields that must survive memset */
    int   best_score         = state->best_score;
    int   samples_per_second = state->audio.samples_per_second;
    float master_volume      = state->audio.master_volume;
    float sfx_volume         = state->audio.sfx_volume;
    float music_volume       = state->audio.music_volume;
    float ambient_volume     = state->audio.ambient_volume;

    memset(state, 0, sizeof(GameState));

    /* Restore preserved fields */
    state->best_score               = best_score;
    state->audio.samples_per_second = samples_per_second;
    state->audio.master_volume      = master_volume;
    state->audio.sfx_volume         = (sfx_volume   > 0.0f) ? sfx_volume   : 0.75f;
    state->audio.music_volume       = (music_volume  > 0.0f) ? music_volume  : 0.40f;
    state->audio.ambient_volume     = (ambient_volume > 0.0f) ? ambient_volume : 0.30f;

    /* Store assets_dir so game_update() can restart via game_init() */
    {
        int ai = 0;
        while (assets_dir[ai] && ai < 255) {
            state->assets_dir[ai] = assets_dir[ai];
            ai++;
        }
        state->assets_dir[ai] = '\0';
    }

    /* Frog starts at tile (8, 9) — centre of the bottom safe strip */
    state->frog_x = 8.0f;
    state->frog_y = 9.0f;
    state->phase  = PHASE_PLAYING;
    state->lives  = 3;

    /* Pre-compute sprite pool offsets.
       spr_cells[i] = total cells for sprite i (width * height). */
    static const int spr_cells[NUM_SPRITES] = {
        8*8,   /* SPR_FROG     (8 cells wide × 8 tall)  */
        8*8,   /* SPR_WATER    */
        8*8,   /* SPR_PAVEMENT */
        8*8,   /* SPR_WALL     */
        8*8,   /* SPR_HOME     */
        24*8,  /* SPR_LOG      (start + 8 mid + end = 24 cells) */
        16*8,  /* SPR_CAR1     (2 tiles × 8 cells)      */
        16*8,  /* SPR_CAR2     */
        32*8,  /* SPR_BUS      (4 tiles × 8 cells)      */
    };
    int i, offset = 0;
    for (i = 0; i < NUM_SPRITES; i++) {
        state->sprites.offsets[i] = offset;
        offset += spr_cells[i];
    }

    /* Load each sprite from the assets directory */
    static const struct { int id; const char *filename; } spr_files[NUM_SPRITES] = {
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
    char path[256];
    for (i = 0; i < NUM_SPRITES; i++) {
        snprintf(path, sizeof(path), "%s/%s", assets_dir, spr_files[i].filename);
        if (!sprite_load(&state->sprites, spr_files[i].id, path)) {
            fprintf(stderr, "Failed to load: %s\n", path);
            exit(1);
        }
    }

    /* Start ambient loops at zero volume (game_update_ambients() fades them in) */
    if (state->audio.samples_per_second > 0) {
        game_play_looping(state, SOUND_AMBIENT_TRAFFIC, 0.0f, 0.0f);
        game_play_looping(state, SOUND_AMBIENT_WATER,   0.0f, 0.0f);
        game_play_sound(state, SOUND_LEVEL_START, 0.0f);
        state->audio.music_playing  = 1;
        state->audio.music_note_idx = -1;  /* will advance to 0 on first update */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SPATIAL AUDIO PAN HELPER
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Converts frog tile X position to a stereo pan value in [-1, +1].
 *   pan = (frog_x / max_tile_x) * 2.0f - 1.0f
 *   gain_l = (pan <= 0) ? 1.0 : (1.0 - pan)
 *   gain_r = (pan >= 0) ? 1.0 : (1.0 + pan)
 *
 * JS analogy:  stereoPanner.pan.value = pan;
 */
static float frog_pan(float frog_x) {
    float max_x = (float)(SCREEN_CELLS_W - TILE_CELLS);
    float pan   = (frog_x / max_x) * 2.0f - 1.0f;
    if (pan < -1.0f) pan = -1.0f;
    if (pan >  1.0f) pan =  1.0f;
    return pan;
}

/* ─── Trigger sound ─────────────────────────────────────────────────────── */
static void trigger_sound(GameState *state, SOUND_ID id, float pan) {
    game_play_sound(state, id, pan);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_update
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Pure game logic — no drawing, no platform calls.
 *
 * Steps:
 *   1. Cap dt to avoid huge jumps (debugger pause, tab switch)
 *   2. Accumulate game time for lane_scroll
 *   3. Handle per-phase logic:
 *      PHASE_DEAD:    countdown dead_timer; respawn when it reaches 0
 *      PHASE_WIN:     wait for restart key; reset game on press
 *      PHASE_PLAYING: input, river drift, danger rebuild, collision, win check
 */
void game_update(GameState *state, const GameInput *input, float dt) {
    if (dt > 0.1f) dt = 0.1f;

    /* Restart request (any phase) — call game_init for a clean restart */
    if (input->restart) {
        game_init(state, state->assets_dir);
        return;
    }

    /* Per-frame audio: music sequencer + ambient distance modulation */
    game_music_update(state, dt);
    game_update_ambients(state);

    /* ── PHASE_DEAD ─────────────────────────────────────────── */
    if (state->phase == PHASE_DEAD) {
        state->dead_timer -= dt;
        if (state->dead_timer <= 0.0f) {
            if (state->lives <= 0) {
                /* Game over — stay in DEAD with 0 lives, wait for restart */
                state->dead_timer = 0.0f;
            } else {
                state->frog_x = 8.0f;
                state->frog_y = 9.0f;
                state->phase  = PHASE_PLAYING;
            }
        }
        return;
    }

    /* ── PHASE_WIN ──────────────────────────────────────────── */
    if (state->phase == PHASE_WIN) {
        return;  /* wait for restart key from platform */
    }

    /* ── PHASE_PLAYING ──────────────────────────────────────── */
    state->time += dt;

    /* Input: one hop per "just pressed" button.
     * "Just pressed" = ended_down AND at least one transition this frame.
     * This prevents held-key multi-hop — frog hops exactly once per key press.
     * JS analogy:  if (event.type === 'keydown' && !event.repeat) { hop(); }  */
    float hop_pan = frog_pan(state->frog_x);

#define JUST_PRESSED(btn) ((btn).ended_down && (btn).half_transition_count > 0)

    if (JUST_PRESSED(input->move_up)) {
        state->frog_y -= 1.0f;
        trigger_sound(state, SOUND_HOP, hop_pan);
        state->score += 10;
    }
    if (JUST_PRESSED(input->move_down)) {
        state->frog_y += 1.0f;
        trigger_sound(state, SOUND_HOP, hop_pan);
    }
    if (JUST_PRESSED(input->move_left)) {
        state->frog_x -= 1.0f;
        trigger_sound(state, SOUND_HOP, hop_pan);
    }
    if (JUST_PRESSED(input->move_right)) {
        state->frog_x += 1.0f;
        trigger_sound(state, SOUND_HOP, hop_pan);
    }

    /* River: push frog sideways on log rows (1–3).
     * The frog drifts with the current — if standing on a log it moves
     * horizontally even without input.                                    */
    {
        int fy = (int)state->frog_y;
        if (fy >= 1 && fy <= 3)
            state->frog_x -= lane_speeds[fy] * dt;
    }

    /* ── Rebuild danger buffer ──────────────────────────────── */
    /* Start with everything unsafe (1), then mark safe cells (0). */
    memset(state->danger, 1, SCREEN_CELLS_W * SCREEN_CELLS_H);

    {
        int y, i, dy, cx, cx_start;
        for (y = 0; y < NUM_LANES; y++) {
            int tile_start, px_offset;
            lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
            int cell_offset = px_offset / CELL_PX;

            for (i = 0; i < LANE_WIDTH; i++) {
                char c    = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
                int  safe = tile_is_safe(c);
                cx_start  = (-1 + i) * TILE_CELLS - cell_offset;

                for (dy = 0; dy < TILE_CELLS; dy++) {
                    int row = y * TILE_CELLS + dy;
                    for (cx = cx_start; cx < cx_start + TILE_CELLS; cx++) {
                        if (cx >= 0 && cx < SCREEN_CELLS_W)
                            state->danger[row * SCREEN_CELLS_W + cx] =
                                (uint8_t)(!safe);
                    }
                }
            }
        }
    }

    /* ── Collision: center-cell check ───────────────────────── */
    /* Use the frog sprite's center pixel to determine its cell.
     * Center-cell collision is immune to log-edge flickering.             */
    {
        int frog_px_x = (int)(state->frog_x * (float)TILE_PX);
        int frog_px_y = (int)(state->frog_y * (float)TILE_PX);

        /* Screen boundary death */
        if (frog_px_x < 0 || frog_px_y < 0 ||
            frog_px_x + TILE_PX > SCREEN_W ||
            frog_px_y + TILE_PX > SCREEN_H) {
            trigger_sound(state, SOUND_SPLASH, frog_pan(state->frog_x));
            state->lives--;
            state->phase      = PHASE_DEAD;
            state->dead_timer = 0.5f;
            return;
        }

        int coll_cx = (frog_px_x + TILE_PX / 2) / CELL_PX;
        int coll_cy = (frog_px_y + TILE_PX / 2) / CELL_PX;

        if (state->danger[coll_cy * SCREEN_CELLS_W + coll_cx]) {
            /* Road lanes 5-8 → squash; river lanes 1-3 and wall → splash */
            int lane = (int)state->frog_y;
            int on_road = (lane >= 5 && lane <= 8);
            trigger_sound(state, on_road ? SOUND_SQUASH : SOUND_SPLASH,
                          frog_pan(state->frog_x));
            state->lives--;
            state->phase      = PHASE_DEAD;
            state->dead_timer = 0.5f;
            if (state->lives <= 0) {
                trigger_sound(state, SOUND_GAME_OVER, 0.0f);
                state->dead_timer = 2.0f;
            }
            return;
        }

        /* ── Win detection ───────────────────────────────────── */
        if ((int)state->frog_y == 0) {
            int pattern_pos = (int)(state->frog_x + 0.5f) % LANE_PATTERN_LEN;
            if (pattern_pos < 0) pattern_pos += LANE_PATTERN_LEN;
            char c = lane_patterns[0][pattern_pos];
            if (c == 'h') {
                state->homes_reached++;
                state->score += 50;
                trigger_sound(state, SOUND_HOME_REACHED, frog_pan(state->frog_x));
                state->frog_x = 8.0f;
                state->frog_y = 9.0f;
                if (state->homes_reached >= 5) {
                    if (state->score > state->best_score)
                        state->best_score = state->score;
                    state->phase = PHASE_WIN;
                    trigger_sound(state, SOUND_WIN_JINGLE, 0.0f);
                    state->audio.music_playing = 0;  /* stop music on win */
                }
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_render
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Writes the complete frame into bb->pixels.
 * Rendering order (painter's algorithm — back to front):
 *   1. Clear to black
 *   2. Draw all lanes (background + sprites)
 *   3. Draw frog (with flash on death)
 *   4. HUD (score, lives, homes)
 *   5. Phase overlays (DEAD flash, WIN / GAME OVER screen)
 */
void game_render(Backbuffer *bb, const GameState *state) {
    int y, i;
    char buf[64];

    /* 1. Clear */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* 2. Draw lanes */
    for (y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
        int dest_py = y * TILE_PX;

        for (i = 0; i < LANE_WIDTH; i++) {
            char c   = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
            int src_x = 0;
            int spr  = tile_to_sprite(c, &src_x);
            int dest_px = (-1 + i) * TILE_PX - px_offset;

            if (spr >= 0) {
                int  off     = state->sprites.offsets[spr];
                int  sheet_w = state->sprites.widths[spr];
                draw_sprite_partial(bb,
                    state->sprites.colors + off,
                    state->sprites.glyphs + off,
                    CONSOLE_PALETTE, sheet_w,
                    src_x, 0, TILE_CELLS, TILE_CELLS,
                    dest_px, dest_py);
            }
            /* '.' (road) is left as cleared black */
        }
    }

    /* 3. Draw frog (with death flash) */
    {
        int show_frog = 1;
        if (state->phase == PHASE_DEAD) {
            int flash = (int)(state->dead_timer / 0.05f);
            show_frog = (flash % 2 == 0);
        }
        if (show_frog) {
            int frog_px = (int)(state->frog_x * (float)TILE_PX);
            int frog_py = (int)(state->frog_y * (float)TILE_PX);
            int off     = state->sprites.offsets[SPR_FROG];
            int sheet_w = state->sprites.widths[SPR_FROG];
            draw_sprite_partial(bb,
                state->sprites.colors + off,
                state->sprites.glyphs + off,
                CONSOLE_PALETTE, sheet_w,
                0, 0,
                state->sprites.widths [SPR_FROG],
                state->sprites.heights[SPR_FROG],
                frog_px, frog_py);
        }
    }

    /* 4. HUD */
    snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
    draw_text(bb, 8, bb->height - 20, buf, COLOR_WHITE);

    snprintf(buf, sizeof(buf), "LIVES: %d", state->lives);
    draw_text(bb, 200, bb->height - 20, buf, COLOR_YELLOW);

    snprintf(buf, sizeof(buf), "HOMES: %d/5", state->homes_reached);
    draw_text(bb, 380, bb->height - 20, buf, COLOR_GREEN);

    if (state->best_score > 0) {
        snprintf(buf, sizeof(buf), "BEST: %d", state->best_score);
        draw_text(bb, 560, bb->height - 20, buf, COLOR_CYAN);
    }

    /* 5. Phase overlays */
    if (state->phase == PHASE_DEAD && state->lives <= 0) {
        /* Game over overlay */
        draw_rect_blend(bb, 0, 0, bb->width, bb->height, COLOR_OVERLAY_DIM);
        draw_text(bb, bb->width / 2 - 4 * GLYPH_W,
                      bb->height / 2 - GLYPH_H,
                  "GAME OVER", COLOR_RED);
        draw_text(bb, bb->width / 2 - 7 * GLYPH_W,
                      bb->height / 2 + GLYPH_H + 4,
                  "PRESS R TO RESTART", COLOR_WHITE);
    } else if (state->phase == PHASE_WIN) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height, COLOR_OVERLAY_DIM);
        draw_text(bb, bb->width / 2 - 4 * GLYPH_W,
                      bb->height / 2 - GLYPH_H,
                  "YOU WIN!", COLOR_YELLOW);
        snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
        draw_text(bb, bb->width / 2 - 4 * GLYPH_W,
                      bb->height / 2 + GLYPH_H + 4,
                  buf, COLOR_WHITE);
        draw_text(bb, bb->width / 2 - 7 * GLYPH_W,
                      bb->height / 2 + GLYPH_H * 2 + 8,
                  "PRESS R TO RESTART", COLOR_WHITE);
    }
}
