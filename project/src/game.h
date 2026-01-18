#ifndef GAME_H
#define GAME_H

#include "base.h"
#include "platform/_common/memory.h"
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
  int width;                  // Current backbuffer dimensions
  int height;
  int pitch;
  int bytes_per_pixel;
} GameOffscreenBuffer;

typedef struct {
  // ═════════════════════════════════════════════════════════
  // HARDWARE PARAMETERS (Platform Layer Owns)
  // ═════════════════════════════════════════════════════════
  bool is_initialized;

  int32_t samples_per_second; // 48000 Hz (hardware config)
  int32_t bytes_per_sample;   // 4 (16-bit stereo)

  // ═════════════════════════════════════════════════════════
  // AUDIO GENERATION STATE (Platform Layer Uses)
  // ═════════════════════════════════════════════════════════
  int64_t running_sample_index; // Sample counter (for waveform)
  int wave_period;               // Samples per wave (cached calculation)
  real32 t_sine;                 // Phase accumulator (0 to 2π)
  int latency_sample_count;      // Target latency in samples

  // ═════════════════════════════════════════════════════════
  // GAME-SET PARAMETERS (Game Layer Sets)
  // ═════════════════════════════════════════════════════════
  int tone_hz;         // Frequency of tone to generate
  int16_t tone_volume; // Volume of tone to generate
  int pan_position;    // -100 (left) to +100 (right)
  int32_t game_update_hz;
  

  // Day 20 safety margin
  int32_t safety_sample_count;  // Samples of safety buffer (~1/3 frame)
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
  /**
   * Number of state changes this frame
   *
   * half_transition_count is NOT a boolean flag!
   *
   * It's a COUNTER that tracks:
   *   0 = No change this frame (button held or released)
   *   1 = Changed once (normal press or release)
   *   2 = Changed twice (press then release, or release then press)
   *   3 = Changed three times (press-release-press, extremely rare!)
   *
   * Why this matters:
   * ```c
   *   if (button.ended_down && button.half_transition_count > 0) {
   *       // Button JUST pressed (not held from last frame)
   *   }
   *
   *   if (button.half_transition_count > 1) {
   *       // User is mashing the button VERY fast!
   *       // Might be lag, or input recording glitch
   *   }
   * ```
   */
  int half_transition_count;
  /** Final state (true = pressed, false = released) */
  bool32 ended_down;
} GameButtonState;

#define CONTROLLER_DEADZONE 0

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
  /**
   * Can access as:
   *   - Array: for(int i=0; i<6; i++) buttons[i]...
   *   - Named: if(controller->up.ended_down) ...
   *
   * SAME MEMORY, TWO ACCESS PATTERNS! ✨
   * ─────────────────────────────────────────────────────────────
   */
  union {
    GameButtonState buttons[12];
    struct {
      // MOVEMENT buttons (stick/WASD)
      GameButtonState move_up;    // ← Was "Up" in Day 16
      GameButtonState move_down;  // ← Was "Down"
      GameButtonState move_left;  // ← Was "Left"
      GameButtonState move_right; // ← Was "Right"

      // ACTION buttons (face buttons/arrows)
      GameButtonState action_up;    // ← Y button / Arrow Up
      GameButtonState action_down;  // ← A button / Arrow Down
      GameButtonState action_left;  // ← X button / Arrow Left
      GameButtonState action_right; // ← B button / Arrow Right

      // SHOULDER buttons (unchanged)
      GameButtonState left_shoulder;
      GameButtonState right_shoulder;

      // MENU buttons
      GameButtonState back;  // ← Select button / Space
      GameButtonState start; // ← Start button / Escape

      // NOTE: All buttons must be added above this line
      GameButtonState terminator; // ← Sentinel for validation
    };
  };

  real32 stick_avg_x; // ← Renamed from EndX (clearer name!)
  real32 stick_avg_y; // ← Renamed from EndY

  // id: useful for debugging multiple controllers and for joystick file
  // descriptor
  // int id;
  int controller_index; // Which controller slot (0-3)
  /** `true` = joystick, `false` = keyboard (digital) */
  bool32 is_analog;
  // int fd;               // File descriptor for /dev/input/jsX
  bool is_connected; // Is this controller active

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

GameControllerInput *GetController(GameInput *Input,
                                   unsigned int ControllerIndex);

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
extern bool g_game_is_paused;

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
void game_update_and_render(GameMemory *memory, GameInput *input,
                            GameOffscreenBuffer *buffer,
                            GameSoundOutput *sound_buffer);

#endif // GAME_H
