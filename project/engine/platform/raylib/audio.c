#include "audio.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

RaylibSoundOutput g_raylib_audio_output = {0};

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 INITIALIZE AUDIO SYSTEM
// ═══════════════════════════════════════════════════════════════════════════
//
// KEY INSIGHT: Raylib audio is fundamentally different from ALSA!
//
// ALSA Model (X11 backend):
//   - Continuous ring buffer with play/write cursors
//   - You calculate how much to write based on cursor positions
//   - Write whenever you want, as long as you don't overrun
//
// Raylib Model:
//   - Double-buffered internally
//   - IsAudioStreamProcessed() returns true when a buffer is consumed
//   - You write EXACTLY buffer_size_frames samples when called
//   - If you try to write when buffer is full, it's rejected
//
// ═══════════════════════════════════════════════════════════════════════════
bool raylib_init_audio(GameAudioOutputBuffer *audio_output,
                       PlatformAudioConfig *audio_config,
                       int32_t samples_per_second, int32_t buffer_size_frames,
                       int32_t game_update_hz) {
  (void)(buffer_size_frames);

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 RAYLIB AUDIO INITIALIZATION\n");
  printf("═══════════════════════════════════════════════════════════\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 1: Initialize Raylib audio device
  // ─────────────────────────────────────────────────────────────────────
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "❌ Audio: Failed to initialize audio device\n");
    audio_config->is_initialized = false;
    return false;
  }

  printf("✅ Audio: Device initialized\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 2: Calculate proper buffer size
  // ─────────────────────────────────────────────────────────────────────
  // For 60 FPS: we need ~800 samples per frame (48000/60)
  // For smooth audio, buffer should be ~2-3 frames worth
  // But not too big or latency increases
  // ─────────────────────────────────────────────────────────────────────

  int32_t samples_per_frame = samples_per_second / game_update_hz;

  // Use 2 frames worth as buffer size for low latency
  // This means we'll get IsAudioStreamProcessed() every ~2 frames
  int32_t actual_buffer_size = samples_per_frame * 2;

  // Clamp to reasonable range
  if (actual_buffer_size < 512)
    actual_buffer_size = 512;
  if (actual_buffer_size > 4096)
    actual_buffer_size = 4096;

  printf("[AUDIO] Samples per frame: %d (at %d Hz game logic)\n",
         samples_per_frame, game_update_hz);
  printf("[AUDIO] Buffer size: %d samples (%.1f ms, ~%.1f frames)\n",
         actual_buffer_size,
         (float)actual_buffer_size / samples_per_second * 1000.0f,
         (float)actual_buffer_size / samples_per_frame);

  // ─────────────────────────────────────────────────────────────────────
  // STEP 3: Configure platform audio config
  // ─────────────────────────────────────────────────────────────────────
  audio_config->samples_per_second = samples_per_second;
  audio_config->bytes_per_sample = sizeof(int16_t) * 2; // 16-bit stereo
  audio_config->running_sample_index = 0;
  audio_config->game_update_hz = game_update_hz;
  audio_config->latency_sample_count = actual_buffer_size;

  // Safety margin: 1/3 of a frame (same as Casey's SafetyBytes)
  audio_config->safety_sample_count = samples_per_frame / 3;

  // Target buffer: 2 frames of audio (provides good latency cushion)
  int32_t target_buffer_samples = samples_per_frame * 2;
  audio_config->secondary_buffer_size =
      target_buffer_samples * audio_config->bytes_per_sample;

  // ─────────────────────────────────────────────────────────────────────
  // STEP 4: Set buffer size BEFORE creating stream
  // ─────────────────────────────────────────────────────────────────────
  SetAudioStreamBufferSizeDefault(actual_buffer_size);

  // ─────────────────────────────────────────────────────────────────────
  // STEP 5: Create audio stream
  // ─────────────────────────────────────────────────────────────────────
  g_raylib_audio_output.stream =
      LoadAudioStream(samples_per_second, // Sample rate
                      16,                 // Bits per sample
                      2                   // Channels (stereo)
      );

  if (!IsAudioStreamValid(g_raylib_audio_output.stream)) {
    fprintf(stderr, "❌ Audio: Failed to create audio stream\n");
    CloseAudioDevice();
    audio_config->is_initialized = false;
    return false;
  }

  g_raylib_audio_output.stream_valid = true;
  // g_raylib_audio_output.buffer_size_frames = actual_buffer_size;
  g_raylib_audio_output.buffer_size_frames = samples_per_frame * 2;

  printf("✅ Audio: Stream created (%d Hz, 16-bit stereo)\n",
         samples_per_second);
  printf("[AUDIO] Stream buffer size: %d frames (%.1f ms)\n",
         g_raylib_audio_output.buffer_size_frames,
         (float)g_raylib_audio_output.buffer_size_frames / samples_per_second *
             1000.0f);

  // Create audio stream with this buffer size
  g_raylib_audio_output.stream = LoadAudioStream(samples_per_second,
                                                 16, // 16-bit
                                                 2   // stereo
  );

  // ─────────────────────────────────────────────────────────────────────
  // STEP 6: Allocate sample buffer for game to fill
  // ─────────────────────────────────────────────────────────────────────
  // Only need enough for one buffer write
  uint32_t buffer_bytes = actual_buffer_size * audio_config->bytes_per_sample;

  g_raylib_audio_output.sample_buffer = platform_allocate_memory(
      NULL, buffer_bytes,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(g_raylib_audio_output.sample_buffer)) {
    fprintf(stderr, "❌ Audio: Failed to allocate sample buffer\n");
    UnloadAudioStream(g_raylib_audio_output.stream);
    CloseAudioDevice();
    audio_config->is_initialized = false;

    fprintf(stderr, "   Error: %s\n",
            g_raylib_audio_output.sample_buffer.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(
                g_raylib_audio_output.sample_buffer.error_code));
    return false;
  }

  g_raylib_audio_output.sample_buffer_size =
      g_raylib_audio_output.buffer_size_frames *
      audio_config->bytes_per_sample * 4;

  g_raylib_audio_output.sample_buffer = platform_allocate_memory(
      NULL, g_raylib_audio_output.sample_buffer_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  printf("✅ Audio: Sample buffer allocated (%d bytes)\n", buffer_bytes);

  // ─────────────────────────────────────────────────────────────────────
  // STEP 7: Configure game audio output buffer
  // ─────────────────────────────────────────────────────────────────────
  audio_output->samples_per_second = samples_per_second;
  audio_output->sample_count = actual_buffer_size; // Fixed size for Raylib
  audio_output->samples_block = g_raylib_audio_output.sample_buffer;

  // ─────────────────────────────────────────────────────────────────────
  // STEP 8: Start playback (Raylib handles pre-buffering internally)
  // ─────────────────────────────────────────────────────────────────────
  PlayAudioStream(g_raylib_audio_output.stream);

  audio_config->is_initialized = true;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 AUDIO SYSTEM INITIALIZED\n");
  printf("═══════════════════════════════════════════════════════════\n");

  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 QUERY SAMPLES TO WRITE
// ═══════════════════════════════════════════════════════════════════════════
// FIXED: Write audio EVERY frame to keep buffer full, not just when empty.
// Raylib's internal buffer needs continuous feeding.
// ═══════════════════════════════════════════════════════════════════════════
int32_t raylib_get_samples_to_write(PlatformAudioConfig *audio_config,
                                    GameAudioOutputBuffer *audio_output) {
  (void)(audio_output);
  if (!audio_config->is_initialized || !g_raylib_audio_output.stream_valid) {
    return 0;
  }

  // CRITICAL: Only write when Raylib's buffer has been consumed
  // This prevents buffer overflow warnings
  if (!IsAudioStreamProcessed(g_raylib_audio_output.stream)) {
    return 0; // Buffer still has data, don't write yet
  }

  // Calculate samples to fill one buffer's worth
  // Use the buffer size we configured the stream with
  int32_t samples_to_write = g_raylib_audio_output.buffer_size_frames;

  // Clamp to our sample buffer capacity
  int32_t max_samples =
      g_raylib_audio_output.sample_buffer_size / audio_config->bytes_per_sample;
  if (samples_to_write > max_samples) {
    samples_to_write = max_samples;
  }

  return samples_to_write;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 SEND SAMPLES TO RAYLIB
// ═══════════════════════════════════════════════════════════════════════════

void raylib_send_samples(PlatformAudioConfig *audio_config,
                         GameAudioOutputBuffer *source) {
  if (!audio_config->is_initialized || !g_raylib_audio_output.stream_valid) {
    return;
  }

  if (!source->samples_block.is_valid || source->sample_count <= 0) {
    return;
  }

  // Ensure stream is playing
  if (!IsAudioStreamPlaying(g_raylib_audio_output.stream)) {
    PlayAudioStream(g_raylib_audio_output.stream);
  }

  // Send samples to Raylib
  UpdateAudioStream(g_raylib_audio_output.stream, source->samples_block.base,
                    source->sample_count);

  // Update running sample index for debugging
  audio_config->running_sample_index += source->sample_count;

  // DEBUG: Track write statistics
#if HANDMADE_INTERNAL
  static int64_t last_log_samples = 0;
  static int write_count = 0;

  write_count++;

  if (FRAME_LOG_EVERY_THREE_SECONDS_CHECK) {
    int64_t samples_written =
        audio_config->running_sample_index - last_log_samples;
    printf("[AUDIO] Writes in last 3s: %d, total samples: %lld\n", write_count,
           (long long)samples_written);
    write_count = 0;
    last_log_samples = audio_config->running_sample_index;
  }
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 CLEAR AUDIO BUFFER (send silence)
// ═══════════════════════════════════════════════════════════════════════════

void raylib_clear_audio_buffer(PlatformAudioConfig *audio_config) {
  if (!audio_config->is_initialized || !g_raylib_audio_output.stream_valid) {
    return;
  }

  // Only send silence if buffer is ready
  if (IsAudioStreamProcessed(g_raylib_audio_output.stream)) {
    memset(g_raylib_audio_output.sample_buffer.base, 0,
           g_raylib_audio_output.buffer_size_frames *
               audio_config->bytes_per_sample);

    UpdateAudioStream(g_raylib_audio_output.stream,
                      g_raylib_audio_output.sample_buffer.base,
                      g_raylib_audio_output.buffer_size_frames);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 DEBUG AUDIO LATENCY
// ═══════════════════════════════════════════════════════════════════════════

void raylib_debug_audio_latency(PlatformAudioConfig *audio_config) {
  if (!audio_config->is_initialized) {
    printf("❌ Audio: Not initialized\n");
    return;
  }

  float runtime_seconds = (float)audio_config->running_sample_index /
                          (float)audio_config->samples_per_second;

  float buffer_latency_ms = (float)g_raylib_audio_output.buffer_size_frames /
                            audio_config->samples_per_second * 1000.0f;

  printf("┌─────────────────────────────────────────────────────────────┐\n");
  printf("│ 🔊 RAYLIB AUDIO DEBUG INFO                                  │\n");
  printf("├─────────────────────────────────────────────────────────────┤\n");
  printf("│ Mode: Double-buffered (Raylib internal)                     │\n");
  printf("│                                                             │\n");
  printf("│ Sample rate:        %6d Hz                               │\n",
         audio_config->samples_per_second);
  printf("│ Bytes per sample:   %6d (16-bit stereo)                  │\n",
         audio_config->bytes_per_sample);
  printf("│ Buffer size:        %6d frames (%.1f ms)                 │\n",
         g_raylib_audio_output.buffer_size_frames, buffer_latency_ms);
  printf("│ Game update rate:   %6d Hz                               │\n",
         audio_config->game_update_hz);
  printf("│                                                             │\n");
  printf("│ Running samples:    %10lld                              │\n",
         (long long)audio_config->running_sample_index);
  printf("│ Runtime:            %10.2f seconds                      │\n",
         runtime_seconds);
  printf("│                                                             │\n");
  printf("│ Stream ready:       %-3s                                    │\n",
         IsAudioStreamValid(g_raylib_audio_output.stream) ? "Yes" : "No");
  printf("│ Stream processed:   %-3s (buffer needs fill)                │\n",
         IsAudioStreamProcessed(g_raylib_audio_output.stream) ? "Yes" : "No");
  printf("│ Stream playing:     %-3s                                    │\n",
         IsAudioStreamPlaying(g_raylib_audio_output.stream) ? "Yes" : "No");
  printf("└─────────────────────────────────────────────────────────────┘\n");

  raylib_debug_audio_overlay();
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 SHUTDOWN AUDIO
// ═══════════════════════════════════════════════════════════════════════════

void raylib_shutdown_audio(GameAudioOutputBuffer *audio_output,
                           PlatformAudioConfig *audio_config) {
  if (!audio_config->is_initialized) {
    return;
  }

  printf("🔊 Shutting down audio...\n");

  if (g_raylib_audio_output.stream_valid) {
    StopAudioStream(g_raylib_audio_output.stream);
    UnloadAudioStream(g_raylib_audio_output.stream);
    g_raylib_audio_output.stream_valid = false;
  }

  if (platform_memory_is_valid(g_raylib_audio_output.sample_buffer)) {
    platform_free_memory(&g_raylib_audio_output.sample_buffer);
  }

  CloseAudioDevice();

  audio_config->is_initialized = false;
  platform_free_memory(&audio_output->samples_block);
  audio_output->sample_count = 0;

  printf("✅ Audio: Shutdown complete\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 HANDLE FPS CHANGE
// ═══════════════════════════════════════════════════════════════════════════

void raylib_audio_fps_change_handling(GameAudioOutputBuffer *audio_output,
                                      PlatformAudioConfig *audio_config) {
  (void)audio_output;
  (void)audio_config;
  // For Raylib, buffer size is fixed at init time
  // Would need to recreate stream to change it
  printf("[AUDIO] Note: FPS change doesn't affect Raylib audio buffer size\n");
}

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│                    AUDIO ARCHITECTURE COMPARISON                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  X11/ALSA (Low-Level)                │  RAYLIB (High-Level)                │
│  ─────────────────────               │  ───────────────────                │
│                                      │                                      │
│  YOU control the ring buffer:        │  RAYLIB controls the ring buffer:   │
│  ┌────────────────────────┐          │  ┌────────────────────────┐         │
│  │  ▼ Play Cursor         │          │  │  ??? (hidden)          │         │
│  │  ████████░░░░░░░░░░░░░░│          │  │  ????????????????      │         │
│  │           ▲ Write Cursor│          │  │  ??? (hidden)          │         │
│  └────────────────────────┘          │  └────────────────────────┘         │
│                                      │                                      │
│  You can query:                      │  You can only ask:                  │
│  - snd_pcm_delay()                   │  - IsAudioStreamProcessed()         │
│  - snd_pcm_avail()                   │    (true/false, no position info)   │
│  - Calculate exact positions         │                                      │
│                                      │                                      │
│  Debug markers SHOW:                 │  Debug markers would show:           │
│  - Where audio is playing            │  - Nothing useful! 😅               │
│  - Where we're writing               │  - We don't have cursor access      │
│  - Predicted flip position           │  - Raylib handles timing internally │
│  - Latency visualization             │                                      │
│                                      │                                      │
└─────────────────────────────────────────────────────────────────────────────┘
*/

void raylib_debug_audio_overlay(void) {
  if (!g_raylib_audio_output.stream_valid)
    return;

  char stats[256];
  snprintf(stats, sizeof(stats),
           "Audio: %lld samples written | %d writes/period | %.1f ms latency "
           "estimate",
           (long long)g_raylib_audio_output.total_samples_written,
           g_raylib_audio_output.writes_this_period,
           (float)g_raylib_audio_output.buffer_size_frames / 48000.0f *
               1000.0f);

  DrawText(stats, 10, 10, 16, GREEN);
}
