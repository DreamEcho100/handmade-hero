/* ═══════════════════════════════════════════════════════════════════════════
 * audio.c  —  Snake Game
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "../utils/audio-helpers.h"

#include "audio.h"
#include <math.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Effect Definitions
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  float frequency;
  float frequency_end;
  float duration_ms;
  float volume;
} SoundDef;

static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    [SOUND_NONE] = {0, 0, 0, 0.0f},
    [SOUND_FOOD_EATEN] = {800.0f, 1200.0f, 80, 0.4f},
    [SOUND_GROW] = {220.0f, 330.0f, 30, 0.25f},
    [SOUND_RESTART] = {300.0f, 1000.0f, 220, 0.5f},
    [SOUND_GAME_OVER] = {400.0f, 100.0f, 500, 0.7f},
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Snake Melody Patterns (MIDI notes)
 * C5=72  D5=74  E5=76  G5=79  A5=81  G4=67
 * ═══════════════════════════════════════════════════════════════════════════
 */

static const uint8_t SNAKE_PATTERN_A[MUSIC_PATTERN_LENGTH] = {
    72, 76, 79, 81, 79, 76, 72, 67, 0, 74, 76, 79, 76, 74, 72, 0,
};

static const uint8_t SNAKE_PATTERN_B[MUSIC_PATTERN_LENGTH] = {
    72, 76, 79, 81, 79, 76, 72, 67, 0, 74, 76, 79, 76, 74, 72, 0,
};

static const uint8_t SNAKE_PATTERN_C[MUSIC_PATTERN_LENGTH] = {
    79, 0, 76, 0, 72, 72, 76, 79, 81, 0, 79, 0, 76, 0, 0, 0,
};

static const uint8_t SNAKE_PATTERN_D[MUSIC_PATTERN_LENGTH] = {
    74, 0, 0, 76, 79, 0, 76, 74, 72, 0, 0, 0, 0, 0, 0, 0,
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_audio_init(GameAudioState *audio) {
  int saved_sps = audio->samples_per_second;

  memset(audio, 0, sizeof(GameAudioState));

  audio->samples_per_second = saved_sps;

  audio->master_volume = 0.8f;
  audio->sfx_volume = 1.0f;
  audio->music_volume = 0.3f;

  audio->music.step_duration_samples = (int)(0.15f * saved_sps);
  audio->music.step_samples_remaining = 0;

  audio->music.tone.volume = 0.3f;
  audio->music.tone.current_volume = 0.0f;
  audio->music.tone.pan_position = 0.0f;

  memcpy(audio->music.patterns[0], SNAKE_PATTERN_A, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[1], SNAKE_PATTERN_B, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[2], SNAKE_PATTERN_C, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[3], SNAKE_PATTERN_D, MUSIC_PATTERN_LENGTH);
}

void game_music_play(GameAudioState *audio) {
  audio->music.is_playing = 1;
  audio->music.current_pattern = 0;
  audio->music.current_step = 0;
  audio->music.step_samples_remaining = 0;
  audio->music.tone.current_volume = 0.0f;
  audio->music.tone.pan_position = 0.0f;
  audio->music.tone.fade_samples_remaining = 0;
  audio->music.tone.is_playing = 0;
  audio->music.tone.phase = 0.0f;
  audio->music.tone.frequency = 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Play Sound Effect
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_play_sound_at(GameAudioState *audio, SOUND_ID sound,
                        float pan_position) {
  if (sound == SOUND_NONE || sound >= SOUND_COUNT)
    return;

  int slot = -1;
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    if (audio->active_sounds[i].samples_remaining <= 0) {
      slot = i;
      break;
    }
  }

  if (slot < 0) {
    slot = 0;
  }

  const SoundDef *def = &SOUND_DEFS[sound];
  SoundInstance *inst = &audio->active_sounds[slot];

  inst->sound_id = sound;
  inst->phase = 0.0f;
  inst->frequency = def->frequency;
  inst->volume = def->volume;
  inst->samples_remaining =
      (int)(def->duration_ms * audio->samples_per_second / 1000.0f);
  inst->total_samples = inst->samples_remaining;
  inst->fade_in_samples = 88;

  if (pan_position < -1.0f)
    pan_position = -1.0f;
  if (pan_position > 1.0f)
    pan_position = 1.0f;
  inst->pan_position = pan_position;

  if (def->frequency_end > 0 && inst->samples_remaining > 0) {
    inst->frequency_slide =
        (def->frequency_end - def->frequency) / inst->samples_remaining;
  } else {
    inst->frequency_slide = 0;
  }
}

void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
  game_play_sound_at(audio, sound, 0.0f);
}

