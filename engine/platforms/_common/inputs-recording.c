// ┌─────────────────────────────────────────────────────────────────────────┐
// │              MEMORY-MAPPED FILES: THE CONCEPT                           │
// ├─────────────────────────────────────────────────────────────────────────┤
// │                                                                         │
// │  TRADITIONAL FILE I/O (Your current approach):                          │
// │  ─────────────────────────────────────────────                          │
// │                                                                         │
// │  ┌──────────────┐                      ┌──────────────┐                │
// │  │ Game Memory  │                      │  Disk File   │                │
// │  │   (1+ GB)    │                      │  (1+ GB)     │                │
// │  └──────────────┘                      └──────────────┘                │
// │         │                                     ▲                         │
// │         │  write_all()                        │                         │
// │         │  ════════════════════════════════►  │                         │
// │         │  (Copy ALL bytes through kernel)    │                         │
// │         │  Time: ~1-5 seconds for 1GB         │                         │
// │         │                                     │                         │
// │         │  read_all()                         │                         │
// │         │  ◄════════════════════════════════  │                         │
// │         │  (Copy ALL bytes through kernel)    │                         │
// │         │  Time: ~1-5 seconds for 1GB         │                         │
// │                                                                         │
// │  MEMORY-MAPPED FILE (Day 25 approach):                                  │
// │  ─────────────────────────────────────                                  │
// │                                                                         │
// │  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐            │
// │  │ Game Memory  │     │ Mapped Region│     │  Disk File   │            │
// │  │   (1+ GB)    │     │   (1+ GB)    │     │  (1+ GB)     │            │
// │  └──────────────┘     └──────────────┘     └──────────────┘            │
// │         │                    ▲                    ▲                     │
// │         │                    │                    │                     │
// │         │  memcpy()          │    OS handles      │                     │
// │         │  ═══════════►      │    sync lazily     │                     │
// │         │  RAM-to-RAM        │    in background   │                     │
// │         │  ~100ms for 1GB!   │                    │                     │
// │         │                    │                    │                     │
// │         │  memcpy()          │                    │                     │
// │         │  ◄═══════════      │                    │                     │
// │         │  RAM-to-RAM        │                    │                     │
// │         │  ~100ms for 1GB!   │                    │                     │
// │                                                                         │
// │  KEY INSIGHT:                                                           │
// │  ─────────────                                                          │
// │  The mapped region IS memory. You can read/write it like any pointer.  │
// │  The OS handles syncing to disk in the background.                      │
// │  For our use case (looping replay), we don't even need disk sync -     │
// │  we just need fast RAM-to-RAM copies!                                   │
// │                                                                         │
// │  Web Analogy:                                                           │
// │  ─────────────                                                          │
// │  Traditional I/O = fetch() with await (blocking, slow)                  │
// │  Memory-mapped  = SharedArrayBuffer (direct memory access)              │
// │                                                                         │
// └─────────────────────────────────────────────────────────────────────────┘

// ┌─────────────────────────────────────────────────────────────────────────┐
// │              WINDOWS vs LINUX MEMORY MAPPING                            │
// ├─────────────────────────────────────────────────────────────────────────┤
// │                                                                         │
// │  WINDOWS (Casey's code):                                                │
// │  ───────────────────────                                                │
// │                                                                         │
// │  1. CreateFileA(filename, ...)                                          │
// │     └─→ Open/create the file                                            │
// │                                                                         │
// │  2. CreateFileMapping(file_handle, ..., size, ...)                      │
// │     └─→ Create a mapping object                                         │
// │                                                                         │
// │  3. MapViewOfFile(mapping_handle, ..., size)                            │
// │     └─→ Map into address space, get pointer                             │
// │                                                                         │
// │  4. Use the pointer like normal memory!                                 │
// │                                                                         │
// │  5. UnmapViewOfFile(pointer)                                            │
// │     CloseHandle(mapping_handle)                                         │
// │     CloseHandle(file_handle)                                            │
// │                                                                         │
// │  ═══════════════════════════════════════════════════════════════════   │
// │                                                                         │
// │  LINUX (Your implementation):                                           │
// │  ────────────────────────────                                           │
// │                                                                         │
// │  1. open(filename, O_RDWR | O_CREAT, 0644)                              │
// │     └─→ Open/create the file                                            │
// │                                                                         │
// │  2. ftruncate(fd, size)                                                 │
// │     └─→ Set file size (REQUIRED before mmap!)                           │
// │                                                                         │
// │  3. mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)           │
// │     └─→ Map into address space, get pointer                             │
// │     └─→ MAP_SHARED = changes visible to file                            │
// │     └─→ MAP_PRIVATE = copy-on-write (changes NOT saved)                 │
// │                                                                         │
// │  4. Use the pointer like normal memory!                                 │
// │                                                                         │
// │  5. munmap(pointer, size)                                               │
// │     close(fd)                                                           │
// │                                                                         │
// │  KEY DIFFERENCE:                                                        │
// │  ───────────────                                                        │
// │  Linux combines steps 2+3 into one mmap() call.                         │
// │  But you MUST ftruncate() first to set the file size!                   │
// │                                                                         │
// └─────────────────────────────────────────────────────────────────────────┘
//

