#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H

#include <math.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Output Buffer
 * ═══════════════════════════════════════════════════════════════════════════
 * Platform fills this struct, game writes samples into it.
 */
typedef struct {
  int16_t *samples_buffer; /* Interleaved stereo: L0,R0,L1,R1,... */
  int samples_per_second;  /* Usually 44100 or 48000 */
  int sample_count;        /* How many stereo pairs to generate */
} AudioOutputBuffer;

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Effect Identifiers
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef enum {
  SOUND_NONE = 0,
  SOUND_MOVE,       /* Piece moved left/right */
  SOUND_ROTATE,     /* Piece rotated */
  SOUND_DROP,       /* Piece locked in place */
  SOUND_LINE_CLEAR, /* 1-3 lines cleared */
  SOUND_TETRIS,     /* 4 lines cleared (special!) */
  SOUND_LEVEL_UP,   /* Level increased */
  SOUND_GAME_OVER,  /* Game ended */
  SOUND_RESTART,    /* Game restarted */
  SOUND_COUNT
} SOUND_ID;

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Instance
 * ═══════════════════════════════════════════════════════════════════════════
 * Represents one currently-playing sound effect.
 */
#define MAX_SIMULTANEOUS_SOUNDS 4

typedef struct {
  SOUND_ID sound_id;     /* Which sound is playing */
  float phase;           /* Current phase for oscillator */
  int samples_remaining; /* How many samples left to play */
  float volume;          /* 0.0 to 1.0 */
  float frequency;       /* Hz (for procedural sounds) */
  float frequency_slide; /* Hz per sample (for pitch bend) */
  int total_samples;
  int fade_in_samples;
  float pan_position; // -1.0 (left) to 1.0 (right)
} SoundInstance;

/* ═══════════════════════════════════════════════════════════════════════════
 * Tone Generator (for background music)
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  float phase;          /* 0.0 to 1.0 (wraps) */
  float frequency;      /* Hz */
  float volume;         /* 0.0 to 1.0 */
  float pan_position;   // -1.0 (left) to 1.0 (right)
  float current_volume; // Smoothed volume for click-free transitions
  int is_playing;
} ToneGenerator;

/* ═══════════════════════════════════════════════════════════════════════════
 * Music Sequencer (simple pattern-based)
 * ═══════════════════════════════════════════════════════════════════════════
 */
#define MUSIC_PATTERN_LENGTH 16
#define MUSIC_NUM_PATTERNS 4

typedef struct {
  /* Notes: MIDI-style, 0 = rest, 60 = middle C */
  uint8_t patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
  int current_pattern;
  int current_step;
  float step_timer;    /* Seconds until next step */
  float step_duration; /* Seconds per step (tempo) */
  ToneGenerator tone;
  int is_playing;
} MusicSequencer;

/* ═══════════════════════════════════════════════════════════════════════════
 * Game Audio State
 * ═══════════════════════════════════════════════════════════════════════════
 * Include this in your GameState.
 */
typedef struct {
  /* Sound effects */
  SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
  int active_sound_count;

  /* Music */
  MusicSequencer music;

  /* Master volume */
  float master_volume; /* 0.0 to 1.0 */
  float sfx_volume;    /* 0.0 to 1.0 */
  float music_volume;  /* 0.0 to 1.0 */

  /* Samples per second (set by platform) */
  int samples_per_second;
} GameAudioState;

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Convert MIDI note to frequency: A4 (69) = 440 Hz */
static inline float midi_to_freq(int note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

/* Clamp sample to int16 range */
static inline int16_t clamp_sample(float sample) {
  if (sample > 32767.0f)
    return 32767;
  if (sample < -32768.0f)
    return -32768;
  return (int16_t)sample;
}

/* Helper to calculate stereo volumes from pan position */
static inline void calculate_pan_volumes(float pan, float *left_vol,
                                         float *right_vol) {
  /* pan: -1.0 = full left, 0.0 = center, 1.0 = full right */
  /* Using constant power panning for smoother transitions */
  if (pan <= 0.0f) {
    *left_vol = 1.0f;
    *right_vol = 1.0f + pan; /* pan is negative, so this reduces right */
  } else {
    *left_vol = 1.0f - pan;
    *right_vol = 1.0f;
  }
}

/* Field boundaries for panning calculation */
#define FIELD_CENTER                                                           \
  ((FIELD_WIDTH - 2) / 2.0f + 1.0f) /* Center column (excluding walls) */

#endif /* UTILS_AUDIO_H */