void game_music_stop(GameAudioState *audio) {
  audio->music.is_playing = 0;
  audio->music.tone.is_playing = 0;
}

void game_audio_update(GameAudioState *audio, float delta_time) {
  (void)audio;
  (void)delta_time;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Generate Audio Samples
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buffer) {
  /* Safety cap: never write past the allocated buffer.
   * Platforms should already clamp before calling us, but this makes the
   * game layer self-defending against accidental misconfiguration. */
  int sample_count = buffer->sample_count;
  if (buffer->max_sample_count > 0 && sample_count > buffer->max_sample_count) {
    sample_count = buffer->max_sample_count;
  }

  int16_t *out = buffer->samples_buffer;
  float inv_sample_rate = 1.0f / (float)buffer->samples_per_second;

  memset(out, 0, sample_count * 2 * sizeof(int16_t));

  for (int s = 0; s < sample_count; s++) {
    float mixed_left = 0.0f;
    float mixed_right = 0.0f;

    /* ─── Sound Effects ─── */
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
      SoundInstance *inst = &audio->active_sounds[i];
      if (inst->samples_remaining <= 0)
        continue;

      float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;

      float env = (float)inst->samples_remaining / (float)inst->total_samples;

      int samples_played = inst->total_samples - inst->samples_remaining;
      if (samples_played < inst->fade_in_samples) {
        float fade_in = (float)samples_played / (float)inst->fade_in_samples;
        env *= fade_in;
      }

      float sample = wave * inst->volume * env * audio->sfx_volume;

      float left_vol, right_vol;
      audio_calculate_pan(inst->pan_position, &left_vol, &right_vol);

      mixed_left += sample * left_vol;
      mixed_right += sample * right_vol;

      inst->phase += inst->frequency * inv_sample_rate;
      if (inst->phase >= 1.0f)
        inst->phase -= 1.0f;

      inst->frequency += inst->frequency_slide;
      inst->samples_remaining--;
    }

    /* ─── Music Sequencer ─── */
    if (audio->music.is_playing) {
      MusicSequencer *seq = &audio->music;
      ToneGenerator *tone = &seq->tone;

      /* Advance to next step when current step expires */
      if (seq->step_samples_remaining <= 0) {
        uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];

        if (note > 0) {
          float new_freq = audio_midi_to_freq(note);

          /* Only reset phase when coming from silence */
          if (!tone->is_playing || tone->frequency == 0.0f) {
            tone->phase = 0.0f;
            tone->fade_samples_remaining = MUSIC_FADE_SAMPLES;
          }

          tone->frequency = new_freq;
          tone->is_playing = 1;
        } else {
          /* REST — fade out */
          tone->is_playing = 0;
          tone->fade_samples_remaining = MUSIC_FADE_SAMPLES;
        }

        seq->step_samples_remaining = seq->step_duration_samples;

        seq->current_step++;
        if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
          seq->current_step = 0;
          seq->current_pattern =
              (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
        }
      }

      seq->step_samples_remaining--;

      /* Fade envelope (smoothstep) */
      if (tone->fade_samples_remaining > 0) {
        float fade_progress = 1.0f - ((float)tone->fade_samples_remaining /
                                      (float)MUSIC_FADE_SAMPLES);
        float t = fade_progress;
        float smooth = t * t * (3.0f - 2.0f * t);
        float fade_env;
        if (tone->is_playing) {
          fade_env = smooth;
        } else {
          fade_env = 1.0f - smooth;
        }
        tone->current_volume = tone->volume * fade_env;
        tone->fade_samples_remaining--;
      } else {
        tone->current_volume = tone->is_playing ? tone->volume : 0.0f;
      }

      if (tone->current_volume > 0.0001f) {
        float wave = (tone->phase < 0.5f) ? 1.0f : -1.0f;
        float sample = wave * tone->current_volume * audio->music_volume;

        float left_vol, right_vol;
        audio_calculate_pan(tone->pan_position, &left_vol, &right_vol);

        mixed_left += sample * left_vol;
        mixed_right += sample * right_vol;

        tone->phase += tone->frequency * inv_sample_rate;
        if (tone->phase >= 1.0f)
          tone->phase -= 1.0f;
      }
    }

    /* ─── Final Mix ─── */
    mixed_left *= audio->master_volume * 16000.0f;
    mixed_right *= audio->master_volume * 16000.0f;

    *out++ = audio_clamp_sample(mixed_left);
    *out++ = audio_clamp_sample(mixed_right);
  }
}
