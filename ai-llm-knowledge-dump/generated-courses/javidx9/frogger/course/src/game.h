/* =============================================================================
 * game.h — Frogger Game Types & Public API
 * =============================================================================
 *
 * This is the single source of truth for:
 *   • All game-state types (GameState, GAME_PHASE, SpriteBank, …)
 *   • Input abstraction (GameInput, GameButtonState, UPDATE_BUTTON)
 *   • Lane data types and lane_scroll() inline function
 *   • The public API that platform code calls
 *
 * Include order:  no prerequisites — game.h includes its own dependencies.
 * Platform code:  #include "platform.h"  (which includes game.h)
 * Game code:      #include "game.h"
 * =============================================================================
 */

#ifndef FROGGER_GAME_H
#define FROGGER_GAME_H

#include <stdint.h>          /* uint32_t, uint8_t, int16_t, int32_t        */
#include <stdlib.h>          /* malloc, free, exit                         */
#include <string.h>          /* memset, memcpy, snprintf                   */
#include <stdio.h>           /* fprintf, fread, fopen — init + debug only  */

#include "utils/backbuffer.h"  /* Backbuffer, GAME_RGBA, SCREEN_W/H        */
#include "utils/math.h"        /* MIN, MAX, CLAMP, ABS                     */
#include "utils/audio.h"       /* GameAudioState, AudioOutputBuffer, SOUND_ID */

/* ══════ Debug Utilities ════════════════════════════════════════════════════

   DEBUG_TRAP — triggers a CPU breakpoint so a debugger can catch it.
   ASSERT     — checks condition; fires DEBUG_TRAP if false.
   Both are compiled OUT in release builds (no -DDEBUG).

   C99 has no built-in assert-with-message; we use __builtin_trap on
   GCC/Clang for tight integration with gdb/lldb.                        */
#ifdef DEBUG
  #define DEBUG_TRAP()  __builtin_trap()
  #define ASSERT(cond, msg) \
      do { \
          if (!(cond)) { \
              fprintf(stderr, "ASSERT FAILED: %s\n  at %s:%d\n", \
                      (msg), __FILE__, __LINE__); \
              DEBUG_TRAP(); \
          } \
      } while (0)
#else
  #define DEBUG_TRAP()          ((void)0)
  #define ASSERT(cond, msg)     ((void)0)
#endif

/* ══════ Tile / Lane Constants ══════════════════════════════════════════════
   Screen is SCREEN_CELLS_W × SCREEN_CELLS_H cells; each cell = CELL_PX px.
   TILE_CELLS / TILE_PX: each sprite tile is 8 cells = 64 pixels.
   LANE_PATTERN_LEN: tile-count of the repeating pattern per lane (wraps).
   NUM_LANES: 10 lanes (home row + river rows + pavements + road rows).    */
#define TILE_CELLS        8
#define TILE_PX          (TILE_CELLS * CELL_PX)   /* 64 pixels per tile  */
#define LANE_WIDTH       18    /* tiles visible on screen + 1 for wrapping */
#define LANE_PATTERN_LEN 64    /* tiles in the repeating pattern           */
#define NUM_LANES        10

/* ══════ GAME_PHASE ═════════════════════════════════════════════════════════

   Replaces the reference's single `int dead` flag with a proper enum.
   Using an enum prevents category errors and makes adding new phases easy.

   JS equivalent:  type GamePhase = 'playing' | 'dead' | 'win';

   > Course Note: Reference uses `int dead`.  The course uses GAME_PHASE to
   > match the Tetris/Snake/Asteroids pattern and enable clean switch dispatch. */
typedef enum {
    PHASE_PLAYING = 0,  /* normal gameplay                                 */
    PHASE_DEAD,         /* frog hit unsafe tile — death flash active       */
    PHASE_WIN           /* all five homes filled                           */
} GAME_PHASE;

/* ══════ Sprite Constants ════════════════════════════════════════════════════
   9 sprite types; each stored in a flat pool inside SpriteBank.          */
#define NUM_SPRITES      9
#define SPR_POOL_CELLS   (9 * 32 * 8)   /* generous pool; checked at load */

#define SPR_FROG       0
#define SPR_WATER      1
#define SPR_PAVEMENT   2
#define SPR_WALL       3
#define SPR_HOME       4
#define SPR_LOG        5
#define SPR_CAR1       6
#define SPR_CAR2       7
#define SPR_BUS        8

/* ══════ SpriteBank — all sprite data in one flat pool ═════════════════════

   No heap allocation after game_init().  All sprite color and glyph data
   lives in these fixed arrays inside GameState.  The offsets[] table maps
   sprite ID → starting index in the pool.

   .spr file format:
     int32_t width;
     int32_t height;
     int16_t colors[width*height];   (FG in low nibble, BG in high nibble)
     int16_t glyphs[width*height];   (0x2588 = solid, 0x0020 = transparent)

   JS analogy: like pre-loading all images into a single ArrayBuffer and
   using byte offsets to address each one — no per-image fetch().         */
