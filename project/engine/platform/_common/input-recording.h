#ifndef DE100_INPUT_RECORDING_H
#define DE100_INPUT_RECORDING_H

#include "../../_common/base.h"
#include "../../game/memory.h"
#include "../../game/input.h"
#include <stdbool.h>

// ═══════════════════════════════════════════════════════════════════════════
// INPUT RECORDING & PLAYBACK
// ═══════════════════════════════════════════════════════════════════════════
//
// This implements Casey's "looped live code editing" feature from Day 23.
//
// HOW IT WORKS:
// ─────────────────────────────────────────────────────────────────────────────
// 1. User presses 'L' to start recording
// 2. We save the ENTIRE game memory state to a file (the "snapshot")
// 3. We append each frame's input to the same file
// 4. User presses 'L' again to stop recording and start playback
// 5. We restore the memory snapshot
// 6. We read back inputs frame-by-frame, feeding them to the game
// 7. When we reach the end, we loop back (restore snapshot, restart inputs)
//
// FILE FORMAT:
// ─────────────────────────────────────────────────────────────────────────────
// [Game Memory Snapshot: total_size bytes]
// [Frame 0 Input: sizeof(GameInput) bytes]
// [Frame 1 Input: sizeof(GameInput) bytes]
// [Frame 2 Input: sizeof(GameInput) bytes]
// ... (continues until recording stops)
//
// WHY SNAPSHOT MEMORY?
// ─────────────────────────────────────────────────────────────────────────────
// Inputs alone aren't enough! If the game has random elements, physics,
// or any state that accumulates, playing back the same inputs won't
// reproduce the same result. By restoring memory state, we guarantee
// EXACT reproduction.
//
// ═══════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────────
// RECORDING FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Begin recording input to a slot.
 *
 * This:
 *   1. Opens/creates the recording file
 *   2. Writes the entire game memory as a snapshot
 *   3. Sets state to "recording mode"
 *
 * @param state         Platform state (will be modified)
 * @param slot_index    Which slot to record to (1-based, e.g., 1, 2, 3)
 * @return              true on success, false on failure
 *
 * After this call, you should call input_recording_record_frame() each frame.
 */
bool input_recording_begin(GameMemoryState *state, int32 slot_index);

/**
 * Record one frame of input.
 *
 * Call this every frame while recording is active.
 * Does nothing if not currently recording.
 *
 * @param state     Platform state
 * @param input     The input to record (will be written to file)
 */
void input_recording_record_frame(GameMemoryState *state, const GameInput *input);

/**
 * Stop recording.
 *
 * Closes the recording file and clears recording state.
 * Safe to call even if not recording (does nothing).
 *
 * @param state     Platform state (will be modified)
 */
void input_recording_end(GameMemoryState *state);

// ─────────────────────────────────────────────────────────────────────────────
// PLAYBACK FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Begin playing back input from a slot.
 *
 * This:
 *   1. Opens the recording file
 *   2. Reads and restores the game memory snapshot
 *   3. Sets state to "playback mode"
 *
 * @param state         Platform state (will be modified)
 * @param slot_index    Which slot to play from (1-based)
 * @return              true on success, false on failure
 *
 * After this call, you should call input_recording_playback_frame() each frame.
 */
bool input_recording_playback_begin(GameMemoryState *state, int32 slot_index);

/**
 * Play back one frame of input.
 *
 * Reads the next input from the recording file and writes it to `input`.
 * If we reach the end of the recording, automatically loops back:
 *   - Restores memory snapshot
 *   - Seeks back to first input frame
 *   - Reads first input
 *
 * Does nothing if not currently playing back.
 *
 * @param state     Platform state
 * @param input     Output: will be OVERWRITTEN with recorded input
 */
void input_recording_playback_frame(GameMemoryState *state, GameInput *input);

/**
 * Stop playback.
 *
 * Closes the playback file and clears playback state.
 * Safe to call even if not playing back (does nothing).
 *
 * @param state     Platform state (will be modified)
 */
void input_recording_playback_end(GameMemoryState *state);

// ─────────────────────────────────────────────────────────────────────────────
// QUERY FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Check if currently recording.
 */
de100_file_scoped_fn inline bool input_recording_is_recording(const GameMemoryState *state) {
    return state->input_recording_index != 0;
}

/**
 * Check if currently playing back.
 */
de100_file_scoped_fn inline bool input_recording_is_playing(const GameMemoryState *state) {
    return state->input_playing_index != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// CONVENIENCE: Toggle function (matches Casey's 'L' key behavior)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Toggle recording/playback state.
 *
 * Behavior:
 *   - If not recording and not playing: START RECORDING
 *   - If recording: STOP RECORDING, START PLAYBACK
 *   - If playing: STOP PLAYBACK (return to normal)
 *
 * This matches Casey's 'L' key behavior from Day 23.
 *
 * @param state     Platform state
 */
void input_recording_toggle(GameMemoryState *state);

#endif // DE100_INPUT_RECORDING_H