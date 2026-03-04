/*
 * game.h  —  Sugar, Sugar | Shared Game Types and Public API
 *
 * This header is the contract between:
 *   - game.c   (implements these types)
 *   - main_x11.c / main_raylib.c  (calls game_init/game_update/game_render)
 *
 * Student note: In JavaScript you'd write
 *   export interface GameState { ... }
 * In C we use structs + function declarations in a header file.
 * The header is like the "module interface" — implementation lives in .c files.
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h> /* uint32_t, uint8_t  — like TypeScript's number types */
#include "utils/audio.h" /* GameAudioState, SOUND_ID, AudioOutputBuffer     */

/* ===================================================================
 * DEBUG HELPERS
 *
 * ASSERT(expr) — traps the debugger if expr is false (debug builds only).
 * DEBUG_TRAP() — unconditional breakpoint.
 *
 * In release builds (-DDEBUG not set) both expand to nothing → zero cost.
 * In debug builds (-DDEBUG set via build-dev.sh) they call __builtin_trap()
 * which halts the program and lets you inspect the call stack.
 *
 * JS analogy: like `console.assert(expr)` but fatal.
 * =================================================================== */
#ifdef DEBUG
  #define DEBUG_TRAP() __builtin_trap()
  #define ASSERT(expr) do { if (!(expr)) { DEBUG_TRAP(); } } while (0)
#else
  #define DEBUG_TRAP() ((void)0)
  #define ASSERT(expr) ((void)(expr))
#endif

/* ===================================================================
 * COLOR SYSTEM
 *
 * Pixel format: 0xAARRGGBB  (alpha in high byte, blue in low byte)
 * On a little-endian CPU the bytes in memory are: [B, G, R, A]
 * X11 reads them as BGR (alpha ignored) — which is exactly what we want.
 *
 * JS analogy: like CSS hex color, but with alpha prepended.
 * =================================================================== */
#define GAME_RGBA(r, g, b, a)                                                  \
  (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) |      \
   (uint32_t)(b))
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 0xFF)

/* Named color constants — include game.h once and use everywhere */
#define COLOR_BG                                                               \
  GAME_RGB(135, 195,                                                           \
           225) /* soft sky-blue — matches original Sugar Sugar canvas */
#define COLOR_LINE GAME_RGB(50, 50, 50)        /* player-drawn lines       */
#define COLOR_OBSTACLE GAME_RGB(130, 110, 80)  /* pre-set level obstacles  */
#define COLOR_CUP_BORDER GAME_RGB(60, 60, 100) /* cup outline              */
#define COLOR_CUP_FILL GAME_RGB(180, 210, 255) /* cup fill-progress bar    */
#define COLOR_CUP_FILL_FULL                                                    \
  GAME_RGB(80, 200, 100)                         /* cup fully-filled (green) */
#define COLOR_CUP_EMPTY GAME_RGB(220, 220, 240)  /* cup empty area           */
#define COLOR_UI_TEXT GAME_RGB(50, 50, 50)       /* labels and numbers       */
#define COLOR_BTN_NORMAL GAME_RGB(210, 200, 190) /* button background */
#define COLOR_BTN_HOVER GAME_RGB(180, 170, 160)  /* button hover             */
#define COLOR_BTN_BORDER GAME_RGB(120, 110, 100) /* button outline */
#define COLOR_WHITE GAME_RGB(255, 255, 255)
#define COLOR_BLACK GAME_RGB(0, 0, 0)
#define COLOR_RED GAME_RGB(229, 57, 53)        /* grain / filter red       */
#define COLOR_GREEN GAME_RGB(67, 160, 71)      /* grain / filter green     */
#define COLOR_ORANGE GAME_RGB(251, 140, 0)     /* grain / filter orange    */
#define COLOR_CREAM GAME_RGB(232, 213, 183)    /* default (white) grain    */
#define COLOR_PORTAL_A GAME_RGB(100, 180, 255) /* teleporter entry         */
#define COLOR_PORTAL_B GAME_RGB(255, 140, 0)   /* teleporter exit          */
#define COLOR_COMPLETE GAME_RGB(76, 175, 80)   /* level-complete overlay   */