typedef struct {
    int16_t colors[SPR_POOL_CELLS];
    int16_t glyphs[SPR_POOL_CELLS];
    int     widths [NUM_SPRITES];
    int     heights[NUM_SPRITES];
    int     offsets[NUM_SPRITES];   /* starting index into colors[]/glyphs[] */
} SpriteBank;

/* ══════ CONSOLE_PALETTE ════════════════════════════════════════════════════

   Maps 4-bit Windows console color index (low nibble of a .spr color byte)
   to an {R, G, B} triple.  Used in draw_sprite_partial to convert sprite
   color indices to GAME_RGBA values.

   Defined static inline here so it is available in game.c AND draw-shapes.c
   without a separate .c compilation unit.

   JS analogy: an enum-indexed CSS color palette — same idea, explicit bytes. */
static const uint8_t CONSOLE_PALETTE[16][3] = {
    {  0,   0,   0},  /* 0  Black        */
    {  0,   0, 128},  /* 1  Dark Blue    */
    {  0, 128,   0},  /* 2  Dark Green   */
    {  0, 128, 128},  /* 3  Dark Cyan    */
    {128,   0,   0},  /* 4  Dark Red     */
    {128,   0, 128},  /* 5  Dark Magenta */
    {128, 128,   0},  /* 6  Dark Yellow  */
    {192, 192, 192},  /* 7  Gray         */
    {128, 128, 128},  /* 8  Dark Gray    */
    {  0,   0, 255},  /* 9  Blue         */
    {  0, 255,   0},  /* 10 Bright Green */
    {  0, 255, 255},  /* 11 Cyan         */
    {255,   0,   0},  /* 12 Red          */
    {255,   0, 255},  /* 13 Magenta      */
    {255, 255,   0},  /* 14 Yellow       */
    {255, 255, 255},  /* 15 White        */
};

/* ══════ Input System ═══════════════════════════════════════════════════════

   GameButtonState tracks both current state AND transitions within a frame.
   Origin: Casey Muratori's Handmade Hero.

   ended_down:            1 = key currently held; 0 = released.
   half_transition_count: state changes this frame.
     0 = no change (held or idle)
     1 = normal press or release
     2 = full tap (pressed + released within one frame)

   "Just pressed this frame":
     button.ended_down && button.half_transition_count > 0

   FROGGER NOTE: Frog moves on "just pressed" only (no DAS/ARR repeat).
   This is intentional: frog hops one cell per key press, never auto-repeats.
   See Lesson 03 for a comparison with Tetris's DAS-ARR system.           */
typedef struct {
    int half_transition_count;
    int ended_down;
} GameButtonState;

/* Call on KeyPress:   UPDATE_BUTTON(btn, 1)
   Call on KeyRelease: UPDATE_BUTTON(btn, 0)
   Only increments count when state actually changes (prevents double-counts). */
#define UPDATE_BUTTON(button, is_down)                     \
    do {                                                    \
        if ((button).ended_down != (is_down)) {             \
            (button).half_transition_count++;               \
            (button).ended_down = (is_down);                \
        }                                                   \
    } while (0)

/* Number of directional buttons (used for loop iteration via buttons[]) */
#define BUTTON_COUNT 4

/* GameInput uses a union so buttons can be accessed by index (for looping
   in prepare_input_frame) OR by name (for readable game code).
   JS analogy:  { buttons: GameButtonState[], moveUp: GameButtonState, … }
   with the array and named fields sharing the same backing memory.        */
typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];
        struct {
            GameButtonState move_up;
            GameButtonState move_down;
            GameButtonState move_left;
            GameButtonState move_right;
        };
    };
    int quit;    /* set to 1 to exit the game loop                        */
    int restart; /* set to 1 to restart after game over                   */
} GameInput;

/* ══════ GameState ══════════════════════════════════════════════════════════

   All game state lives here.  No global variables, no hidden static locals
   except constant data tables (lane_speeds, lane_patterns, CONSOLE_PALETTE).
   Passing GameState by pointer keeps the function signatures explicit.

   JS analogy:
     class FroggerGame {
       frogX = 8; frogY = 9; phase = 'playing'; homes = 0; …
     }

   FIELD NOTES:
   • frog_x, frog_y: frog tile position (float so river drift is smooth).
   • time:           accumulated game time for lane_scroll calculations.
   • dead_timer:     countdown for death flash (decrements in PHASE_DEAD).
   • danger[]:       flat 2D collision grid — 1 = unsafe, 0 = safe.
                     Rebuilt every frame in game_update, matched to render.
   • sprites:        all sprite sheets packed into a fixed pool.
   • audio:          procedural SFX mixer state.
   • score:          incremented on each successful hop; preserved on death.
   • lives:          start at 3; decremented on death; game over at 0.
   • best_score:     preserved across restart (not cleared by memset).     */
