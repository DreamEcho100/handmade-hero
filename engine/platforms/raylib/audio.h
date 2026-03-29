#ifndef DE100_PLATFORMS_RAYLIB_AUDIO_H
#define DE100_PLATFORMS_RAYLIB_AUDIO_H

#include "../../_common/base.h"
#include "../../_common/memory.h"
#include "../../game/audio.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// RAYLIB SOUND OUTPUT STATE
//
// Uses a lock-free SPSC ring buffer between the main thread (producer)
// and Raylib/miniaudio's audio callback thread (consumer).
//
// Latency = how far ahead we write into the ring, NOT the ring size.
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  AudioStream stream;
  bool stream_valid;
  bool stream_playing;

  // Latency targeting
  u32 latency_frames;  // samples_per_frame * FRAMES_OF_AUDIO_LATENCY
  u32 safety_frames;   // samples_per_frame / 3 (may be 0, see A2 test)
  i32 samples_per_second;

  // Lock-free SPSC ring buffer (main thread writes, audio callback reads)
  De100MemoryBlock ring_buffer;
  u32 ring_buffer_size_bytes;
  u32 ring_buffer_frames;
  _Atomic u32 ring_write_pos;
  _Atomic u32 ring_read_pos;

  // Bytes per stereo frame (e.g., 4 for 16-bit stereo)
  u32 bytes_per_frame;

  // Platform-level master volume
  f32 master_volume; // 0.0 to 1.0, default 1.0

  // Statistics
  i64 total_samples_written;
  _Atomic i32 callback_underruns;
  _Atomic i32 callback_calls;
  _Atomic u32 callback_last_frames;
  _Atomic u32 callback_total_frames;

} RaylibSoundOutput;

extern RaylibSoundOutput g_raylib_audio_output;

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

bool raylib_init_audio(GameAudioOutputBuffer *audio_output,
                       i32 samples_per_second, i32 game_update_hz);
void raylib_shutdown_audio(GameAudioOutputBuffer *audio_output);

/** Query how many samples to generate this frame (ring buffer fill level). */
u32 raylib_get_samples_to_write(GameAudioOutputBuffer *audio_output);

/** Copy game-generated samples into the ring buffer. */
void raylib_send_samples(GameAudioOutputBuffer *audio_output);

/** Flush ring buffer (silence). */
void raylib_clear_audio_buffer(GameAudioOutputBuffer *audio_output);

/** Print audio debug info to terminal. */
void raylib_debug_audio_latency(GameAudioOutputBuffer *audio_output);

/** Recalculate latency targets when game_update_hz changes. */
void raylib_audio_fps_change_handling(GameAudioOutputBuffer *audio_output,
                                      i32 new_game_update_hz);

/** Pause audio stream (e.g., on focus loss). */
void raylib_pause_audio(void);

/** Resume audio stream (e.g., on focus gain). */
void raylib_resume_audio(void);

/** Reset all audio statistics counters. */
void raylib_audio_reset_stats(void);

/** Set platform-level master volume (0.0 to 1.0). */
void raylib_set_master_volume(f32 volume);

// ═══════════════════════════════════════════════════════════════════════════
// TODO: FUTURE FEATURES
// ═══════════════════════════════════════════════════════════════════════════
//
// Multi-stream support:
//   Raylib supports multiple AudioStream instances via LoadAudioStream().
//   For games needing separate music + SFX routing with independent volume,
//   create a second stream + ring buffer pair. Each stream gets its own
//   callback and ring. The game mixes into separate buffers, platform routes
//   them to separate Raylib streams.
//
// Audio file loading:
//   Raylib has LoadSound() / LoadMusicStream() for WAV/OGG/MP3 playback.
//   Integrate with the platform audio layer for games that need sample
//   playback beyond synthesis. Would need a sound asset registry and
//   a way to reference loaded sounds from game code.
//
// ═══════════════════════════════════════════════════════════════════════════

#endif // DE100_PLATFORMS_RAYLIB_AUDIO_H
