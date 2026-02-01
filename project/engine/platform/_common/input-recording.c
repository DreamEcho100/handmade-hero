#include "input-recording.h"
#include "../../_common/file.h"

#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Generate filename for a recording slot.
 */
de100_file_scoped_fn inline void
get_recording_filename(int32 slot_index, char *buffer, size_t buffer_size) {
  // slot_index is used to support multiple recording slots.
  snprintf(buffer, buffer_size, "out/recording-%d.hmi", slot_index);
}

// ═══════════════════════════════════════════════════════════════════════════
// RECORDING IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════

bool input_recording_begin(GameMemoryState *state, int32 slot_index) {
  if (!state) {
    fprintf(stderr, "[INPUT RECORDING] ERROR: NULL state\n");
    return false;
  }

  if (!state->game_memory || state->total_size == 0) {
    fprintf(stderr, "[INPUT RECORDING] ERROR: Game memory not set up\n");
    fprintf(stderr, "  game_memory_block=%p, total_size=%llu\n",
            state->game_memory, (unsigned long long)state->total_size);
    return false;
  }

  if (state->input_recording_index != 0) {
    fprintf(stderr, "[INPUT RECORDING] WARNING: Already recording to slot %d\n",
            state->input_recording_index);
    return false;
  }

  char filename[256];
  get_recording_filename(slot_index, filename, sizeof(filename));

  De100FileOpenResult open_result = de100_file_open(
      filename, DE100_FILE_WRITE | DE100_FILE_CREATE | DE100_FILE_TRUNCATE);

  if (!open_result.success) {
    fprintf(stderr, "[INPUT RECORDING] Failed to create recording file: %s\n",
            de100_file_strerror(open_result.error_code));
    return false;
  }

  printf("[INPUT RECORDING] 📼 Starting recording to '%s'\n", filename);
  printf("[INPUT RECORDING] Saving memory snapshot: %llu bytes\n",
         (unsigned long long)state->total_size);

  De100FileIOResult write_result = de100_file_write_all(
      open_result.fd, state->game_memory, state->total_size);

  if (!write_result.success) {
    fprintf(stderr, "[INPUT RECORDING] Failed to write memory snapshot: %s\n",
            de100_file_strerror(write_result.error_code));
    de100_file_close(open_result.fd);
    return false;
  }

  state->recording_fd = open_result.fd;
  state->input_recording_index = slot_index;

  printf("[INPUT RECORDING] ✅ Recording started (slot %d)\n", slot_index);
  return true;
}

void input_recording_record_frame(GameMemoryState *state,
                                  const GameInput *input) {
  if (state->input_recording_index == 0) {
    return;
  }

  De100FileIOResult result =
      de100_file_write_all(state->recording_fd, input, sizeof(GameInput));

  if (!result.success) {
    fprintf(stderr, "[INPUT RECORDING] Failed to write input frame: %s\n",
            de100_file_strerror(result.error_code));
    input_recording_end(state);
  }
}