#include "inputs-recording.h"
#include "../../_common/file.h"
#include "./replay-buffer.h"
#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Generate filename for input events (not state - that's in replay buffer).
 */
de100_file_scoped_fn inline void get_input_filename(const char *exe_directory,
                                                    int32 slot_index,
                                                    char *buffer,
                                                    size_t buffer_size) {
  // Day 25 naming: loop_edit_N_input.hmi
  snprintf(buffer, buffer_size, "%sloop_edit_%d_input.hmi", exe_directory,
           slot_index);
}

// ═══════════════════════════════════════════════════════════════════════════
// RECORDING IMPLEMENTATION (Updated for Day 25)
// ═══════════════════════════════════════════════════════════════════════════

bool input_recording_begin(const char *exe_directory, GameMemoryState *state,
                           int32 slot_index) {
  if (!state) {
    fprintf(stderr, "[INPUT RECORDING] ERROR: NULL state\n");
    return false;
  }

  if (!state->game_memory || state->total_size == 0) {
    fprintf(stderr, "[INPUT RECORDING] ERROR: Game memory not set up\n");
    return false;
  }

  if (state->input_recording_index != 0) {
    fprintf(stderr, "[INPUT RECORDING] WARNING: Already recording to slot %d\n",
            state->input_recording_index);
    return false;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Get replay buffer for this slot
  // ─────────────────────────────────────────────────────────────────────

  ReplayBuffer *replay_buffer =
      replay_buffer_get(state->replay_buffers, slot_index);
  if (!replay_buffer_is_valid(replay_buffer)) {
    fprintf(stderr,
            "[INPUT RECORDING] ERROR: Replay buffer slot %d not valid\n",
            slot_index);
    return false;
  }

  // Open file for input events only (state goes to replay buffer)
  char input_filename[256];
  get_input_filename(exe_directory, slot_index, input_filename,
                     sizeof(input_filename));

  De100FileOpenResult open_result =
      de100_file_open(input_filename, DE100_FILE_WRITE | DE100_FILE_CREATE |
                                          DE100_FILE_TRUNCATE);

  if (!open_result.success) {
    fprintf(stderr, "[INPUT RECORDING] Failed to create input file: %s\n",
            de100_file_strerror(open_result.error_code));
    return false;
  }

  printf("[INPUT RECORDING] 📼 Starting recording to slot %d\n", slot_index);

  // ─────────────────────────────────────────────────────────────────────
  // FAST: Save state to memory-mapped replay buffer (memcpy, not file I/O!)
  // ─────────────────────────────────────────────────────────────────────
  printf("[INPUT RECORDING] 📼 Starting recording to slot %d\n", slot_index);

  ReplayBufferResult save_result = replay_buffer_save_state(
      replay_buffer, state->game_memory, state->total_size);
  if (!save_result.success) {
    fprintf(stderr, "[INPUT RECORDING] Failed to save state: %s\n",
            replay_buffer_strerror(save_result.error_code));
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

  // Write input frame to file (this is small, file I/O is fine)
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
// PLAYBACK IMPLEMENTATION (Updated for Day 25)
// ═══════════════════════════════════════════════════════════════════════════

bool input_recording_playback_begin(const char *exe_directory,
                                    GameMemoryState *state, int32 slot_index) {
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

  // ─────────────────────────────────────────────────────────────────────
  // Day 25 Change: Use replay buffer for state restore
  // ─────────────────────────────────────────────────────────────────────
  ReplayBuffer *replay_buffer =
      replay_buffer_get(state->replay_buffers, slot_index);
  if (!replay_buffer_is_valid(replay_buffer)) {
    fprintf(stderr, "[INPUT PLAYBACK] ERROR: Replay buffer slot %d not valid\n",
            slot_index);
    return false;
  }

  // Open file for input events
  char input_filename[256];
  get_input_filename(exe_directory, slot_index, input_filename,
                     sizeof(input_filename));

  De100FileOpenResult open_result =
      de100_file_open(input_filename, DE100_FILE_READ);

  if (!open_result.success) {
    fprintf(stderr, "[INPUT PLAYBACK] Failed to open input file: %s\n",
            de100_file_strerror(open_result.error_code));
    return false;
  }

  printf("[INPUT PLAYBACK] ▶️ Starting playback from slot %d\n", slot_index);

  // ─────────────────────────────────────────────────────────────────────
  // FAST: Restore state from memory-mapped replay buffer (memcpy!)
  // ─────────────────────────────────────────────────────────────────────
  ReplayBufferResult restore_result = replay_buffer_restore_state(
      replay_buffer, state->game_memory, state->total_size);

  if (!restore_result.success) {
    fprintf(stderr, "[INPUT PLAYBACK] Failed to restore state: %s\n",
            replay_buffer_strerror(restore_result.error_code));
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

  if (read_result.success) {
    return;
  }

  if (!read_result.success && read_result.error_code != DE100_FILE_ERROR_EOF) {
    fprintf(stderr, "[INPUT PLAYBACK] Failed to read input: %s\n",
            de100_file_strerror(read_result.error_code));
    input_recording_playback_end(state);
    return;
  }

  // End of input stream - loop back!
  int32 slot = state->input_playing_index;
  printf("[INPUT PLAYBACK] 🔄 Looping back to start (slot %d)\n", slot);

  // Seek back to beginning of input file
  De100FileSizeResult seek_result =
      de100_file_seek(state->playback_fd, 0, DE100_SEEK_SET);

  if (!seek_result.success) {
    fprintf(stderr, "[INPUT PLAYBACK] Failed to seek: %s\n",
            de100_file_strerror(seek_result.error_code));
    input_recording_playback_end(state);
    return;
  }

  // ─────────────────────────────────────────────────────────────────
  // FAST: Restore state from replay buffer (memcpy, not file read!)
  // ─────────────────────────────────────────────────────────────────

  ReplayBuffer *replay_buffer = replay_buffer_get(state->replay_buffers, slot);
  if (!replay_buffer_is_valid(replay_buffer)) {
    fprintf(stderr, "[INPUT PLAYBACK] Replay buffer became invalid\n");
    input_recording_playback_end(state);
    return;
  }

  ReplayBufferResult restore_result = replay_buffer_restore_state(
      replay_buffer, state->game_memory, state->total_size);

  if (!restore_result.success) {
    fprintf(stderr, "[INPUT PLAYBACK] Failed to restore state on loop: %s\n",
            replay_buffer_strerror(restore_result.error_code));
    input_recording_playback_end(state);
    return;
  }

  // Read first input frame
  read_result =
      de100_file_read_all(state->playback_fd, input, sizeof(GameInput));

  if (!read_result.success) {
    fprintf(stderr, "[INPUT PLAYBACK] Failed to read first input on loop: %s\n",
            de100_file_strerror(read_result.error_code));
    input_recording_playback_end(state);
    return;
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

// ═══════════════════════════════════════════════════════════════════════════
// TOGGLE FUNCTION (Updated for Day 25 - Can Exit Playback!)
// ═══════════════════════════════════════════════════════════════════════════

INPUT_RECORDING_TOGGLE_RESULT_CODE
input_recording_toggle(const char *exe_directory, GameMemoryState *state) {
  // Day 25 change: Check playback FIRST, so L can exit playback
  if (input_recording_is_playing(state)) {
    // NEW in Day 25: Can stop playback!
    printf("[INPUT RECORDING] 🛑 Stopping playback\n");
    input_recording_playback_end(state);
    return INPUT_RECORDING_TOGGLE_STOPPED_PLAYBACK;
  } else if (input_recording_is_recording(state)) {
    printf("[INPUT RECORDING] 📼→▶️ Switching from recording to playback\n");
    int32 slot = state->input_recording_index;
    input_recording_end(state);
    input_recording_playback_begin(exe_directory, state, slot);
    return INPUT_RECORDING_TOGGLE_SWITCHED_TO_PLAYBACK;
  } else {
    printf("[INPUT RECORDING] ⏺️ Starting recording\n");
    input_recording_begin(exe_directory, state, 1); // Use slot 1 by default
    return INPUT_RECORDING_TOGGLE_STARTTED_RECORDING;
  }
}
