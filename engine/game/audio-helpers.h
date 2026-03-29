#ifndef DE100_GAME_AUDIO_HELPERS_H
#define DE100_GAME_AUDIO_HELPERS_H

#include "../_common/base.h"
#include "audio.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════
//
// These are OPTIONAL utilities that games can use.
// All are static inline for zero overhead if unused.
//
// ═══════════════════════════════════════════════════════════════════════════

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MIDI to Frequency Conversion
// ─────────────────────────────────────────────────────────────────────────────
// Standard MIDI note to Hz: A4 (note 69) = 440 Hz
//
// Usage:
//   f32 freq = de100_audio_midi_to_freq(60);  // Middle C = 261.63 Hz
//
de100_file_scoped_fn inline f32 de100_audio_midi_to_freq(i32 midi_note) {
  return 440.0f * powf(2.0f, (f32)(midi_note - 69) / 12.0f);
}

/* Common MIDI note constants */
#define DE100_MIDI_C4 60
#define DE100_MIDI_D4 62
#define DE100_MIDI_E4 64
#define DE100_MIDI_F4 65
#define DE100_MIDI_G4 67
#define DE100_MIDI_A4 69
#define DE100_MIDI_B4 71
#define DE100_MIDI_C5 72
#define DE100_MIDI_D5 74
#define DE100_MIDI_E5 76
#define DE100_MIDI_G5 79
#define DE100_MIDI_A5 81
#define DE100_MIDI_REST 0

// ─────────────────────────────────────────────────────────────────────────────
// audio_write_sample — write one stereo frame into a GameAudioOutputBuffer
// ─────────────────────────────────────────────────────────────────────────────
//
// All game mixing must happen in float (-1.0 to 1.0).  The format conversion
// to whatever PCM the backend requested lives here, at the platform boundary.
//
// Usage:
//   audio_write_sample(buf, s, left_f32, right_f32);
//   audio_write_sample(buf, s, left_f64, right_f64);   // f64 overload
//
// frame_index : zero-based sample frame index (0 .. sample_count-1)
// left/right  : normalised float amplitude  (-1.0 to  1.0)
//
// ─────────────────────────────────────────────────────────────────────────────

de100_file_scoped_fn inline void
_audio_write_sample_f32(GameAudioOutputBuffer *buf, i32 frame_index, f32 left,
                        f32 right) {
  /* Clamp to normalised range */
  left = left < -1.0f ? -1.0f : (left > 1.0f ? 1.0f : left);
  right = right < -1.0f ? -1.0f : (right > 1.0f ? 1.0f : right);
  i32 base = frame_index * 2;
  switch (buf->format) {
  case AUDIO_FORMAT_I16: {
    i16 *s = (i16 *)buf->samples_buffer;
    s[base] = (i16)(left * 32767.0f);
    s[base + 1] = (i16)(right * 32767.0f);
  } break;
  case AUDIO_FORMAT_I32: {
    i32 *s = (i32 *)buf->samples_buffer;
    s[base] = (i32)(left * 2147483647.0f);
    s[base + 1] = (i32)(right * 2147483647.0f);
  } break;
  case AUDIO_FORMAT_F32: {
    f32 *s = (f32 *)buf->samples_buffer;
    s[base] = left;
    s[base + 1] = right;
  } break;
  case AUDIO_FORMAT_F64: {
    f64 *s = (f64 *)buf->samples_buffer;
    s[base] = (f64)left;
    s[base + 1] = (f64)right;
  } break;
  }
}

de100_file_scoped_fn inline void
_audio_write_sample_f64(GameAudioOutputBuffer *buf, i32 frame_index, f64 left,
                        f64 right) {
  left = left < -1.0 ? -1.0 : (left > 1.0 ? 1.0 : left);
  right = right < -1.0 ? -1.0 : (right > 1.0 ? 1.0 : right);
  i32 base = frame_index * 2;
  switch (buf->format) {
  case AUDIO_FORMAT_I16: {
    i16 *s = (i16 *)buf->samples_buffer;
    s[base] = (i16)(left * 32767.0);
    s[base + 1] = (i16)(right * 32767.0);
  } break;
  case AUDIO_FORMAT_I32: {
    i32 *s = (i32 *)buf->samples_buffer;
    s[base] = (i32)(left * 2147483647.0);
    s[base + 1] = (i32)(right * 2147483647.0);
  } break;
  case AUDIO_FORMAT_F32: {
    f32 *s = (f32 *)buf->samples_buffer;
    s[base] = (f32)left;
    s[base + 1] = (f32)right;
  } break;
  case AUDIO_FORMAT_F64: {
    f64 *s = (f64 *)buf->samples_buffer;
    s[base] = left;
    s[base + 1] = right;
  } break;
  }
}