typedef struct {
    float      frog_x;
    float      frog_y;
    float      time;
    GAME_PHASE phase;
    int        homes_reached;
    float      dead_timer;
    int        score;
    int        lives;
    int        best_score;  /* must save/restore across game_init() memset */

    /* Flat 2D danger buffer: danger[row * SCREEN_CELLS_W + col]
       Written and read within the same game_update() call — always
       consistent with what game_render() will draw.                       */
    uint8_t    danger[SCREEN_CELLS_W * SCREEN_CELLS_H];

    SpriteBank sprites;
    GameAudioState audio;

    /* Stored so game_update() can trigger a full game_init() on restart. */
    char       assets_dir[256];
} GameState;

/* ══════ lane_scroll ════════════════════════════════════════════════════════
 *
 * Pixel-accurate scroll position for a lane.  Used by BOTH game_update
 * (danger buffer write) and game_render (sprite draw) so the collision grid
 * is always consistent with what the player sees on screen.
 *
 * THE BUG THIS FIXES (sub-tile jumping):
 *   The naive pattern:  tile_start = (int)(time * speed) % PATTERN_LEN
 *   fails in two ways:
 *
 *   1. C `(int)` TRUNCATES toward zero, not toward -infinity.
 *      floor(-4.5) = -5, but (int)(-4.5) = -4.
 *      For negative speeds, tile_start jumps one extra tile when the
 *      fractional part crosses zero — visible as a hard snap.
 *
 *   2. Sub-tile offset in CELL units (8-pixel steps) means sprites jump
 *      8 pixels at a time instead of scrolling smoothly pixel by pixel.
 *
 * THE FIX: work entirely in PIXELS, use positive modulo.
 *
 *   sc = (int)(time * speed * TILE_PX) % PATTERN_PX  ← may be negative
 *   if (sc < 0) sc += PATTERN_PX                      ← always [0, PATTERN_PX)
 *   tile_start = sc / TILE_PX
 *   px_offset  = sc % TILE_PX                         ← 0..63 px (smooth)
 *
 * EXAMPLE (speed = -3, time = 1.33 s, TILE_PX = 64):
 *   raw      = 1.33 * -3 * 64 = -255.36
 *   sc       = (int)(-255.36) % (64*64) = -255 % 4096 = -255
 *   sc after clamp: -255 + 4096 = 3841
 *   tile_start = 3841 / 64 = 60
 *   px_offset  = 3841 % 64 = 1   ← 1 pixel offset, sub-tile smooth
 *
 * Both frogger_tick (danger) and frogger_render (drawing) call this with
 * the same state->time — guaranteed to produce the same tile_start and
 * px_offset, so collision always matches rendering.
 *
 * JS analogy:  ctx.translate(-time * speed * TILE_PX, 0) — same positive-mod
 *              trick would be needed if you used integer tile positions.  */
static inline void lane_scroll(float time, float speed,
                                int *out_tile_start, int *out_px_offset) {
    const int PATTERN_PX = LANE_PATTERN_LEN * TILE_PX;  /* 64 * 64 = 4096 */
    int sc = (int)(time * speed * (float)TILE_PX) % PATTERN_PX;
    if (sc < 0) sc += PATTERN_PX;
    *out_tile_start = sc / TILE_PX;
    *out_px_offset  = sc % TILE_PX;
}

/* ══════ Public Game API ════════════════════════════════════════════════════

   Called by platform backends; implemented in game.c and audio.c.        */

/* Reset per-frame transition counts.  Call BEFORE platform_get_input.
   Copies ended_down from old frame so held keys remain held.              */
void prepare_input_frame(GameInput *old_input, GameInput *current_input);

/* Load sprites from assets_dir; place frog at start; set lives = 3.
   Preserves best_score across the memset.                                 */
void game_init(GameState *state, const char *assets_dir);

/* Advance the game by dt seconds — pure logic, no drawing.               */
void game_update(GameState *state, const GameInput *input, float dt);

/* Render the full frame into bb — no platform API calls.                  */
void game_render(Backbuffer *bb, const GameState *state);

/* Fill audio_out->samples with the next batch of synthesised PCM audio.
   Called by the platform audio callback (ALSA handler or Raylib loop).    */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *audio_out);

/* Trigger a one-shot sound effect.
   pan: -1.0=full left, 0.0=centre, +1.0=full right.                      */
void game_play_sound(GameState *state, SOUND_ID id, float pan);

/* Start a looping ambient sound at an initial vol_scale (usually 0).
   If the sound is already looping, only vol_scale is updated.             */
void game_play_looping(GameState *state, SOUND_ID id, float pan, float vol_scale);

/* Update vol_scale on an existing looping/active sound by SOUND_ID.      */
void game_set_sound_vol(GameState *state, SOUND_ID id, float vol_scale);

/* Advance the background music note sequencer by dt seconds.
   Triggers SOUND_MUSIC notes according to MUSIC_NOTES[].
   Call from game_update() every frame.                                    */
void game_music_update(GameState *state, float dt);

/* Update ambient loop volumes based on frog position.
   Call from game_update() every frame after game logic.                   */
void game_update_ambients(GameState *state);

#endif /* FROGGER_GAME_H */
