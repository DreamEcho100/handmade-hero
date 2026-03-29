#ifndef DE100_GAME_AUDIO_MIXER_H
#define DE100_GAME_AUDIO_MIXER_H

#include "../_common/base.h"
#include "audio.h"
#include "audio-helpers.h"
#include "audio-loader.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO MIXER
//
// Playing sound pool + mixer inspired by Casey's Handmade Hero audio system.
// Manages multiple simultaneous sounds with volume, pitch, and chaining.
//
// Usage:
//   // Init (once, from game memory arena):
//   De100AudioMixer mixer = {0};
//   de100_mixer_init(&mixer, 32); // 32-sound pool
//
//   // Play a sound:
//   De100PlayingSound *snd = de100_mixer_play(&mixer, &loaded_sound);
//
//   // Control:
//   de100_mixer_change_volume(&mixer, snd, 2.0f, 0.0f, 0.0f); // fade out 2s
//   de100_mixer_change_pitch(&mixer, snd, 1.5f); // 1.5x speed
//
//   // Each frame in game_get_audio_samples:
//   de100_mixer_output(&mixer, audio_buffer);
//
// ═══════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────
// Sound chaining
// ─────────────────────────────────────────────────────────────────────────

typedef enum {
  DE100_SOUND_CHAIN_NONE = 0, // Sound ends
  DE100_SOUND_CHAIN_LOOP,     // Restart same sound
  DE100_SOUND_CHAIN_ADVANCE,  // Play next sound (caller sets next_sound)
} De100SoundChain;

// ─────────────────────────────────────────────────────────────────────────
// Playing sound (one active voice)
// ─────────────────────────────────────────────────────────────────────────

typedef struct De100PlayingSound {
  De100LoadedSound *sound; // Source PCM data (NULL = inactive)

  f32 samples_played;       // Current playback position (fractional)
  f32 d_sample;             // Playback speed (1.0 = normal)

  f32 volume_left;          // Current left volume
  f32 volume_right;         // Current right volume
  f32 d_volume_left;        // Per-sample volume delta (for fading)
  f32 d_volume_right;
  f32 target_volume_left;   // Fade target
  f32 target_volume_right;

  De100SoundChain chain;    // What happens when sound ends
  De100LoadedSound *next_sound; // For ADVANCE chain

  struct De100PlayingSound *next; // Linked list
} De100PlayingSound;

// ─────────────────────────────────────────────────────────────────────────
// Mixer state
// ─────────────────────────────────────────────────────────────────────────

#ifndef DE100_MIXER_MAX_SOUNDS
#define DE100_MIXER_MAX_SOUNDS 32
#endif

typedef struct {
  De100PlayingSound pool[DE100_MIXER_MAX_SOUNDS]; // Static pool
  De100PlayingSound *first_playing;  // Active sounds linked list
  De100PlayingSound *first_free;     // Free pool linked list

  f32 master_volume_left;
  f32 master_volume_right;
} De100AudioMixer;

// ─────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────

static inline void de100_mixer_init(De100AudioMixer *mixer) {
  memset(mixer, 0, sizeof(*mixer));
  mixer->master_volume_left = 1.0f;
  mixer->master_volume_right = 1.0f;

  // Build free list from pool
  mixer->first_free = &mixer->pool[0];
  for (i32 i = 0; i < DE100_MIXER_MAX_SOUNDS - 1; i++) {
    mixer->pool[i].next = &mixer->pool[i + 1];
  }
  mixer->pool[DE100_MIXER_MAX_SOUNDS - 1].next = NULL;
  mixer->first_playing = NULL;

  printf("[MIXER] Initialized with %d voice pool\n", DE100_MIXER_MAX_SOUNDS);
}

// ─────────────────────────────────────────────────────────────────────────
// Play a sound
// ─────────────────────────────────────────────────────────────────────────