/* ===================================================================
 * BACKBUFFER
 *
 * The game writes pixels into this struct.  The platform uploads it to
 * the screen each frame.
 *
 * Think of it as a canvas.  Every frame we fill it from scratch (clear to
 * background color, then draw everything on top).
 * =================================================================== */
typedef struct {
  uint32_t *pixels; /* flat ARGB array: pixels[y * width + x] = color  */
  int width;        /* canvas width  in pixels (640)                    */
  int height;       /* canvas height in pixels (480)                    */
  int pitch;        /* bytes per row = width * 4                        */
} GameBackbuffer;

/* ===================================================================
 * INPUT
 *
 * GameButtonState tracks not just "is this held" but also "how many times
 * did it change state this frame?"  That lets us detect a very-fast
 * press-and-release in a single frame.
 *
 * Origin: Casey Muratori — Handmade Hero episode 4.
 *
 * JS analogy: like having both onKeyDown and onKeyUp handlers, but packed
 * into one struct that's queried at the start of each frame.
 * =================================================================== */
typedef struct {
  int half_transition_count; /* +1 for every press OR release this frame */
  int ended_down;            /* 1 = currently held, 0 = currently released */
} GameButtonState;

/* Macro: call this for every press or release event the OS sends us.
 * Only increments the counter when the state actually changed — prevents
 * duplicate events from auto-repeat. */
#define UPDATE_BUTTON(button, is_down)                                         \
  do {                                                                         \
    if ((button).ended_down != (is_down)) {                                    \
      (button).half_transition_count++;                                        \
      (button).ended_down = (is_down);                                         \
    }                                                                          \
  } while (0)

/* Convenience macros */
#define BUTTON_PRESSED(b) ((b).half_transition_count > 0 && (b).ended_down)
#define BUTTON_RELEASED(b) ((b).half_transition_count > 0 && !(b).ended_down)

typedef struct {
  int x, y;           /* current pixel position this frame  */
  int prev_x, prev_y; /* position last frame (for draw lines) */
  GameButtonState left;
  GameButtonState right;
} MouseInput;

typedef struct {
  MouseInput mouse;
  GameButtonState escape;  /* Escape key   — quit / back to title  */
  GameButtonState reset;   /* R key        — restart current level */
  GameButtonState gravity; /* G key        — flip gravity          */
  GameButtonState enter;   /* Enter/Space  — confirm / advance     */
} GameInput;

/* ===================================================================
 * CANVAS SIZE
 * =================================================================== */
#define CANVAS_W 640
#define CANVAS_H 480

/* ===================================================================
 * PHYSICS CONSTANTS  (tweakable per-project knobs)
 * =================================================================== */

/* Horizontal damping per frame.  0.98 at 60 fps = 0.98^60 ≈ 0.30/s.
 * A grain at 50 px/s loses speed slowly enough to travel ~1.5 s of slide
 * before stopping, which feels natural on a long ramp. */
#define GRAIN_HORIZ_DRAG    0.995f /* multiplied against vx each frame;
                                    * very gentle decay keeps grains sliding
                                    * and bouncing longer for a lively feel    */

/* Minimum slide speed assigned when a grain lands on a surface and finds a
 * free diagonal pixel.  Using max(|vy|, GRAIN_SLIDE_MIN) ensures grains
 * always slide with enough speed to be detected as "moving" by the
 * displacement-based settle check, even on shallow collisions where
 * gravity has only barely re-accumulated since the last surface contact.
 *
 * Why a minimum is safe here (unlike Issue 16):
 *   The minimum ONLY runs inside the "free diagonal found" branch.
 *   When both diagonals are blocked (flat pile), vx and vy are zeroed by
 *   the !slid branch regardless — the minimum never fires, so the grain
 *   settles and bakes correctly.
 *
 * Why 25 px/s (not higher):
 *   50 px/s caused sliding grains to fly far from the pile, forming
 *   discrete "satellite" secondary piles several pixels away — making the
 *   stream look like 3 separate trickles.  25 px/s keeps slide-off grains
 *   close to the main pile for a more natural, cohesive heap.
 *   At 25 px/s, dist = 25×dt = 0.42 px/frame >> settle threshold (0.17 px).
 *   Sliding grains are always detected as "moving" — safe. */
