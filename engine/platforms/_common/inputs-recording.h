#ifndef DE100_INPUT_RECORDING_H
#define DE100_INPUT_RECORDING_H

#include "../../game/inputs.h"
#include "../../game/memory.h"
#include <stdbool.h>

// ═══════════════════════════════════════════════════════════════════════════
// INPUT RECORDING API
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Begin recording input to a slot.
 * Saves game state to replay buffer, opens input file for writing.
 */
bool input_recording_begin(const char *exe_directory, GameMemoryState *state,
                           int32 slot_index);

/**
 * Record a single frame of input.
 * Called every frame while recording.
 */
void input_recording_record_frame(GameMemoryState *state,
                                  const GameInput *input);

/**
 * End recording.
 * Closes input file.
 */
void input_recording_end(GameMemoryState *state);

/**
 * Begin playback from a slot.
 * Restores game state from replay buffer, opens input file for reading.
 */
bool input_recording_playback_begin(const char *exe_directory,
                                    GameMemoryState *state, int32 slot_index);

/**
 * Play back a single frame of input.
 * Called every frame while playing. Handles looping automatically.
 *
 * @param exe_directory  Needed for state restore on loop
 * @param state          Game memory state
 * @param input          Output: filled with recorded input
 */
void input_recording_playback_frame(GameMemoryState *state, GameInput *input);

/**
 * End playback.
 * Closes input file.
 */
void input_recording_playback_end(GameMemoryState *state);

typedef enum {
  INPUT_RECORDING_TOGGLE_STARTTED_RECORDING,
  INPUT_RECORDING_TOGGLE_SWITCHED_TO_PLAYBACK,
  INPUT_RECORDING_TOGGLE_STOPPED_PLAYBACK,
} INPUT_RECORDING_TOGGLE_RESULT_CODE;

/**
 * Toggle recording/playback state.
 * IDLE -> RECORDING -> PLAYBACK -> IDLE
 */
INPUT_RECORDING_TOGGLE_RESULT_CODE
input_recording_toggle(const char *exe_directory, GameMemoryState *state);

/**
 * Check if currently recording.
 */
static inline bool input_recording_is_recording(const GameMemoryState *state) {
  return state && state->input_recording_index != 0;
}

/**
 * Check if currently playing back.
 */
static inline bool input_recording_is_playing(const GameMemoryState *state) {
  return state && state->input_playing_index != 0;
}

#endif // DE100_INPUT_RECORDING_H
