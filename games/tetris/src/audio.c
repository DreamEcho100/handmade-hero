#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    [SOUND_MOVE] = {200, 150, 50, 0.3f},
    [SOUND_ROTATE] = {300, 400, 80, 0.3f},
    [SOUND_DROP] = {150, 50, 100, 0.5f},
    [SOUND_LINE_CLEAR] = {400, 800, 200, 0.6f},
    [SOUND_TETRIS] = {300, 1200, 400, 0.8f},
    [SOUND_LEVEL_UP] = {440, 880, 300, 0.5f},
    [SOUND_GAME_OVER] = {400, 100, 500, 0.7f},
    [SOUND_RESTART] = {600, 1200, 200, 0.5f},
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Tetris Theme Patterns
 * ═══════════════════════════════════════════════════════════════════════════
 */

static const uint8_t TETRIS_PATTERN_A[MUSIC_PATTERN_LENGTH] = {
    76, 71, 72, 74, 72, 71, 69, 69, 72, 76, 74, 72, 71, 71, 72, 74,
};

static const uint8_t TETRIS_PATTERN_B[MUSIC_PATTERN_LENGTH] = {
    76, 0, 72, 0, 69, 69, 72, 74, 76, 0, 72, 0, 69, 0, 0, 0,
};

static const uint8_t TETRIS_PATTERN_C[MUSIC_PATTERN_LENGTH] = {
    74, 0, 74, 76, 78, 0, 76, 74, 72, 0, 72, 74, 76, 0, 74, 72,
};

static const uint8_t TETRIS_PATTERN_D[MUSIC_PATTERN_LENGTH] = {
    71, 0, 0, 72, 74, 0, 72, 71, 69, 0, 0, 0, 0, 0, 0, 0,
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_audio_init(GameAudioState *audio, int samples_per_second) {
  memset(audio, 0, sizeof(GameAudioState));

  audio->samples_per_second = samples_per_second;
  audio->master_volume = 0.8f;
  audio->sfx_volume = 1.0f;
  audio->music_volume = 0.5f;

  audio->music.step_duration = 0.15f;
  audio->music.tone.volume = 0.3f;
  audio->music.tone.current_volume = 0.0f;
  audio->music.tone.pan_position = 0.0f; /* Music centered */

  memcpy(audio->music.patterns[0], TETRIS_PATTERN_A, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[1], TETRIS_PATTERN_B, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[2], TETRIS_PATTERN_C, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[3], TETRIS_PATTERN_D, MUSIC_PATTERN_LENGTH);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Play Sound Effect (with panning!)
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
  inst->fade_in_samples = 96; /* ~2ms fade-in */

  /* Clamp pan position to valid range */
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

/* Original function - plays centered */
void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
  game_play_sound_at(audio, sound, 0.0f); /* Center pan */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Music Control
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_music_play(GameAudioState *audio) {
  audio->music.is_playing = 1;
  audio->music.current_pattern = 0;
  audio->music.current_step = 0;
  audio->music.step_timer = 0;
  audio->music.tone.current_volume = 0.0f;
  audio->music.tone.pan_position = 0.0f; /* Music stays centered */
}

void game_music_stop(GameAudioState *audio) {
  audio->music.is_playing = 0;
  audio->music.tone.is_playing = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Update Music Sequencer
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_audio_update(GameAudioState *audio, float delta_time) {
  if (!audio->music.is_playing)
    return;

  MusicSequencer *seq = &audio->music;

  seq->step_timer += delta_time;
  if (seq->step_timer >= seq->step_duration) {
    seq->step_timer -= seq->step_duration;

    uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];

    if (note > 0) {
      seq->tone.frequency = midi_to_freq(note);
      seq->tone.is_playing = 1;
    } else {
      seq->tone.is_playing = 0;
    }

    seq->current_step++;
    if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
      seq->current_step = 0;
      seq->current_pattern = (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Generate Audio Samples (with stereo panning!)
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
  GameAudioState *audio = &state->audio;
  int16_t *out = buffer->samples_buffer;
  int sample_count = buffer->sample_count;
  float inv_sample_rate = 1.0f / (float)buffer->samples_per_second;

  memset(out, 0, sample_count * 2 * sizeof(int16_t));

  const float VOLUME_RAMP_SPEED = 0.002f;

  for (int s = 0; s < sample_count; s++) {
    float mixed_left = 0.0f;
    float mixed_right = 0.0f;

    /* ─── Sound Effects (with panning) ─── */
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
      SoundInstance *inst = &audio->active_sounds[i];
      if (inst->samples_remaining <= 0)
        continue;

      float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;

      /* Envelope: fade-out at end */
      float env = (float)inst->samples_remaining / (float)inst->total_samples;

      /* Fade-in at start */
      int samples_played = inst->total_samples - inst->samples_remaining;
      if (samples_played < inst->fade_in_samples) {
        float fade_in = (float)samples_played / (float)inst->fade_in_samples;
        env *= fade_in;
      }

      float sample = wave * inst->volume * env * audio->sfx_volume;

      /* Apply panning */
      float left_vol, right_vol;
      calculate_pan_volumes(inst->pan_position, &left_vol, &right_vol);

      mixed_left += sample * left_vol;
      mixed_right += sample * right_vol;

      inst->phase += inst->frequency * inv_sample_rate;
      if (inst->phase >= 1.0f)
        inst->phase -= 1.0f;

      inst->frequency += inst->frequency_slide;
      inst->samples_remaining--;
    }

    /* ─── Music (with optional panning) ─── */
    if (audio->music.is_playing) {
      ToneGenerator *tone = &audio->music.tone;

      float target_volume = tone->is_playing ? tone->volume : 0.0f;

      if (tone->current_volume < target_volume) {
        tone->current_volume += VOLUME_RAMP_SPEED;
        if (tone->current_volume > target_volume)
          tone->current_volume = target_volume;
      } else if (tone->current_volume > target_volume) {
        tone->current_volume -= VOLUME_RAMP_SPEED;
        if (tone->current_volume < 0.0f)
          tone->current_volume = 0.0f;
      }

      if (tone->current_volume > 0.0001f || tone->is_playing) {
        float wave = (tone->phase < 0.5f) ? 1.0f : -1.0f;
        float sample = wave * tone->current_volume * audio->music_volume;

        /* Apply panning to music */
        float left_vol, right_vol;
        calculate_pan_volumes(tone->pan_position, &left_vol, &right_vol);

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

    *out++ = clamp_sample(mixed_left);  /* Left */
    *out++ = clamp_sample(mixed_right); /* Right */
  }
}
