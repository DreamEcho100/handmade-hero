#include "audio.h"

#include <raylib.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

RaylibSoundOutput g_raylib_audio_output = {0};

// ═══════════════════════════════════════════════════════════════════════════
// RING BUFFER HELPERS (SPSC - single producer, single consumer)
// ═══════════════════════════════════════════════════════════════════════════

static inline u32 ring_available_to_read(u32 write_pos, u32 read_pos,
                                         u32 capacity) {
  return (write_pos - read_pos + capacity) % capacity;
}

static inline u32 ring_available_to_write(u32 write_pos, u32 read_pos,
                                          u32 capacity) {
  u32 readable = ring_available_to_read(write_pos, read_pos, capacity);
  u32 max_writable = capacity - g_raylib_audio_output.bytes_per_frame;
  if (readable >= max_writable)
    return 0;
  return max_writable - readable;
}

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO CALLBACK (runs on miniaudio's audio thread)
//
// Only does memcpy + atomic store. No game code, no allocations.
// ═══════════════════════════════════════════════════════════════════════════

static void raylib_audio_callback(void *buffer_data, unsigned int frames) {
  RaylibSoundOutput *out = &g_raylib_audio_output;
  u32 bytes_needed = frames * out->bytes_per_frame;
  u32 capacity = out->ring_buffer_size_bytes;

  atomic_fetch_add_explicit(&out->callback_calls, 1, memory_order_relaxed);
  atomic_store_explicit(&out->callback_last_frames, (u32)frames,
                        memory_order_relaxed);
  atomic_fetch_add_explicit(&out->callback_total_frames, (u32)frames,
                            memory_order_relaxed);

  u32 wp = atomic_load_explicit(&out->ring_write_pos, memory_order_acquire);
  u32 rp = atomic_load_explicit(&out->ring_read_pos, memory_order_relaxed);
  u32 available = ring_available_to_read(wp, rp, capacity);

  u8 *dst = (u8 *)buffer_data;
  u8 *ring = (u8 *)out->ring_buffer.base;

  if (available >= bytes_needed) {
    u32 first_chunk = capacity - rp;
    if (first_chunk >= bytes_needed) {
      memcpy(dst, ring + rp, bytes_needed);
    } else {
      memcpy(dst, ring + rp, first_chunk);
      memcpy(dst + first_chunk, ring, bytes_needed - first_chunk);
    }
    u32 new_rp = (rp + bytes_needed) % capacity;
    atomic_store_explicit(&out->ring_read_pos, new_rp, memory_order_release);
  } else {
    // Underrun: copy what we have, zero-pad the rest
    if (available > 0) {
      u32 first_chunk = capacity - rp;
      if (first_chunk >= available) {
        memcpy(dst, ring + rp, available);
      } else {
        memcpy(dst, ring + rp, first_chunk);
        memcpy(dst + first_chunk, ring, available - first_chunk);
      }
      memset(dst + available, 0, bytes_needed - available);
      u32 new_rp = (rp + available) % capacity;
      atomic_store_explicit(&out->ring_read_pos, new_rp, memory_order_release);
    } else {
      memset(dst, 0, bytes_needed);
    }
    atomic_fetch_add_explicit(&out->callback_underruns, 1,
                              memory_order_relaxed);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// INITIALIZE AUDIO
// ═══════════════════════════════════════════════════════════════════════════

bool raylib_init_audio(GameAudioOutputBuffer *audio_output,
                       i32 samples_per_second, i32 game_update_hz) {

  printf("═══════════════════════════════════════════════════════════\n");
  printf("RAYLIB AUDIO INITIALIZATION (ring buffer + callback)\n");
  printf("═══════════════════════════════════════════════════════════\n");

  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "Audio: Failed to initialize audio device\n");
    if (audio_output)
      audio_output->is_initialized = false;
    return false;
  }

  printf("[AUDIO] Device initialized\n");

  g_raylib_audio_output.samples_per_second = samples_per_second;
  g_raylib_audio_output.master_volume = 1.0f;

  // ─── Latency targeting ───
  i32 samples_per_frame = samples_per_second / game_update_hz;
  g_raylib_audio_output.latency_frames =
      (u32)(samples_per_frame * FRAMES_OF_AUDIO_LATENCY);
  g_raylib_audio_output.safety_frames = (u32)(samples_per_frame / 3);

  u32 target_buffered =
      g_raylib_audio_output.latency_frames +
      g_raylib_audio_output.safety_frames;

  printf("[AUDIO] Samples per frame: %d (at %d Hz game logic)\n",
         samples_per_frame, game_update_hz);
  printf("[AUDIO] Latency target: %u frames (%.1f ms)\n", target_buffered,
         (float)target_buffered / samples_per_second * 1000.0f);

  // ─── Format ───
  // The Raylib backend's format — F32 matches miniaudio's internal format
  // (no double conversion). All buffer sizing derives from this.
  const AudioSampleFormat raylib_format = AUDIO_FORMAT_F32;
  const u32 bytes_per_frame =
      (u32)audio_format_bytes_per_sample(raylib_format);
  g_raylib_audio_output.bytes_per_frame = bytes_per_frame;

  // ─── Ring buffer ───
  u32 ring_frames = target_buffered * 4;
  u32 pow2 = 1;
  while (pow2 < ring_frames)
    pow2 <<= 1;
  ring_frames = pow2;

  u32 ring_bytes = ring_frames * bytes_per_frame;
  g_raylib_audio_output.ring_buffer_frames = ring_frames;
  g_raylib_audio_output.ring_buffer_size_bytes = ring_bytes;

  g_raylib_audio_output.ring_buffer =
      de100_memory_alloc(NULL, ring_bytes,
                         De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE |
                             De100_MEMORY_FLAG_ZEROED);

  if (!de100_memory_is_valid(g_raylib_audio_output.ring_buffer)) {
    fprintf(stderr, "Audio: Failed to allocate ring buffer\n");
    CloseAudioDevice();
    if (audio_output)
      audio_output->is_initialized = false;
    return false;
  }

  atomic_store(&g_raylib_audio_output.ring_write_pos, 0);
  atomic_store(&g_raylib_audio_output.ring_read_pos, 0);
  atomic_store(&g_raylib_audio_output.callback_underruns, 0);
  atomic_store(&g_raylib_audio_output.callback_calls, 0);
  atomic_store(&g_raylib_audio_output.callback_total_frames, 0);

  printf("[AUDIO] Ring buffer: %u frames (%u bytes, %.1f ms capacity)\n",
         ring_frames, ring_bytes,
         (float)ring_frames / samples_per_second * 1000.0f);

  // ─── Create audio stream ───
  // Note: SetAudioStreamBufferSizeDefault is NOT called — Raylib/miniaudio
  // picks its own callback chunk size based on the audio backend. We don't
  // control it and don't need to.
  // Bits per sample: F32=32, I16=16, I32=32, F64=64
  i32 bits = (i32)(bytes_per_frame / 2) * 8; // bytes_per_frame / channels * 8
  g_raylib_audio_output.stream =
      LoadAudioStream(samples_per_second, bits, 2);

  if (!IsAudioStreamValid(g_raylib_audio_output.stream)) {
    fprintf(stderr, "Audio: Failed to create stream\n");
    de100_memory_free(&g_raylib_audio_output.ring_buffer);
    CloseAudioDevice();
    if (audio_output)
      audio_output->is_initialized = false;
    return false;
  }

  g_raylib_audio_output.stream_valid = true;
  printf("[AUDIO] Stream created (%d Hz, %d-bit stereo, format=%d)\n",
         samples_per_second, bits, (int)raylib_format);

  // ─── Attach callback ───
  SetAudioStreamCallback(g_raylib_audio_output.stream,
                         raylib_audio_callback);
  printf("[AUDIO] Audio callback attached\n");

  // ─── Pre-fill ring with silence ───
  u32 prefill_bytes = target_buffered * bytes_per_frame;
  atomic_store(&g_raylib_audio_output.ring_write_pos, prefill_bytes);
  printf("[AUDIO] Pre-filled %u frames of silence (%.1f ms)\n",
         target_buffered,
         (float)target_buffered / samples_per_second * 1000.0f);

  // ─── Start playback ───
  PlayAudioStream(g_raylib_audio_output.stream);
  g_raylib_audio_output.stream_playing = true;

  if (audio_output) {
    audio_output->samples_per_second = samples_per_second;
    audio_output->format = raylib_format;
    audio_output->is_initialized = true;
  }

  printf("[AUDIO] READY: %d Hz, ring=%u frames, target=%.1f ms\n",
         samples_per_second, ring_frames,
         (float)target_buffered / samples_per_second * 1000.0f);
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// QUERY SAMPLES TO WRITE
// ═══════════════════════════════════════════════════════════════════════════

u32 raylib_get_samples_to_write(GameAudioOutputBuffer *audio_output) {
  if (!audio_output->is_initialized || !g_raylib_audio_output.stream_valid)
    return 0;

  u32 bpf = g_raylib_audio_output.bytes_per_frame;
  u32 capacity = g_raylib_audio_output.ring_buffer_size_bytes;

  u32 wp = atomic_load_explicit(&g_raylib_audio_output.ring_write_pos,
                                memory_order_relaxed);
  u32 rp = atomic_load_explicit(&g_raylib_audio_output.ring_read_pos,
                                memory_order_acquire);

  u32 buffered_frames = ring_available_to_read(wp, rp, capacity) / bpf;
  u32 target_frames = g_raylib_audio_output.latency_frames +
                      g_raylib_audio_output.safety_frames;

  if (buffered_frames >= target_frames)
    return 0;

  u32 frames_to_write = target_frames - buffered_frames;

  u32 writable_frames = ring_available_to_write(wp, rp, capacity) / bpf;
  if (frames_to_write > writable_frames)
    frames_to_write = writable_frames;

  return frames_to_write;
}

// ═══════════════════════════════════════════════════════════════════════════
// SEND SAMPLES TO RING BUFFER
// ═══════════════════════════════════════════════════════════════════════════

void raylib_send_samples(GameAudioOutputBuffer *audio_output) {
  if (!audio_output->is_initialized || !g_raylib_audio_output.stream_valid)
    return;
  if (audio_output->sample_count <= 0)
    return;

  u32 bpf = g_raylib_audio_output.bytes_per_frame;
  u32 bytes_to_write = (u32)audio_output->sample_count * bpf;
  u32 capacity = g_raylib_audio_output.ring_buffer_size_bytes;

  u32 wp = atomic_load_explicit(&g_raylib_audio_output.ring_write_pos,
                                memory_order_relaxed);

  u8 *ring = (u8 *)g_raylib_audio_output.ring_buffer.base;
  u8 *src = (u8 *)audio_output->samples_buffer;

  u32 first_chunk = capacity - wp;
  if (first_chunk >= bytes_to_write) {
    memcpy(ring + wp, src, bytes_to_write);
  } else {
    memcpy(ring + wp, src, first_chunk);
    memcpy(ring, src + first_chunk, bytes_to_write - first_chunk);
  }

  u32 new_wp = (wp + bytes_to_write) % capacity;
  atomic_store_explicit(&g_raylib_audio_output.ring_write_pos, new_wp,
                        memory_order_release);

  g_raylib_audio_output.total_samples_written += audio_output->sample_count;
}

// ═══════════════════════════════════════════════════════════════════════════
// CLEAR AUDIO BUFFER
// ═══════════════════════════════════════════════════════════════════════════

void raylib_clear_audio_buffer(GameAudioOutputBuffer *audio_output) {
  if (!audio_output->is_initialized || !g_raylib_audio_output.stream_valid)
    return;

  u32 rp = atomic_load_explicit(&g_raylib_audio_output.ring_read_pos,
                                memory_order_acquire);
  atomic_store_explicit(&g_raylib_audio_output.ring_write_pos, rp,
                        memory_order_release);
}

// ═══════════════════════════════════════════════════════════════════════════
// FPS CHANGE HANDLING
// ═══════════════════════════════════════════════════════════════════════════

void raylib_audio_fps_change_handling(GameAudioOutputBuffer *audio_output,
                                      i32 new_game_update_hz) {
  (void)audio_output;
  if (!g_raylib_audio_output.stream_valid || new_game_update_hz <= 0)
    return;

  i32 samples_per_frame =
      g_raylib_audio_output.samples_per_second / new_game_update_hz;

  g_raylib_audio_output.latency_frames =
      (u32)(samples_per_frame * FRAMES_OF_AUDIO_LATENCY);
  g_raylib_audio_output.safety_frames = (u32)(samples_per_frame / 3);

  printf("[AUDIO] FPS changed to %d Hz: latency=%u safety=%u target=%u\n",
         new_game_update_hz,
         g_raylib_audio_output.latency_frames,
         g_raylib_audio_output.safety_frames,
         g_raylib_audio_output.latency_frames +
             g_raylib_audio_output.safety_frames);
}

// ═══════════════════════════════════════════════════════════════════════════
// PAUSE / RESUME
// ═══════════════════════════════════════════════════════════════════════════

void raylib_pause_audio(void) {
  if (!g_raylib_audio_output.stream_valid || !g_raylib_audio_output.stream_playing)
    return;

  PauseAudioStream(g_raylib_audio_output.stream);
  g_raylib_audio_output.stream_playing = false;
}

void raylib_resume_audio(void) {
  if (!g_raylib_audio_output.stream_valid || g_raylib_audio_output.stream_playing)
    return;

  ResumeAudioStream(g_raylib_audio_output.stream);
  g_raylib_audio_output.stream_playing = true;
}

// ═══════════════════════════════════════════════════════════════════════════
// STATS RESET
// ═══════════════════════════════════════════════════════════════════════════

void raylib_audio_reset_stats(void) {
  atomic_store(&g_raylib_audio_output.callback_underruns, 0);
  atomic_store(&g_raylib_audio_output.callback_calls, 0);
  atomic_store(&g_raylib_audio_output.callback_total_frames, 0);
  g_raylib_audio_output.total_samples_written = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// MASTER VOLUME
// ═══════════════════════════════════════════════════════════════════════════

void raylib_set_master_volume(f32 volume) {
  if (volume < 0.0f) volume = 0.0f;
  if (volume > 1.0f) volume = 1.0f;
  g_raylib_audio_output.master_volume = volume;
  SetMasterVolume(volume);
}

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG (terminal only)
// ═══════════════════════════════════════════════════════════════════════════

void raylib_debug_audio_latency(GameAudioOutputBuffer *audio_output) {
  if (!audio_output->is_initialized) {
    printf("Audio: Not initialized\n");
    return;
  }

  u32 bpf = g_raylib_audio_output.bytes_per_frame;
  u32 capacity = g_raylib_audio_output.ring_buffer_size_bytes;
  u32 wp = atomic_load(&g_raylib_audio_output.ring_write_pos);
  u32 rp = atomic_load(&g_raylib_audio_output.ring_read_pos);
  u32 buffered = ring_available_to_read(wp, rp, capacity) / bpf;
  i32 underruns = atomic_load(&g_raylib_audio_output.callback_underruns);
  i32 cb_calls = atomic_load(&g_raylib_audio_output.callback_calls);
  u32 cb_chunk = atomic_load(&g_raylib_audio_output.callback_last_frames);

  u32 target = g_raylib_audio_output.latency_frames +
               g_raylib_audio_output.safety_frames;

  printf("[AUDIO] buffered=%u/%u target=%u underruns=%d "
         "cb_calls=%d cb_chunk=%u vol=%.1f\n",
         buffered, g_raylib_audio_output.ring_buffer_frames,
         target, underruns, cb_calls, cb_chunk,
         g_raylib_audio_output.master_volume);
}

// ═══════════════════════════════════════════════════════════════════════════
// SHUTDOWN
// ═══════════════════════════════════════════════════════════════════════════

void raylib_shutdown_audio(GameAudioOutputBuffer *audio_output) {
  if (!audio_output || !audio_output->is_initialized)
    return;

  printf("Shutting down audio...\n");

  if (g_raylib_audio_output.stream_valid) {
    StopAudioStream(g_raylib_audio_output.stream);
    UnloadAudioStream(g_raylib_audio_output.stream);
    g_raylib_audio_output.stream_valid = false;
  }

  if (de100_memory_is_valid(g_raylib_audio_output.ring_buffer)) {
    de100_memory_free(&g_raylib_audio_output.ring_buffer);
  }

  CloseAudioDevice();

  audio_output->is_initialized = false;
  audio_output->sample_count = 0;

  printf("[AUDIO] Shutdown complete\n");
}
