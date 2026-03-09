/* ═══════════════════════════════════════════════════════════════════════════
 * audio.c  —  Snake Game
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "../utils/audio-helpers.h"

#include "audio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Effect Definitions (from Snake course)
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
    [SOUND_FOOD_EATEN] = {800.0f, 1200.0f, 80, 0.4f}, /* quick rising blip */
    [SOUND_GROW] = {220.0f, 330.0f, 30, 0.25f},      /* subtle segment-extend */
    [SOUND_RESTART] = {300.0f, 1000.0f, 220, 0.5f},  /* rising sweep jingle */
    [SOUND_GAME_OVER] = {400.0f, 100.0f, 500, 0.7f}, /* descending sad tone */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Snake Melody Patterns (from Snake course, converted to MIDI notes)
 * ═══════════════════════════════════════════════════════════════════════════
 * C5=72  D5=74  E5=76  G5=79  A5=81  G4=67
 */

static const uint8_t SNAKE_PATTERN_A[MUSIC_PATTERN_LENGTH] = {
    72, 76, 79, 81, 79, 76, 72, 67, /* Bar 1-2: C E G A G E C G4 */
    0,  74, 76, 79, 76, 74, 72, 0,  /* Bar 3-4: rest D E G E D C rest */
};

static const uint8_t SNAKE_PATTERN_B[MUSIC_PATTERN_LENGTH] = {
    72, 76, 79, 81, 79, 76, 72, 67, /* Repeat pattern A */
    0,  74, 76, 79, 76, 74, 72, 0,
};

static const uint8_t SNAKE_PATTERN_C[MUSIC_PATTERN_LENGTH] = {
    79, 0, 76, 0, 72, 72, 76, 79, /* Variation */
    81, 0, 79, 0, 76, 0,  0,  0,
};

static const uint8_t SNAKE_PATTERN_D[MUSIC_PATTERN_LENGTH] = {
    74, 0, 0, 76, 79, 0, 76, 74, /* Resolution */
    72, 0, 0, 0,  0,  0, 0,  0,
};

/* ═══════════════════════════════════════════════════════════════════════════
 * MIDI to Frequency conversion
 * ═══════════════════════════════════════════════════════════════════════════
 */

static inline float midi_to_freq(int note) {
  if (note == 0)
    return 0.0f;
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

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

  /* Convert 150ms step duration to samples */
  audio->music.step_duration_samples =
      (int)(0.15f * saved_sps); /* 7200 samples at 48kHz */
  audio->music.step_samples_remaining =
      0; /* Will trigger first note immediately */

  audio->music.tone.volume = 0.3f;
  audio->music.tone.current_volume = 0.0f;
  audio->music.tone.pan_position = 0.0f;

  memcpy(audio->music.patterns[0], SNAKE_PATTERN_A, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[1], SNAKE_PATTERN_B, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[2], SNAKE_PATTERN_C, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[3], SNAKE_PATTERN_D, MUSIC_PATTERN_LENGTH);

  printf("[AUDIO_INIT] samples_per_second=%d, step_duration_samples=%d, "
         "music_volume=%.2f\n",
         audio->samples_per_second, audio->music.step_duration_samples,
         audio->music_volume);
}