static inline De100PlayingSound *
de100_mixer_play(De100AudioMixer *mixer, De100LoadedSound *sound) {
  if (!sound || !sound->is_valid) return NULL;

  // Grab from free pool
  De100PlayingSound *playing = mixer->first_free;
  if (!playing) {
    printf("[MIXER] No free voices!\n");
    return NULL;
  }
  mixer->first_free = playing->next;

  // Initialize
  playing->sound = sound;
  playing->samples_played = 0.0f;
  playing->d_sample = 1.0f;
  playing->volume_left = 1.0f;
  playing->volume_right = 1.0f;
  playing->d_volume_left = 0.0f;
  playing->d_volume_right = 0.0f;
  playing->target_volume_left = 1.0f;
  playing->target_volume_right = 1.0f;
  playing->chain = DE100_SOUND_CHAIN_NONE;
  playing->next_sound = NULL;

  // Insert at head of playing list
  playing->next = mixer->first_playing;
  mixer->first_playing = playing;

  return playing;
}

// ─────────────────────────────────────────────────────────────────────────
// Volume fading (per-channel, time-based)
// ─────────────────────────────────────────────────────────────────────────

static inline void
de100_mixer_change_volume(De100AudioMixer *mixer, De100PlayingSound *sound,
                          f32 fade_seconds, f32 target_left,
                          f32 target_right) {
  (void)mixer;
  if (!sound || !sound->sound) return;

  sound->target_volume_left = target_left;
  sound->target_volume_right = target_right;

  if (fade_seconds <= 0.0f) {
    sound->volume_left = target_left;
    sound->volume_right = target_right;
    sound->d_volume_left = 0.0f;
    sound->d_volume_right = 0.0f;
  } else {
    f32 inv_samples = 1.0f / (fade_seconds * (f32)sound->sound->sample_rate);
    sound->d_volume_left = (target_left - sound->volume_left) * inv_samples;
    sound->d_volume_right = (target_right - sound->volume_right) * inv_samples;
  }
}

// ─────────────────────────────────────────────────────────────────────────
// Pitch control
// ─────────────────────────────────────────────────────────────────────────

static inline void
de100_mixer_change_pitch(De100AudioMixer *mixer, De100PlayingSound *sound,
                         f32 d_sample) {
  (void)mixer;
  if (!sound) return;
  sound->d_sample = d_sample;
}

// ─────────────────────────────────────────────────────────────────────────
// Stop a sound (returns to free pool next mixer output)
// ─────────────────────────────────────────────────────────────────────────

static inline void
de100_mixer_stop(De100AudioMixer *mixer, De100PlayingSound *sound) {
  (void)mixer;
  if (sound) sound->sound = NULL; // Marked for removal
}

// ─────────────────────────────────────────────────────────────────────────
// Mixer output — mix all playing sounds into the audio buffer
//
// This is the main mixing function, called from game_get_audio_samples.
// Handles: sample reading, linear interpolation, volume fading,
// pitch control, sound chaining, and automatic cleanup.
// ─────────────────────────────────────────────────────────────────────────

// Max samples per mixer call (stack-allocated float mix buffers).
// 4800 matches the engine's max_sample_count (samples_per_frame * 3).
#ifndef DE100_MIXER_MAX_SAMPLES
#define DE100_MIXER_MAX_SAMPLES 4800
#endif