void input_recording_end(GameMemoryState *state) {
  if (state->input_recording_index == 0) {
    return;
  }

  printf("[INPUT RECORDING] ⏹️ Stopping recording (slot %d)\n",
         state->input_recording_index);

  de100_file_close(state->recording_fd);
  state->recording_fd = -1;
  state->input_recording_index = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// PLAYBACK IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════

bool input_recording_playback_begin(GameMemoryState *state, int32 slot_index) {
  if (!state) {
    fprintf(stderr, "[INPUT PLAYBACK] ERROR: NULL state\n");
    return false;
  }

  if (!state->game_memory || state->total_size == 0) {
    fprintf(stderr, "[INPUT PLAYBACK] ERROR: Game memory not set up\n");
    return false;
  }

  if (state->input_playing_index != 0) {
    fprintf(stderr, "[INPUT PLAYBACK] WARNING: Already playing from slot %d\n",
            state->input_playing_index);
    return false;
  }

  char filename[256];
  get_recording_filename(slot_index, filename, sizeof(filename));

  De100FileOpenResult open_result = de100_file_open(filename, DE100_FILE_READ);

  if (!open_result.success) {
    fprintf(stderr, "[INPUT PLAYBACK] Failed to open recording file: %s\n",
            de100_file_strerror(open_result.error_code));
    return false;
  }

  printf("[INPUT PLAYBACK] ▶️ Starting playback from '%s'\n", filename);
  printf("[INPUT PLAYBACK] Restoring memory snapshot: %llu bytes\n",
         (unsigned long long)state->total_size);

  De100FileIOResult read_result = de100_file_read_all(
      open_result.fd, state->game_memory, state->total_size);

  if (!read_result.success) {
    fprintf(stderr, "[INPUT PLAYBACK] Failed to read memory snapshot: %s\n",
            de100_file_strerror(read_result.error_code));
    de100_file_close(open_result.fd);
    return false;
  }

  state->playback_fd = open_result.fd;
  state->input_playing_index = slot_index;

  printf("[INPUT PLAYBACK] ✅ Playback started (slot %d)\n", slot_index);
  return true;
}

void input_recording_playback_frame(GameMemoryState *state, GameInput *input) {
  if (state->input_playing_index == 0) {
    return;
  }

  De100FileIOResult read_result =
      de100_file_read_all(state->playback_fd, input, sizeof(GameInput));

  // TODO: should use the `read_result.error_code == FILE_ERROR_EOF` instead of
  // `!read_result.success`
  if (!read_result.success) {
    int32 slot = state->input_playing_index;
    printf("[INPUT PLAYBACK] 🔄 Looping back to start (slot %d)\n", slot);

    // Seek back to beginning of file
    De100FileSizeResult seek_result =
        de100_file_seek(state->playback_fd, 0, DE100_SEEK_SET);

    if (!seek_result.success) {
      fprintf(stderr, "[INPUT PLAYBACK] Failed to seek: %s\n",
              de100_file_strerror(seek_result.error_code));
      input_recording_playback_end(state);
      return;
    }

    // Re-read and restore memory snapshot
    read_result = de100_file_read_all(state->playback_fd, state->game_memory,
                                      state->total_size);

    if (!read_result.success) {
      fprintf(stderr, "[INPUT PLAYBACK] Failed to restore memory on loop: %s\n",
              de100_file_strerror(read_result.error_code));
      input_recording_playback_end(state);
      return;
    }

    // Read first input frame
    read_result =
        de100_file_read_all(state->playback_fd, input, sizeof(GameInput));

    if (!read_result.success) {
      fprintf(stderr,
              "[INPUT PLAYBACK] Failed to read first input on loop: %s\n",
              de100_file_strerror(read_result.error_code));
      input_recording_playback_end(state);
      return;
    }
  }
}

void input_recording_playback_end(GameMemoryState *state) {
  if (state->input_playing_index == 0) {
    return;
  }

  printf("[INPUT PLAYBACK] ⏹️ Stopping playback (slot %d)\n",
         state->input_playing_index);

  de100_file_close(state->playback_fd);
  state->playback_fd = -1;
  state->input_playing_index = 0;
}

/**
 * ═══════════════════════════════════════════════════════════════════
 * TOGGLE FUNCTION
 * ═══════════════════════════════════════════════════════════════════
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │             STATE MACHINE                                       │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │                  ┌─────────────┐                                │
 * │                  │             │                                │
 * │                  │    IDLE     │◄─────────────────────┐         │
 * │                  │             │                      │         │
 * │                  └──────┬──────┘                      │         │
 * │                         │                             │         │
 * │                  Press L│                             │         │
 * │                         ▼                             │         │
 * │                  ┌─────────────┐                      │         │
 * │                  │             │                      │         │
 * │                  │  RECORDING  │                      │         │
 * │                  │             │                      │         │
 * │                  └──────┬──────┘                      │         │
 * │                         │                             │         │
 * │                  Press L│                             │         │
 * │                         ▼                             │         │
 * │                  ┌─────────────┐                      │         │
 * │                  │             │                      │         │
 * │          ┌──────►│  PLAYBACK   │──────┐               │         │
 * │          │       │             │      │               │         │
 * │          │       └─────────────┘      │               │         │
 * │          │                            │               │         │
 * │          │       End of input         │  Press L      │         │
 * │          │       stream reached       │               │         │
 * │          │                            │               │         │
 * │          └────────────────────────────┘               │         │
 * │                  (loop back)                          │         │
 * │                                                       │         │
 * │                                          ─────────────┘         │
 * │                                          (stop playback)        │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 */

void input_recording_toggle(GameMemoryState *state) {
  if (input_recording_is_playing(state)) {
    printf("[INPUT RECORDING] 🛑 Stopping playback\n");
    input_recording_playback_end(state);
  } else if (input_recording_is_recording(state)) {
    printf("[INPUT RECORDING] 📼→▶️ Switching from recording to playback\n");
    int32 slot = state->input_recording_index;
    input_recording_end(state);
    input_recording_playback_begin(state, slot);
  } else {
    printf("[INPUT RECORDING] ⏺️ Starting recording\n");
    input_recording_begin(state, 1);
  }
}
