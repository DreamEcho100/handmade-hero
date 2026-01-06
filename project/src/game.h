#ifndef GAME_H
#define GAME_H

#include "platform/platform.h"
#include "platform/_common/memory.h"
#include "base.h"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
// 🧠 DAY 14: EXPLICIT GAME MEMORY
// ═══════════════════════════════════════════════════════════════
// Casey's Day 14 addition: Platform allocates, game manages!
//
// MEMORY LAYOUT:
// ┌─────────────────────────────────────────────────────────────┐
// │ PermanentStorage (64MB)                                     │
// │ ┌───────────────────────────────────────────────────────┐   │
// │ │ game_state (your actual game data)                   │   │
// │ │ - gradient_state                                      │   │
// │ │ - pixel_state                                         │   │
// │ │ - speed                                               │   │
// │ └───────────────────────────────────────────────────────┘   │
// │ [Rest of 64MB available for:]                               │
// │ - Save game data                                            │
// │ - Player stats                                              │
// │ - Settings/options                                          │
// └─────────────────────────────────────────────────────────────┘
// ┌─────────────────────────────────────────────────────────────┐
// │ TransientStorage (4GB)                                      │
// │ [Ready for:]                                                │
// │ - Level geometry                                            │
// │ - Particle systems                                          │
// │ - Temporary buffers                                         │
// └─────────────────────────────────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════

/**
 * 🧠 GAME MEMORY
 * ───────────────────────────────────────────────────────────────
 * Platform allocates this ONCE at startup using mmap().
 * Game receives pointer and casts to game_state*.
 *
 * Casey's Day 14 pattern:
 *   - IsInitialized flag for first-run detection
 *   - PermanentStorage for save data, settings
 *   - TransientStorage for level data, temp buffers
 * ───────────────────────────────────────────────────────────────
 */
typedef struct {
  // A permanent block of memory that the game can use between calls to
  // `game_update_and_render`. This is where you should store all your game
  // state!
  PlatformMemoryBlock permanent_storage;         
  // A temporary block of memory that the game can use between calls to
  // `game_update_and_render`. This is where you should store all your
  // scratch data!
  PlatformMemoryBlock transient_storage;          
  // Size of the permanent storage block in bytes
  uint64_t permanent_storage_size;  
  // Size of the temporary storage block in bytes
  uint64_t transient_storage_size;  
  // Has this memory been initialized?
  bool32 is_initialized;            
} GameMemory;

typedef struct {
  PlatformMemoryBlock memory; // Raw pixel memory (our canvas!)
  int width;    // Current backbuffer dimensions
  int height;
  int pitch;
  int bytes_per_pixel;
  pixel_composer_fn compose_pixel;
} GameOffscreenBuffer;

typedef struct {
  // ═════════════════════════════════════════════════════════
  // HARDWARE PARAMETERS (Platform Layer Owns)
  // ═════════════════════════════════════════════════════════
  bool is_initialized;

  int32_t samples_per_second;  // 48000 Hz (hardware config)
  int32_t bytes_per_sample;    // 4 (16-bit stereo)

  // ═════════════════════════════════════════════════════════
  // AUDIO GENERATION STATE (Platform Layer Uses)
  // ═════════════════════════════════════════════════════════
  uint32_t running_sample_index;  // Sample counter (for waveform)
  int wave_period;                // Samples per wave (cached calculation)
  real32 t_sine;                  // Phase accumulator (0 to 2π)
  int latency_sample_count;       // Target latency in samples


  // ═════════════════════════════════════════════════════════
  // GAME-SET PARAMETERS (Game Layer Sets)
  // ═════════════════════════════════════════════════════════
  int tone_hz;               // Frequency of tone to generate           
  int16_t tone_volume;       // Volume of tone to generate
  int pan_position;          // -100 (left) to +100 (right)
} GameSoundOutput;

/**
  * 🎮 DAY 13: PLATFORM-INDEPENDENT INPUT ABSTRACTION
  * ═══════════════════════════════════════════════════════════════
  *
  * These structures replace ALL platform-specific input handling.
  * Game layer ONLY sees these - no X11 KeySym, no joystick events!
  *
  * Casey's Day 13 pattern: Abstract button state + analog sticks
  * ═══════════════════════════════════════════════════════════════
  *
  * BUTTON STATE (replaces raw bool flags)
  * ───────────────────────────────────────────────────────────────
  *
  * Tracks BOTH current state AND transitions (press/release events).
  *
  * Casey's pattern:
  *   EndedDown = final state this frame
  *   HalfTransitionCount = how many times it changed
  *
  * Examples:
  *   HalfTransitionCount=0, EndedDown=false → Button not pressed, no change
  *   HalfTransitionCount=1, EndedDown=true  → Button JUST pressed!
  *   HalfTransitionCount=0, EndedDown=true  → Button held down
  *   HalfTransitionCount=1, EndedDown=false → Button JUST released!
  *   HalfTransitionCount=2, EndedDown=true  → Pressed THEN released THEN pressed
  *   (same frame!)
  * ───────────────────────────────────────────────────────────────
  */