/* Dispatch on the type of the L argument (R must match).
 * f32 → _audio_write_sample_f32 ; f64/double → _audio_write_sample_f64 */
#define audio_write_sample(buf, frame_index, L, R)                             \
  _Generic((L), f32: _audio_write_sample_f32, f64: _audio_write_sample_f64)(   \
      (buf), (frame_index), (L), (R))

// ─────────────────────────────────────────────────────────────────────────────
// audio_read_sample — read one stereo frame from a GameAudioOutputBuffer
// ─────────────────────────────────────────────────────────────────────────────
//
// Mirrors audio_write_sample() but in reverse: reads a frame from the buffer
// and returns normalized f32 left/right values.
//
// Usage:
//   f32 left, right;
//   audio_read_sample(buf, frame_index, &left, &right);
//
// ─────────────────────────────────────────────────────────────────────────────

de100_file_scoped_fn inline void
audio_read_sample(GameAudioOutputBuffer *buf, i32 frame_index, f32 *out_left,
                  f32 *out_right) {
  i32 base = frame_index * 2;
  switch (buf->format) {
  case AUDIO_FORMAT_I16: {
    i16 *s = (i16 *)buf->samples_buffer;
    *out_left = (f32)s[base] / 32767.0f;
    *out_right = (f32)s[base + 1] / 32767.0f;
  } break;
  case AUDIO_FORMAT_I32: {
    i32 *s = (i32 *)buf->samples_buffer;
    *out_left = (f32)((f64)s[base] / 2147483647.0);
    *out_right = (f32)((f64)s[base + 1] / 2147483647.0);
  } break;
  case AUDIO_FORMAT_F32: {
    f32 *s = (f32 *)buf->samples_buffer;
    *out_left = s[base];
    *out_right = s[base + 1];
  } break;
  case AUDIO_FORMAT_F64: {
    f64 *s = (f64 *)buf->samples_buffer;
    *out_left = (f32)s[base];
    *out_right = (f32)s[base + 1];
  } break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Stereo Panning
// ─────────────────────────────────────────────────────────────────────────────
// Calculate left/right volumes from pan position.
//
// pan: -1.0 (full left) to 1.0 (full right), 0.0 = center
//
// Uses linear panning (simple, works well for game audio).
// For music production, constant-power panning would be better.
//
de100_file_scoped_fn inline void
de100_audio_calculate_pan(f32 pan, f32 *left_vol, f32 *right_vol) {
  // Clamp pan to valid range
  if (pan < -1.0f)
    pan = -1.0f;
  if (pan > 1.0f)
    pan = 1.0f;

  if (pan <= 0.0f) {
    *left_vol = 1.0f;
    *right_vol = 1.0f + pan; // pan is negative, reduces right
  } else {
    *left_vol = 1.0f - pan;
    *right_vol = 1.0f;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Volume Ramping
// ─────────────────────────────────────────────────────────────────────────────
// Smoothly transition current volume toward target volume.
// Call this per-sample for click-free volume changes.
//
// Returns: new current volume (store back into your state)
//
// Usage:
//   tone.current_volume = de100_audio_ramp_volume(
//       tone.current_volume,
//       tone.is_playing ? tone.volume : 0.0f,
//       DE100_AUDIO_VOLUME_RAMP_SPEED
//   );
//
de100_file_scoped_fn inline f32 de100_audio_ramp_volume(f32 current, f32 target,
                                                        f32 speed) {
  if (current < target) {
    current += speed;
    if (current > target)
      current = target;
  } else if (current > target) {
    current -= speed;
    if (current < 0.0f)
      current = 0.0f;
  }
  return current;
}

#define DE100_AUDIO_VOLUME_RAMP_SPEED 0.002f

// ─────────────────────────────────────────────────────────────────────────────
// Timed Volume Ramping
// ─────────────────────────────────────────────────────────────────────────────
// Same as de100_audio_ramp_volume but with duration-based speed calculation.
// Call per-sample. Returns the per-sample speed to pass to de100_audio_ramp_volume.
//
// Usage:
//   f32 speed = de100_audio_ramp_speed_for_duration(2.0f, 48000);
//   // Then per-sample:
//   vol = de100_audio_ramp_volume(vol, target, speed);
//
de100_file_scoped_fn inline f32
de100_audio_ramp_speed_for_duration(f32 duration_seconds, i32 sample_rate) {
  if (duration_seconds <= 0.0f || sample_rate <= 0)
    return 1.0f; // instant
  return 1.0f / ((f32)sample_rate * duration_seconds);
}

// ─────────────────────────────────────────────────────────────────────────────
// Waveform Generators
// ─────────────────────────────────────────────────────────────────────────────
// Generate sample values for common waveforms.
// phase: 0.0 to 1.0 (normalized phase)
// Returns: -1.0 to 1.0
//

// Sine wave (smooth, pure tone)
de100_file_scoped_fn inline f32 de100_audio_wave_sine(f32 phase) {
  return sinf(phase * 2.0f * (f32)M_PI);
}

// Square wave (harsh, retro game sound)
de100_file_scoped_fn inline f32 de100_audio_wave_square(f32 phase) {
  return (phase < 0.5f) ? 1.0f : -1.0f;
}

// Triangle wave (softer than square, good for bass)
de100_file_scoped_fn inline f32 de100_audio_wave_triangle(f32 phase) {
  if (phase < 0.25f) {
    return phase * 4.0f;
  } else if (phase < 0.75f) {
    return 2.0f - phase * 4.0f;
  } else {
    return phase * 4.0f - 4.0f;
  }
}

// Sawtooth wave (bright, buzzy)
de100_file_scoped_fn inline f32 de100_audio_wave_sawtooth(f32 phase) {
  return 2.0f * phase - 1.0f;
}

// Pulse wave with variable duty cycle (0.0 to 1.0)
de100_file_scoped_fn inline f32 de100_audio_wave_pulse(f32 phase, f32 duty) {
  return (phase < duty) ? 1.0f : -1.0f;
}

/* ─── Sequencer Step Timing ─────────────────────────────────────────────────
 */

/* Returns true when step should advance, handles timer internally */
de100_file_scoped_fn inline bool
de100_sequencer_should_advance(f32 *step_timer, f32 step_duration,
                               f32 delta_time) {
  *step_timer += delta_time;
  if (*step_timer >= step_duration) {
    *step_timer -= step_duration;
    return true;
  }
  return false;
}

/* Calculate step duration from BPM and steps per beat */
de100_file_scoped_fn inline f32
de100_sequencer_bpm_to_step_duration(f32 bpm, i32 steps_per_beat) {
  /* BPM = beats per minute
   * steps_per_beat = how many sequencer steps per beat (typically 4 for 16th
   * notes) step_duration = seconds per step */
  return 60.0f / (bpm * (f32)steps_per_beat);
}

/* ─── Oscillator State ──────────────────────────────────────────────────────
 */

/* Generic oscillator that games can embed in their own structures */
typedef struct {
  f32 phase;          /* 0.0 to 1.0, wraps */
  f32 frequency;      /* Hz */
  f32 volume;         /* Target volume */
  f32 current_volume; /* Smoothed volume (for click-free) */
} De100Oscillator;

/* Advance oscillator phase, returns current phase before advance */
de100_file_scoped_fn inline f32 de100_oscillator_advance(De100Oscillator *osc,
                                                         f32 inv_sample_rate) {
  f32 current_phase = osc->phase;
  osc->phase += osc->frequency * inv_sample_rate;
  if (osc->phase >= 1.0f)
    osc->phase -= 1.0f;
  return current_phase;
}

/* Update oscillator volume with ramping */
de100_file_scoped_fn inline void
de100_oscillator_update_volume(De100Oscillator *osc, bool is_playing,
                               f32 ramp_speed) {
  f32 target = is_playing ? osc->volume : 0.0f;
  osc->current_volume =
      de100_audio_ramp_volume(osc->current_volume, target, ramp_speed);
}

// ═══════════════════════════════════════════════════════════════════════════
// SOUND INSTANCE (For polyphonic SFX)
// ═══════════════════════════════════════════════════════════════════════════
//
// Represents one currently-playing sound effect.
// Game defines what sound_id values mean (cast from game's enum).
//
// Features:
//   - Pitch slides (frequency_slide)
//   - Fade in/out envelopes
//   - Stereo panning
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  i32 sound_id;          // Game-defined ID (0 = inactive/empty slot)
  f32 phase;             // 0.0 to 1.0 (normalized)
  f32 frequency;         // Current frequency (Hz)
  f32 frequency_slide;   // Hz per sample (for pitch bends)
  f32 volume;            // Base volume (0.0 to 1.0)
  f32 pan_position;      // -1.0 (left) to 1.0 (right)
  i32 samples_remaining; // Countdown to end
  i32 total_samples;     // Original duration
  i32 fade_in_samples;   // Samples to fade in (prevents clicks)
  i32 fade_out_samples;  // Samples to fade out (optional)
} De100SoundInstance;

// Maximum simultaneous sounds (games can define their own limit)
#ifndef DE100_MAX_SOUND_INSTANCES
#define DE100_MAX_SOUND_INSTANCES 8
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Sound Instance Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Check if instance is active
de100_file_scoped_fn inline bool
de100_sound_is_active(De100SoundInstance *inst) {
  return inst->samples_remaining > 0;
}

// Calculate envelope for current sample (fade in/out)
de100_file_scoped_fn inline f32 de100_sound_envelope(De100SoundInstance *inst) {
  f32 env = 1.0f;

  // Fade out at end
  if (inst->fade_out_samples > 0 &&
      inst->samples_remaining <= inst->fade_out_samples) {
    env = (f32)inst->samples_remaining / (f32)inst->fade_out_samples;
  }

  // Fade in at start
  i32 samples_played = inst->total_samples - inst->samples_remaining;
  if (inst->fade_in_samples > 0 && samples_played < inst->fade_in_samples) {
    f32 fade_in = (f32)samples_played / (f32)inst->fade_in_samples;
    env *= fade_in;
  }

  return env;
}

// Advance instance by one sample
de100_file_scoped_fn inline void de100_sound_advance(De100SoundInstance *inst,
                                                     f32 inv_sample_rate) {
  inst->phase += inst->frequency * inv_sample_rate;
  if (inst->phase >= 1.0f)
    inst->phase -= 1.0f;

  inst->frequency += inst->frequency_slide;
  if (inst->frequency < 20.0f)
    inst->frequency = 20.0f; // Clamp to audible

  inst->samples_remaining--;
}

// ═══════════════════════════════════════════════════════════════════════════
// SOUND PLAYER (Manages multiple instances)
// ═══════════════════════════════════════════════════════════════════════════
//
// Simple polyphonic sound manager.
// Games with complex needs can implement their own.
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  De100SoundInstance instances[DE100_MAX_SOUND_INSTANCES];
  f32 volume; // Master volume for all SFX
} De100SoundPlayer;

// Find an empty slot, or steal the oldest sound
de100_file_scoped_fn inline i32
de100_sound_player_find_slot(De100SoundPlayer *player) {
  // First, look for an empty slot
  for (i32 i = 0; i < DE100_MAX_SOUND_INSTANCES; i++) {
    if (!de100_sound_is_active(&player->instances[i])) {
      return i;
    }
  }
  // All slots full - steal slot 0 (could be smarter: steal quietest)
  return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// MUSIC SEQUENCER (Pattern-based music)
// ═══════════════════════════════════════════════════════════════════════════
//
// Simple step sequencer for chiptune-style music.
// Each pattern is a sequence of MIDI note numbers (0 = rest).
//
// Usage:
//   De100MusicSequencer seq = {0};
//   seq.steps_per_pattern = 16;
//   seq.step_duration = 0.15f;  // 150ms per step = 100 BPM
//   // Copy patterns, set pattern_count
//   seq.is_playing = true;
//
// ═══════════════════════════════════════════════════════════════════════════

#ifndef DE100_MAX_PATTERN_LENGTH
#define DE100_MAX_PATTERN_LENGTH 32
#endif

#ifndef DE100_MAX_PATTERNS
#define DE100_MAX_PATTERNS 8
#endif

typedef struct {
  // Pattern data
  u8 patterns[DE100_MAX_PATTERNS][DE100_MAX_PATTERN_LENGTH];
  i32 pattern_count;     // How many patterns are defined
  i32 steps_per_pattern; // How many steps in each pattern

  // Playback state
  i32 current_pattern;
  i32 current_step;
  f32 step_timer;    // Time since last step
  f32 step_duration; // Seconds per step (tempo)

  // Tone generator
  SoundSource tone;

  // Control
  bool is_playing;
  bool loop; // Loop back to pattern 0 at end
} De100MusicSequencer;

// ─────────────────────────────────────────────────────────────────────────────
// Sequencer Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Initialize sequencer to default state
de100_file_scoped_fn inline void
de100_sequencer_init(De100MusicSequencer *seq) {
  seq->current_pattern = 0;
  seq->current_step = 0;
  seq->step_timer = 0.0f;
  seq->step_duration = 0.15f; // Default: ~100 BPM
  seq->tone.current_volume = 0.0f;
  seq->tone.volume = 0.3f;
  seq->tone.pan_position = 0.0f;
  seq->is_playing = false;
  seq->loop = true;
}

// Start playback from beginning
de100_file_scoped_fn inline void
de100_sequencer_play(De100MusicSequencer *seq) {
  seq->current_pattern = 0;
  seq->current_step = 0;
  seq->step_timer = 0.0f;
  seq->tone.current_volume = 0.0f;
  seq->is_playing = true;
}

// Stop playback (volume ramps down naturally)
de100_file_scoped_fn inline void
de100_sequencer_stop(De100MusicSequencer *seq) {
  seq->is_playing = false;
  seq->tone.is_playing = false;
}

// Update sequencer (call each frame with delta_time)
de100_file_scoped_fn inline void
de100_sequencer_update(De100MusicSequencer *seq, f32 delta_time) {
  if (!seq->is_playing)
    return;

  seq->step_timer += delta_time;

  if (seq->step_timer >= seq->step_duration) {
    seq->step_timer -= seq->step_duration;

    // Get current note
    u8 note = seq->patterns[seq->current_pattern][seq->current_step];

    if (note > 0) {
      seq->tone.frequency = de100_audio_midi_to_freq(note);
      seq->tone.is_playing = true;
    } else {
      seq->tone.is_playing = false;
    }

    // Advance step
    seq->current_step++;
    if (seq->current_step >= seq->steps_per_pattern) {
      seq->current_step = 0;
      seq->current_pattern++;

      if (seq->current_pattern >= seq->pattern_count) {
        if (seq->loop) {
          seq->current_pattern = 0;
        } else {
          seq->is_playing = false;
        }
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// MIXING HELPERS
// ═══════════════════════════════════════════════════════════════════════════

// Mix a mono sample into stereo output with panning
de100_file_scoped_fn inline void de100_audio_mix_sample_stereo(f32 sample,
                                                               f32 pan,
                                                               f32 *mix_left,
                                                               f32 *mix_right) {
  f32 left_vol, right_vol;
  de100_audio_calculate_pan(pan, &left_vol, &right_vol);
  *mix_left += sample * left_vol;
  *mix_right += sample * right_vol;
}

// Finalize mix: apply master volume and write one stereo frame to the buffer.
// mix_left / mix_right should be normalised amplitude (-1.0 to 1.0) BEFORE
// master_volume is applied.  The function delegates to audio_write_sample so
// format conversion happens at the platform boundary.
de100_file_scoped_fn inline void
de100_audio_finalize_stereo(GameAudioOutputBuffer *buf, i32 frame_index,
                            f32 mix_left, f32 mix_right, f32 master_volume) {
  audio_write_sample(buf, frame_index, mix_left * master_volume,
                     mix_right * master_volume);
}

#endif // DE100_GAME_AUDIO_HELPERS_H