void game_music_play(GameAudioState *audio) {
  audio->music.is_playing = 1;
  audio->music.current_pattern = 0;
  audio->music.current_step = 0;
  audio->music.step_samples_remaining = 0; /* Trigger first note immediately */
  audio->music.tone.current_volume = 0.0f;
  audio->music.tone.pan_position = 0.0f;
  audio->music.tone.fade_samples_remaining = 0;
  audio->music.tone.is_playing = 0;
  audio->music.tone.phase = 0.0f;
  audio->music.tone.frequency = 0.0f;
  printf("[MUSIC_PLAY] Started music sequencer\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Play Sound Effect (with panning)
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
    slot = 0; /* Steal oldest slot */
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
  inst->fade_in_samples = 88; /* ~2ms fade-in at 44100 Hz */

  /* Clamp pan position */
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
  printf("[MUSIC_STOP] Stopped music sequencer\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Update Music Sequencer (called each frame) - NOW A NO-OP
 * Sequencer advances in game_get_audio_samples based on samples generated
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_audio_update(GameAudioState *audio, float delta_time) {
  (void)audio;
  (void)delta_time;
  /* Sequencer now advances in game_get_audio_samples based on samples generated
   */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Generate Audio Samples
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int g_step_count = 0;           /* DEBUG: track step changes */
static int g_sample_call_count = 0;    /* DEBUG: track calls */
static int g_last_fade_remaining = -1; /* DEBUG: track fade transitions */

void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buffer) {
  int16_t *out = buffer->samples_buffer;
  int sample_count = buffer->sample_count;
  float inv_sample_rate = 1.0f / (float)buffer->samples_per_second;

  memset(out, 0, sample_count * 2 * sizeof(int16_t));

  g_sample_call_count++;

  for (int s = 0; s < sample_count; s++) {
    float mixed_left = 0.0f;
    float mixed_right = 0.0f;

    /* ─── Sound Effects (with panning) ─── */
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

    /* ─── Music Sequencer (sample-based timing) ─── */
    if (audio->music.is_playing) {
      MusicSequencer *seq = &audio->music;
      ToneGenerator *tone = &seq->tone;

      /* Check if we need to advance to next step */
      if (seq->step_samples_remaining <= 0) {
        g_step_count++;

        /* Get current note */
        uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];
        float prev_freq = tone->frequency;
        int was_playing = tone->is_playing;

        if (note > 0) {
          float new_freq = midi_to_freq(note);

          /* Only reset phase when coming from silence */
          if (!was_playing || prev_freq == 0.0f) {
            printf("[STEP %3d] pattern=%d step=%2d note=%d freq=%.1f->%.1f "
                   "PLAY (from REST, phase reset)\n",
                   g_step_count, seq->current_pattern, seq->current_step, note,
                   prev_freq, new_freq);
            tone->phase = 0.0f;
            tone->fade_samples_remaining = MUSIC_FADE_SAMPLES;
          } else {
            printf("[STEP %3d] pattern=%d step=%2d note=%d freq=%.1f->%.1f "
                   "PLAY (continuous, phase=%.4f)\n",
                   g_step_count, seq->current_pattern, seq->current_step, note,
                   prev_freq, new_freq, tone->phase);
          }

          tone->frequency = new_freq;
          tone->is_playing = 1;
        } else {
          /* REST - fade out */
          printf("[STEP %3d] pattern=%d step=%2d note=REST prev_freq=%.1f STOP "
                 "fade_start=%d phase=%.4f\n",
                 g_step_count, seq->current_pattern, seq->current_step,
                 prev_freq, MUSIC_FADE_SAMPLES, tone->phase);
          tone->is_playing = 0;
          tone->fade_samples_remaining = MUSIC_FADE_SAMPLES;
        }

        /* Reset step counter */
        seq->step_samples_remaining = seq->step_duration_samples;

        /* Advance step */
        seq->current_step++;
        if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
          seq->current_step = 0;
          seq->current_pattern =
              (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
          printf("[PATTERN] Advanced to pattern %d\n", seq->current_pattern);
        }
      }

      seq->step_samples_remaining--;

      /* DEBUG: Log fade state changes */
      if (tone->fade_samples_remaining != g_last_fade_remaining) {
        if (tone->fade_samples_remaining == MUSIC_FADE_SAMPLES) {
          printf("[SAMPLES call=%d] FADE STARTED: is_playing=%d freq=%.1f "
                 "fade_remaining=%d current_vol=%.4f phase=%.4f\n",
                 g_sample_call_count, tone->is_playing, tone->frequency,
                 tone->fade_samples_remaining, tone->current_volume,
                 tone->phase);
        }
        g_last_fade_remaining = tone->fade_samples_remaining;
      }

      /* Generate music sample with fade envelope.
       *
       * FADE-OUT FIX: the old code used `target_volume * fade_mult` where
       * target_volume = 0 when is_playing=0.  0 * anything = 0, so the tone
       * jumped to silence on the very first sample of every REST — the main
       * cause of audible clicks.  We now drive the envelope from tone->volume
       * (the non-zero target level) and apply the curve directly.
       *
       * SMOOTHSTEP: t²(3-2t) eliminates the derivative discontinuity at both
       * ends of the ramp that the linear curve left behind.
       */
      if (tone->fade_samples_remaining > 0) {
        float fade_progress = 1.0f - ((float)tone->fade_samples_remaining /
                                      (float)MUSIC_FADE_SAMPLES);
        /* Smoothstep S(t) = t²(3 - 2t): smooth start AND end of the ramp */
        float t = fade_progress;
        float smooth = t * t * (3.0f - 2.0f * t);
        float fade_env;
        if (tone->is_playing) {
          fade_env = smooth; /* Fade IN:  0 → 1 */
        } else {
          fade_env = 1.0f - smooth; /* Fade OUT: 1 → 0 */
        }
        tone->current_volume = tone->volume * fade_env;
        tone->fade_samples_remaining--;

        /* DEBUG: Log when fade completes */
        if (tone->fade_samples_remaining == 0) {
          printf("[SAMPLES call=%d sample=%d] FADE COMPLETE: is_playing=%d "
                 "fade_env=%.4f current_vol=%.4f phase=%.4f\n",
                 g_sample_call_count, s, tone->is_playing, fade_env,
                 tone->current_volume, tone->phase);
        }
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
