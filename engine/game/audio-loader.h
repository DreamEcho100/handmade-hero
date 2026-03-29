#ifndef DE100_GAME_AUDIO_LOADER_H
#define DE100_GAME_AUDIO_LOADER_H

#include "../_common/base.h"
#include "../_common/file.h"
#include "../_common/memory.h"
#include "../_common/parsers/wav.h"

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO LOADER
//
// Wraps the raw WAV parser into game-usable loaded sound assets.
// Handles file I/O + parsing in one call for convenience.
//
// Usage:
//   De100LoadedSound snd = de100_audio_load_wav_file("assets/bloop.wav");
//   if (snd.is_valid) {
//     // snd.samples, snd.sample_count, etc. are ready
//     // snd owns the memory — call de100_audio_unload_sound() when done
//   }
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  i16 *samples;         // Interleaved stereo PCM (owned memory)
  u32 sample_count;     // Total frames
  u32 channel_count;    // 1 = mono, 2 = stereo
  u32 sample_rate;      // Hz
  De100MemoryBlock memory; // Backing allocation (for cleanup)
  bool is_valid;
} De100LoadedSound;

// ─────────────────────────────────────────────────────────────────────────
// Load a WAV file from disk into a De100LoadedSound.
//
// Reads the file, parses WAV headers, and copies PCM data into owned memory.
// The returned sound owns its memory — call de100_audio_unload_sound() to free.
// ─────────────────────────────────────────────────────────────────────────

static inline De100LoadedSound
de100_audio_load_wav_file(const char *filepath) {
  De100LoadedSound result = {0};

  // Get file size
  De100FileSizeResult size_result = de100_file_get_size(filepath);
  if (!size_result.success || size_result.value <= 0) {
    printf("[AUDIO_LOADER] Cannot stat '%s'\n", filepath);
    return result;
  }

  size_t file_size = (size_t)size_result.value;

  // Allocate temp buffer for file contents
  De100MemoryBlock file_mem = de100_memory_alloc(
      NULL, file_size, De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE);
  if (!de100_memory_is_valid(file_mem)) {
    printf("[AUDIO_LOADER] Cannot allocate %zu bytes for '%s'\n", file_size,
           filepath);
    return result;
  }

  // Read file
  De100FileOpenResult f = de100_file_open(filepath, DE100_FILE_READ);
  if (!f.success) {
    printf("[AUDIO_LOADER] Cannot open '%s'\n", filepath);
    de100_memory_free(&file_mem);
    return result;
  }

  De100FileIOResult read_result =
      de100_file_read_all(f.fd, file_mem.base, file_size);
  de100_file_close(f.fd);

  if (!read_result.success) {
    printf("[AUDIO_LOADER] Read failed for '%s'\n", filepath);
    de100_memory_free(&file_mem);
    return result;
  }

  // Parse WAV
  De100WavResult wav = de100_wav_parse(file_mem.base, file_size);
  if (!wav.is_valid) {
    printf("[AUDIO_LOADER] WAV parse failed for '%s': %s\n", filepath,
           wav.error);
    de100_memory_free(&file_mem);
    return result;
  }

  if (wav.bits_per_sample != 16) {
    printf("[AUDIO_LOADER] Only 16-bit WAV supported, got %d-bit in '%s'\n",
           wav.bits_per_sample, filepath);
    de100_memory_free(&file_mem);
    return result;
  }

  // Copy PCM data into its own allocation (so we can free the file buffer)
  De100MemoryBlock pcm_mem = de100_memory_alloc(
      NULL, wav.data_size_bytes,
      De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE);
  if (!de100_memory_is_valid(pcm_mem)) {
    printf("[AUDIO_LOADER] Cannot allocate %u bytes for PCM from '%s'\n",
           wav.data_size_bytes, filepath);
    de100_memory_free(&file_mem);
    return result;
  }

  memcpy(pcm_mem.base, wav.samples, wav.data_size_bytes);

  // Free file buffer (PCM is now in its own allocation)
  de100_memory_free(&file_mem);

  // Fill result
  result.samples = (i16 *)pcm_mem.base;
  result.sample_count = wav.sample_count;
  result.channel_count = wav.channel_count;
  result.sample_rate = wav.sample_rate;
  result.memory = pcm_mem;
  result.is_valid = true;

  printf("[AUDIO_LOADER] Loaded '%s': %u frames, %u ch, %u Hz\n", filepath,
         result.sample_count, result.channel_count, result.sample_rate);

  return result;
}

// ─────────────────────────────────────────────────────────────────────────
// Free a loaded sound's memory.
// ─────────────────────────────────────────────────────────────────────────

static inline void de100_audio_unload_sound(De100LoadedSound *sound) {
  if (sound && sound->is_valid) {
    de100_memory_free(&sound->memory);
    sound->samples = NULL;
    sound->is_valid = false;
  }
}

#endif // DE100_GAME_AUDIO_LOADER_H