typedef struct {
  /**  Number of state changes this frame */
  int half_transition_count;
  /** Final state (true = pressed, false = released) */
  bool32 ended_down;
} GameButtonState;

#define CONTROLLER_DEADZONE 0.10f

/**
* CONTROLLER INPUT (replaces your GameControls struct)
* ───────────────────────────────────────────────────────────────
*
* Platform-agnostic controller abstraction.
* Works with:
*   - Xbox controllers (via joystick API)
*   - PlayStation controllers (via joystick API)
*   - Keyboard (converted to analog values)
*   - Future: Steam Deck, Switch Pro, etc.
*
* Casey's design: Analog sticks normalized to -1.0 to +1.0
*
* ────────────────────
*/
typedef struct {
  /** `true` = joystick, `false` = keyboard (digital) */
  bool32 is_analog;

  /**
   * ─────────────────────────────────────────────────────────────
   * Analog stick tracking (all values -1.0 to +1.0)
   * ─────────────────────────────────────────────────────────────
   * start_x/y: Stick position at beginning of frame
   * min_x/y:   Minimum position seen this frame (for deadzone)
   * max_x/y:   Maximum position seen this frame (for deadzone)
   * end_x/y:   Final position this frame (what game uses)
   *
   * NOTE: Day 13: Just use end_x/y (min/max for future Day 14+)
   * ─────────────────────────────────────────────────────────────
   */
  //
  real32 start_x, start_y;
  real32 min_x, min_y;
  real32 max_x, max_y;
  real32 end_x, end_y;


  /**
  * Can access as:
  *   - Array: for(int i=0; i<6; i++) buttons[i]...
  *   - Named: if(controller->up.ended_down) ...
  *
  * SAME MEMORY, TWO ACCESS PATTERNS! ✨
  * ─────────────────────────────────────────────────────────────
  */
  union {
    GameButtonState buttons[6];
    struct {
      GameButtonState up;
      GameButtonState down;
      GameButtonState left;
      GameButtonState right;

      // GameButtonState start;
      // GameButtonState back;
      // GameButtonState a_button;
      // GameButtonState b_button;
      // GameButtonState x_button;
      // GameButtonState y_button;
      GameButtonState left_shoulder;
      GameButtonState right_shoulder;
    };
  };

  // id: useful for debugging multiple controllers and for joystick file
  // descriptor
  // int id;
  int controller_index; // Which controller slot (0-3)
  // int fd;               // File descriptor for /dev/input/jsX
  bool is_connected;    // Is this controller active?
} GameControllerInput;

/**
* GAME INPUT (replaces your GameControls struct)
* ───────────────────────────────────────────────────────────────
*
* Supports up to 4 controllers (local multiplayer ready!)
*
* Casey's pattern:
*   Controllers[0] = Player 1
*   Controllers[1] = Player 2 (future)
*   Controllers[2] = Player 3 (future)
*   Controllers[3] = Player 4 (future)
*
* ───────────────────────────────────────────────────────────────
*/
typedef struct {
  GameControllerInput controllers[5];
} GameInput;

// typedef struct {
//   int speed;
//   bool is_running;
//   // int gamepad_id; // Which gamepad to use (0-3)
// } GameState;

#define MAX_CONTROLLER_COUNT 5
#define MAX_KEYBOARD_COUNT 1
#define MAX_JOYSTICK_COUNT (MAX_CONTROLLER_COUNT - MAX_KEYBOARD_COUNT)

extern int KEYBOARD_CONTROLLER_INDEX;
// extern GameState g_game_state;
extern bool is_game_running;

typedef struct {
  int offset_x;
  int offset_y;
} GradientState;

typedef struct {
  int offset_x;
  int offset_y;
} PixelState;

typedef struct {
  GradientState gradient_state;
  PixelState pixel_state;
  int speed;
} GameState;


typedef enum {
  INIT_BACKBUFFER_STATUS_SUCCESS = 0,
  INIT_BACKBUFFER_STATUS_MMAP_FAILED = 1,
} INIT_BACKBUFFER_STATUS;
INIT_BACKBUFFER_STATUS init_backbuffer(GameOffscreenBuffer *buffer, int width, int height, int bytes_per_pixel, pixel_composer_fn composer);

/**
* 🎮 DAY 13: Updated Game Entry Point
* ═══════════════════════════════════════════════════════════════
*
* New signature takes abstract input (not pixel_color parameter).
*
* Casey's Day 13 change:
*   OLD: GameUpdateAndRender(Buffer, XOffset, YOffset, Sound, ToneHz)
*   NEW: GameUpdateAndRender(Input, Buffer, Sound)
*
* We add:
*   game_input *input  ← Platform-agnostic input state
*
* Game state (offsets, tone_hz, etc.) now lives in game.c as
* local_persist variables (hidden from platform!).
*
* ═══════════════════════════════════════════════════════════════
*/
void game_update_and_render(GameMemory *memory, GameInput *input, GameOffscreenBuffer *buffer, GameSoundOutput *sound_buffer);


//
void process_game_button_state(bool is_down, GameButtonState *old_state, GameButtonState *new_state);

#endif // GAME_H