// The mixer hot path MUST be optimized even in debug builds — without
// optimization, per-sample float math is ~10x slower and causes underruns.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O2")
#endif
__attribute__((unused)) static void
de100_mixer_output(De100AudioMixer *mixer, GameAudioOutputBuffer *buf) {
  if (!buf || !buf->is_initialized || buf->sample_count <= 0)
    return;

  i32 sample_count = buf->sample_count;
  if (sample_count > DE100_MIXER_MAX_SAMPLES)
    sample_count = DE100_MIXER_MAX_SAMPLES;

  // ─── Step 1: Read existing buffer into float mix channels ───
  // Preserves whatever the game's synthesis loop already wrote.
  // Format is checked ONCE here, then direct pointer access for speed.
  f32 mix_left[DE100_MIXER_MAX_SAMPLES];
  f32 mix_right[DE100_MIXER_MAX_SAMPLES];

  switch (buf->format) {
  case AUDIO_FORMAT_I16: {
    i16 *src = (i16 *)buf->samples_buffer;
    for (i32 s = 0; s < sample_count; s++) {
      mix_left[s] = (f32)src[s * 2] / 32767.0f;
      mix_right[s] = (f32)src[s * 2 + 1] / 32767.0f;
    }
  } break;
  case AUDIO_FORMAT_I32: {
    i32 *src = (i32 *)buf->samples_buffer;
    for (i32 s = 0; s < sample_count; s++) {
      mix_left[s] = (f32)((f64)src[s * 2] / 2147483647.0);
      mix_right[s] = (f32)((f64)src[s * 2 + 1] / 2147483647.0);
    }
  } break;
  case AUDIO_FORMAT_F32: {
    f32 *src = (f32 *)buf->samples_buffer;
    for (i32 s = 0; s < sample_count; s++) {
      mix_left[s] = src[s * 2];
      mix_right[s] = src[s * 2 + 1];
    }
  } break;
  case AUDIO_FORMAT_F64: {
    f64 *src = (f64 *)buf->samples_buffer;
    for (i32 s = 0; s < sample_count; s++) {
      mix_left[s] = (f32)src[s * 2];
      mix_right[s] = (f32)src[s * 2 + 1];
    }
  } break;
  }

  // ─── Step 2: Mix all playing sounds into float buffers ───
  De100PlayingSound **playing_ptr = &mixer->first_playing;

  while (*playing_ptr) {
    De100PlayingSound *playing = *playing_ptr;

    if (!playing->sound || !playing->sound->is_valid) {
      *playing_ptr = playing->next;
      playing->next = mixer->first_free;
      mixer->first_free = playing;
      playing->sound = NULL;
      continue;
    }

    De100LoadedSound *sound = playing->sound;
    i16 *samples = sound->samples;
    u32 total_frames = sound->sample_count;
    u32 channels = sound->channel_count;
    bool sound_finished = false;

    for (i32 s = 0; s < sample_count; s++) {
      // ─── Volume ramping ───
      if (playing->d_volume_left != 0.0f) {
        playing->volume_left += playing->d_volume_left;
        if ((playing->d_volume_left > 0.0f &&
             playing->volume_left >= playing->target_volume_left) ||
            (playing->d_volume_left < 0.0f &&
             playing->volume_left <= playing->target_volume_left)) {
          playing->volume_left = playing->target_volume_left;
          playing->d_volume_left = 0.0f;
        }
      }
      if (playing->d_volume_right != 0.0f) {
        playing->volume_right += playing->d_volume_right;
        if ((playing->d_volume_right > 0.0f &&
             playing->volume_right >= playing->target_volume_right) ||
            (playing->d_volume_right < 0.0f &&
             playing->volume_right <= playing->target_volume_right)) {
          playing->volume_right = playing->target_volume_right;
          playing->d_volume_right = 0.0f;
        }
      }

      // ─── Sample position ───
      f32 pos = playing->samples_played;
      u32 sample_index = (u32)pos;

      if (sample_index >= total_frames) {
        if (playing->chain == DE100_SOUND_CHAIN_LOOP) {
          playing->samples_played -= (f32)total_frames;
          pos = playing->samples_played;
          sample_index = (u32)pos;
        } else if (playing->chain == DE100_SOUND_CHAIN_ADVANCE &&
                   playing->next_sound && playing->next_sound->is_valid) {
          playing->sound = playing->next_sound;
          playing->next_sound = NULL;
          playing->samples_played = 0.0f;
          sound = playing->sound;
          samples = sound->samples;
          total_frames = sound->sample_count;
          channels = sound->channel_count;
          pos = 0.0f;
          sample_index = 0;
        } else {
          sound_finished = true;
          break;
        }
      }

      // ─── Read + interpolate ───
      f32 frac = pos - (f32)sample_index;
      u32 next_index = sample_index + 1;
      if (next_index >= total_frames) {
        next_index = (playing->chain == DE100_SOUND_CHAIN_LOOP) ? 0
                                                                 : sample_index;
      }

      f32 left, right;
      if (channels >= 2) {
        f32 l0 = (f32)samples[sample_index * 2] / 32767.0f;
        f32 r0 = (f32)samples[sample_index * 2 + 1] / 32767.0f;
        f32 l1 = (f32)samples[next_index * 2] / 32767.0f;
        f32 r1 = (f32)samples[next_index * 2 + 1] / 32767.0f;
        left = l0 + frac * (l1 - l0);
        right = r0 + frac * (r1 - r0);
      } else {
        f32 s0 = (f32)samples[sample_index] / 32767.0f;
        f32 s1 = (f32)samples[next_index] / 32767.0f;
        left = s0 + frac * (s1 - s0);
        right = left;
      }

      // ─── Accumulate into float mix buffers (no clamping yet!) ───
      left *= playing->volume_left * mixer->master_volume_left;
      right *= playing->volume_right * mixer->master_volume_right;
      mix_left[s] += left;
      mix_right[s] += right;

      playing->samples_played += playing->d_sample;
    }

    if (sound_finished) {
      *playing_ptr = playing->next;
      playing->next = mixer->first_free;
      mixer->first_free = playing;
      playing->sound = NULL;
    } else {
      playing_ptr = &playing->next;
    }
  }

  // ─── Step 3: Write float mix back to output buffer (clamp + convert) ───
  // Format checked ONCE, direct pointer write for speed.
  switch (buf->format) {
  case AUDIO_FORMAT_I16: {
    i16 *out = (i16 *)buf->samples_buffer;
    for (i32 s = 0; s < sample_count; s++) {
      f32 l = mix_left[s], r = mix_right[s];
      if (l > 1.0f) l = 1.0f; if (l < -1.0f) l = -1.0f;
      if (r > 1.0f) r = 1.0f; if (r < -1.0f) r = -1.0f;
      out[s * 2] = (i16)(l * 32767.0f);
      out[s * 2 + 1] = (i16)(r * 32767.0f);
    }
  } break;
  case AUDIO_FORMAT_I32: {
    i32 *out = (i32 *)buf->samples_buffer;
    for (i32 s = 0; s < sample_count; s++) {
      f32 l = mix_left[s], r = mix_right[s];
      if (l > 1.0f) l = 1.0f; if (l < -1.0f) l = -1.0f;
      if (r > 1.0f) r = 1.0f; if (r < -1.0f) r = -1.0f;
      out[s * 2] = (i32)(l * 2147483647.0f);
      out[s * 2 + 1] = (i32)(r * 2147483647.0f);
    }
  } break;
  case AUDIO_FORMAT_F32: {
    f32 *out = (f32 *)buf->samples_buffer;
    for (i32 s = 0; s < sample_count; s++) {
      f32 l = mix_left[s], r = mix_right[s];
      if (l > 1.0f) l = 1.0f; if (l < -1.0f) l = -1.0f;
      if (r > 1.0f) r = 1.0f; if (r < -1.0f) r = -1.0f;
      out[s * 2] = l;
      out[s * 2 + 1] = r;
    }
  } break;
  case AUDIO_FORMAT_F64: {
    f64 *out = (f64 *)buf->samples_buffer;
    for (i32 s = 0; s < sample_count; s++) {
      f32 l = mix_left[s], r = mix_right[s];
      if (l > 1.0f) l = 1.0f; if (l < -1.0f) l = -1.0f;
      if (r > 1.0f) r = 1.0f; if (r < -1.0f) r = -1.0f;
      out[s * 2] = (f64)l;
      out[s * 2 + 1] = (f64)r;
    }
  } break;
  }
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif

#endif // DE100_GAME_AUDIO_MIXER_H