#define GRAIN_SLIDE_MIN     25.0f  /* px/s floor for surface-slide speed   */

/* Coefficient of restitution: fraction of |vy| preserved as an upward
 * bounce when a grain hits a solid surface from above.
 *
 *   vy_after = -|vy_before| × GRAIN_BOUNCE
 *
 * Only applied when |vy| > GRAIN_BOUNCE_MIN so very slow taps (gravity
 * re-accumulation) don't produce perpetual micro-bounces that prevent
 * settling.  After GRAIN_BOUNCE_MIN is reached the bounce term is dropped
 * and vy is set to 0 — the normal settle path resumes.
 *
 * Example chain for a grain landing at vy = 200 px/s:
 *   bounce 1: vy = -200 × 0.25 = -50 px/s (upward)
 *   bounce 2: vy = -50  × 0.25 = -12.5 px/s  (< MIN → stops)
 *
 * 0.25 gives a satisfying light-bounce without making grains leap high. */
#define GRAIN_BOUNCE        0.25f  /* restitution coefficient (0 = no bounce) */
#define GRAIN_BOUNCE_MIN   20.0f  /* px/s: only bounce if |vy| > this        */

/* How many pixels to each side the slide-diagonal check reaches.
 * 1 → angle of repose ~45° (grains stack like a wall).
 * 2 → angle of repose ~27° (grains flow more like sugar/sand).
 * Higher values let grains hop over neighbours for a more fluid look;
 * the path-clear guard ensures they never jump through solid walls. */
#define GRAIN_SLIDE_REACH   2      /* max lateral pixels checked for slide */

/* Grain settle: a grain is considered "still" if its average speed this frame
 * is below GRAIN_SETTLE_SPEED px/s.  We compute speed from displacement:
 *   speed ≈ dist / dt   →   dist < GRAIN_SETTLE_SPEED * dt
 * Stored as squared for a branch-free compare:
 *   dist² < (GRAIN_SETTLE_SPEED * dt)²
 *
 * This is FRAME-RATE INDEPENDENT: the threshold shrinks proportionally with dt
 * so a grain at 50 px/s never settles whether running at 60 fps or 1000 fps.
 * A truly stuck grain (dist ≈ 0) always settles. */
#define GRAIN_SETTLE_SPEED  10.0f  /* px/s below which a grain is "still"   */
#define GRAIN_SETTLE_FRAMES 8      /* consecutive "stuck" frames → bake      */

/* ===================================================================
 * GRAIN COLORS
 * =================================================================== */
typedef enum {
  GRAIN_WHITE = 0, /* default — any cup accepts white grains */
  GRAIN_RED = 1,
  GRAIN_GREEN = 2,
  GRAIN_ORANGE = 3,
  GRAIN_COLOR_COUNT
} GRAIN_COLOR;

/* ===================================================================
 * GRAIN POOL  (Struct-of-Arrays for cache efficiency)
 *
 * WHY SoA instead of AoS?
 *
 * AoS (Array of Structs):
 *   struct Grain { float x, y, vx, vy; uint8_t color, active; };
 *   Grain pool[4096];
 *   — When we loop over x[] and y[] to apply gravity, we also load
 *     color and active into cache even though we don't need them.
 *     This wastes cache bandwidth.
 *
 * SoA (Struct of Arrays):
 *   x[4096], y[4096], vx[4096], vy[4096], color[4096], active[4096]
 *   — The gravity loop only touches x[], y[], vx[], vy[].
 *     Those 4 arrays fit in L1/L2 cache together.  Color/active are
 *     never loaded, so no cache pollution.
 *
 * For 4096 grains the gravity pass reads 4×4096×4 = 64 KB — fits in
 * typical 256 KB L2 cache perfectly.
 * =================================================================== */
