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
                       i32 samples_per_second, i32 game_update_hz) {

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 RAYLIB AUDIO INITIALIZATION\n");
  printf("═══════════════════════════════════════════════════════════\n");

  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "❌ Audio: Failed to initialize audio device\n");
    if (audio_output)
      audio_output->is_initialized = false;
    return false;
  }

  printf("✅ Audio: Device initialized\n");

  i32 samples_per_frame = samples_per_second / game_update_hz;
  i32 buffer_size = samples_per_frame * 2; // ~2 frames

  if (buffer_size < 512)
    buffer_size = 512;
  if (buffer_size > 4096)
    buffer_size = 4096;

  printf("[AUDIO] Samples per frame: %d (at %d Hz game logic)\n",
         samples_per_frame, game_update_hz);
  printf("[AUDIO] Buffer size: %d samples (%.1f ms, ~%.1f frames)\n",
         buffer_size, (float)buffer_size / samples_per_second * 1000.0f,
         (float)buffer_size / samples_per_frame);

  // ───────
  // Store sample rate so game can read it back if needed
  if (audio_output)
    audio_output->samples_per_second = samples_per_second;

  // Raylib-private config lives in g_raylib_audio_output; nothing goes into
  // a shared PlatformAudioConfig anymore.

  // Set buffer size BEFORE creating stream
  SetAudioStreamBufferSizeDefault(buffer_size);

  // Create stream ONCE
  g_raylib_audio_output.stream = LoadAudioStream(samples_per_second, 16, 2);

  if (!IsAudioStreamValid(g_raylib_audio_output.stream)) {
    fprintf(stderr, "❌ Audio: Failed to create stream\n");
    CloseAudioDevice();
    if (audio_output)
      audio_output->is_initialized = false;
    return false;
  }

  g_raylib_audio_output.stream_valid = true;
  g_raylib_audio_output.buffer_size_frames = buffer_size;

  printf("✅ Audio: Stream created (%d Hz, 16-bit stereo)\n",
         samples_per_second);
  printf("[AUDIO] Stream buffer size: %d frames (%.1f ms)\n",
         g_raylib_audio_output.buffer_size_frames,
         (float)g_raylib_audio_output.buffer_size_frames / samples_per_second *
             1000.0f);

  // Allocate buffer ONCE
  const i32 bytes_per_sample = (i32)(sizeof(i16) * 2); // 16-bit stereo
  u32 buffer_bytes = (u32)buffer_size * (u32)bytes_per_sample;
  g_raylib_audio_output.sample_buffer =
      de100_memory_alloc(NULL, buffer_bytes,
                         De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE |
                             De100_MEMORY_FLAG_ZEROED);

  if (!de100_memory_is_valid(g_raylib_audio_output.sample_buffer)) {
    fprintf(stderr, "❌ Audio: Failed to allocate buffer\n");
    UnloadAudioStream(g_raylib_audio_output.stream);
    CloseAudioDevice();
    if (audio_output)
      audio_output->is_initialized = false;
    return false;
  }

  printf("✅ Audio: Sample buffer allocated (%d bytes)\n",
         g_raylib_audio_output.sample_buffer_size);

  g_raylib_audio_output.sample_buffer_size = buffer_bytes;

  PlayAudioStream(g_raylib_audio_output.stream);
  if (audio_output) {
    audio_output->format = AUDIO_FORMAT_I16; /* Raylib stream: 16-bit signed */
    audio_output->is_initialized = true;
  }

  printf("✅ Audio: %d Hz, %d sample buffer\n", samples_per_second,
         buffer_size);
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 QUERY SAMPLES TO WRITE
// ═══════════════════════════════════════════════════════════════════════════
// FIXED: Write audio EVERY frame to keep buffer full, not just when empty.
// Raylib's internal buffer needs continuous feeding.
// ═══════════════════════════════════════════════════════════════════════════
u32 raylib_get_samples_to_write(GameAudioOutputBuffer *audio_output) {
  if (!audio_output->is_initialized || !g_raylib_audio_output.stream_valid) {
    return 0;
  }

  // Check if ANY buffer is ready
  if (!IsAudioStreamProcessed(g_raylib_audio_output.stream)) {
    return 0;
  }

  return g_raylib_audio_output.buffer_size_frames;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 SEND SAMPLES TO RAYLIB
// ═══════════════════════════════════════════════════════════════════════════

void raylib_send_samples(GameAudioOutputBuffer *audio_output) {
  if (!audio_output->is_initialized || !g_raylib_audio_output.stream_valid) {
    return;
  }

  if (audio_output->sample_count <= 0) {
    return;
  }

  // Ensure stream is playing
  if (!IsAudioStreamPlaying(g_raylib_audio_output.stream)) {
    PlayAudioStream(g_raylib_audio_output.stream);
  }

  // Send samples to Raylib
  UpdateAudioStream(g_raylib_audio_output.stream, audio_output->samples_buffer,
                    audio_output->sample_count);

  // Track total written for debug overlay
  g_raylib_audio_output.total_samples_written += audio_output->sample_count;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 CLEAR AUDIO BUFFER (send silence)
// ═══════════════════════════════════════════════════════════════════════════

void raylib_clear_audio_buffer(GameAudioOutputBuffer *audio_output) {
  if (!audio_output->is_initialized || !g_raylib_audio_output.stream_valid) {
    return;
  }

  // Only send silence if buffer is ready
  if (IsAudioStreamProcessed(g_raylib_audio_output.stream)) {
    const i32 bytes_per_sample = (i32)(sizeof(i16) * 2);
    de100_mem_set(g_raylib_audio_output.sample_buffer.base, 0,
                  (u32)g_raylib_audio_output.buffer_size_frames *
                      (u32)bytes_per_sample);

    UpdateAudioStream(g_raylib_audio_output.stream,
                      g_raylib_audio_output.sample_buffer.base,
                      g_raylib_audio_output.buffer_size_frames);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 DEBUG AUDIO LATENCY
// ═══════════════════════════════════════════════════════════════════════════

void raylib_debug_audio_latency(GameAudioOutputBuffer *audio_output) {
  if (!audio_output->is_initialized) {
    printf("\u274c Audio: Not initialized\n");
    return;
  }

  float runtime_seconds = (float)g_raylib_audio_output.total_samples_written /
                          (float)audio_output->samples_per_second;

  float buffer_latency_ms = (float)g_raylib_audio_output.buffer_size_frames /
                            (float)audio_output->samples_per_second * 1000.0f;

  printf("\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n");
  printf("\u2502 [*] RAYLIB AUDIO DEBUG INFO                                  "
         "\u2502\n");
  printf("\u251c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2524\n");
  printf("\u2502 Mode: Double-buffered (Raylib internal)                     "
         "\u2502\n");
  printf("\u2502                                                             "
         "\u2502\n");
  printf("\u2502 Sample rate:        %6d Hz                               "
         "\u2502\n",
         audio_output->samples_per_second);
  printf("\u2502 Bytes per sample:   %6d (16-bit stereo)                  "
         "\u2502\n",
         (int)(sizeof(i16) * 2));
  printf("\u2502 Buffer size:        %6d frames (%.1f ms)                 "
         "\u2502\n",
         g_raylib_audio_output.buffer_size_frames, buffer_latency_ms);
  printf("\u2502                                                             "
         "\u2502\n");
  printf(
      "\u2502 Total samples:      %10lld                              \u2502\n",
      (long long)g_raylib_audio_output.total_samples_written);
  printf(
      "\u2502 Runtime:            %10.2f seconds                      \u2502\n",
      runtime_seconds);
  printf("\u2502                                                             "
         "\u2502\n");
  printf("\u2502 Stream ready:       %-3s                                    "
         "\u2502\n",
         IsAudioStreamValid(g_raylib_audio_output.stream) ? "Yes" : "No");
  printf("\u2502 Stream processed:   %-3s (buffer needs fill)                "
         "\u2502\n",
         IsAudioStreamProcessed(g_raylib_audio_output.stream) ? "Yes" : "No");
  printf("\u2502 Stream playing:     %-3s                                    "
         "\u2502\n",
         IsAudioStreamPlaying(g_raylib_audio_output.stream) ? "Yes" : "No");
  printf("\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
         "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n");

  raylib_debug_audio_overlay();
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 SHUTDOWN AUDIO
// ═══════════════════════════════════════════════════════════════════════════

void raylib_shutdown_audio(GameAudioOutputBuffer *audio_output) {
  if (!audio_output || !audio_output->is_initialized) {
    return;
  }

  printf("🔊 Shutting down audio...\n");

  if (g_raylib_audio_output.stream_valid) {
    StopAudioStream(g_raylib_audio_output.stream);
    UnloadAudioStream(g_raylib_audio_output.stream);
    g_raylib_audio_output.stream_valid = false;
  }

  if (de100_memory_is_valid(g_raylib_audio_output.sample_buffer)) {
    de100_memory_free(&g_raylib_audio_output.sample_buffer);
  }

  CloseAudioDevice();

  audio_output->is_initialized = false;
  audio_output->sample_count = 0;

  printf("✅ Audio: Shutdown complete\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 HANDLE FPS CHANGE
// ═══════════════════════════════════════════════════════════════════════════

void raylib_audio_fps_change_handling(GameAudioOutputBuffer *audio_output) {
  (void)audio_output;
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