#define MAX_GRAINS 4096

typedef struct {
  float x[MAX_GRAINS];  /* horizontal position (pixels, float for sub-pixel) */
  float y[MAX_GRAINS];  /* vertical   position                               */
  float vx[MAX_GRAINS]; /* horizontal velocity (pixels/second)               */
  float vy[MAX_GRAINS]; /* vertical   velocity (pixels/second, +down)        */
  uint8_t color[MAX_GRAINS];  /* GRAIN_COLOR value  */
  uint8_t active[MAX_GRAINS]; /* 1 = alive, 0 = free slot */
  uint8_t tpcd[MAX_GRAINS]; /* teleport cooldown (frames) to prevent re-entry */
  uint8_t still[MAX_GRAINS]; /* consecutive frames with near-zero velocity */
  int count; /* high-watermark: slots 0..count-1 are valid      */
} GrainPool;

/* ===================================================================
 * LEVEL ENTITIES
 * =================================================================== */

typedef struct {
  int x, y;              /* pixel position of the spout                  */
  int grains_per_second; /* emission rate (e.g. 80)                      */
  float spawn_timer;     /* internal: seconds accumulated since last emit */
} Emitter;

typedef struct {
  int x, y, w, h;             /* bounding box (pixels)                   */
  GRAIN_COLOR required_color; /* GRAIN_WHITE = accepts any color          */
  int required_count;         /* grains needed to fill the cup            */
  int collected;              /* grains received so far                   */
} Cup;

typedef struct {
  int x, y, w, h;           /* bounding box of the filter zone          */
  GRAIN_COLOR output_color; /* color grains get when passing through    */
} ColorFilter;

typedef struct {
  int ax, ay; /* portal A center (pixels) */
  int bx, by; /* portal B center (pixels) */
  int radius; /* trigger radius  (pixels) */
} Teleporter;

typedef struct {
  int x, y, w, h; /* solid rectangle stamped into the line bitmap on load */
} Obstacle;

#define MAX_EMITTERS 2
#define MAX_CUPS 8
#define MAX_FILTERS 4
#define MAX_TELEPORTERS 2
#define MAX_OBSTACLES 12

typedef struct {
  int index;
  Emitter emitters[MAX_EMITTERS];
  int emitter_count;
  Cup cups[MAX_CUPS];
  int cup_count;
  ColorFilter filters[MAX_FILTERS];
  int filter_count;
  Teleporter teleporters[MAX_TELEPORTERS];
  int teleporter_count;
  Obstacle obstacles[MAX_OBSTACLES];
  int obstacle_count;
  int has_gravity_switch; /* 1 = show the gravity flip button */
  int is_cyclic;          /* 1 = sugar wraps bottom → top     */
} LevelDef;

/* ===================================================================
 * LINE BITMAP  (player-drawn + obstacle + baked-grain pixels)
 *
 * Each byte encodes one pixel of the collision + render layer:
 *
 *   0        → empty
 *   1        → obstacle / cup wall (stamped at level load)
 *   255      → player-drawn line (stamped by the brush)
 *   2..5     → baked settled grain, color index = pixels[…] − 2
 *              (GRAIN_WHITE=0 → stored as 2, GRAIN_RED→3, etc.)
 *
 * Physics:  any non-zero value is solid (IS_SOLID checks `!= 0`).
 * Renderer: value 1 or 255 → COLOR_LINE; value 2..5 → grain color.
 *
 * This single-byte encoding avoids a second parallel array for
 * "what color was this settled grain?" while keeping O(1) lookup.
 *
 * At 640×480 = 307,200 bytes ≈ 300 KB — fits entirely in L2 cache.
 * =================================================================== */
typedef struct {
  uint8_t pixels[CANVAS_W * CANVAS_H];
} LineBitmap;

/* ===================================================================
 * GAME STATE MACHINE
 * =================================================================== */
typedef enum {
  PHASE_TITLE,          /* title screen + level select grid              */
  PHASE_PLAYING,        /* active puzzle: simulation + drawing           */
  PHASE_LEVEL_COMPLETE, /* brief celebration, then advance               */
  PHASE_FREEPLAY,       /* sandbox after completing all 30 levels        */
  PHASE_COUNT
} GAME_PHASE;

/* ===================================================================
 * GAME STATE  (the whole world)
 *
 * Declared as a static local in main() so the ~460 KB of arrays lives in
 * the BSS segment (zero-initialised at program start), not on the stack.
 * =================================================================== */
typedef struct {
  /* ---- state machine ---- */
  GAME_PHASE phase;
  float phase_timer; /* seconds in current phase                 */

  /* ---- level progress ---- */
  int current_level;  /* 0-based index into g_levels[]            */
  int unlocked_count; /* how many levels are accessible           */
  LevelDef level;     /* active level (copied from g_levels[])    */

  /* ---- simulation ---- */
  GrainPool grains; /* SoA particle pool                        */
  LineBitmap lines; /* player-drawn + obstacle pixels           */
  int gravity_sign; /* +1 = down (normal), -1 = up (flipped)   */

  /* ---- UI hover state ---- */
  int title_hover; /* which level button is hovered (-1=none)  */
  int reset_hover;
  int gravity_hover;

  /* ---- quit signal ---- */
  int should_quit; /* set to 1 by game_update to request exit  */

  /* ---- audio ---- */
  GameAudioState audio; /* procedural sound + music state           */
} GameState;

/* ===================================================================
 * PUBLIC GAME API  (implemented in game.c)
 * =================================================================== */

/* Must be called once before the game loop.
 * Allocates nothing — all state is inside GameState.
 * Resets state, loads level 0. */
void game_init(GameState *state, GameBackbuffer *backbuffer);

/* Call at the start of each frame, BEFORE platform_get_input().
 * Resets half_transition_count on every button — keeps "pressed this frame"
 * semantics clean from one frame to the next. */
void prepare_input_frame(GameInput *input);

/* Advance the simulation by delta_time seconds.
 * Reads input, runs physics, handles FSM transitions.
 * NEVER reads from the backbuffer — only writes to GameState. */
void game_update(GameState *state, GameInput *input, float delta_time);

/* Write the current frame into backbuffer->pixels.
 * READ-ONLY on GameState — never modifies simulation data. */
void game_render(const GameState *state, GameBackbuffer *backbuffer);

/* Level definitions — defined in levels.c, referenced in game.c */
#define TOTAL_LEVELS 30
extern LevelDef g_levels[TOTAL_LEVELS];

/* ===================================================================
 * AUDIO API  (implemented in audio.c)
 *
 * game_audio_init        — zero audio state, wire sample rate (call from game_init)
 * game_play_sound        — fire-and-forget SFX at center pan
 * game_play_sound_at     — fire-and-forget SFX with stereo position
 * game_music_play_title    — start dreamy title-screen music (fades in)
 * game_music_play_gameplay — start upbeat gameplay music (fades in)
 * game_music_stop        — stop all music (fades out cleanly in sample loop)
 * game_audio_update      — advance music sequencers + fade volumes (1× per frame)
 * game_get_audio_samples — fill platform audio buffer (called by platform)
 * =================================================================== */
void game_audio_init(GameAudioState *audio, int samples_per_second);
void game_play_sound(GameAudioState *audio, SOUND_ID sound);
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound, float pan);
void game_music_play_title(GameAudioState *audio);
void game_music_play_gameplay(GameAudioState *audio);
void game_music_stop(GameAudioState *audio);
void game_audio_update(GameAudioState *audio, float delta_time);
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer);

#endif /* GAME_H */
