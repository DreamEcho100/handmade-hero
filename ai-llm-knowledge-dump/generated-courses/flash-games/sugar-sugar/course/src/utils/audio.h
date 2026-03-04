/*
 * utils/audio.h  —  Sugar, Sugar | Procedural Audio Types
 *
 * All audio state lives in GameAudioState (embedded in GameState).
 * The platform layer asks the game for samples via game_get_audio_samples();
 * the game never calls back into the platform for audio.
 *
 * Architecture:
 *   GameState.audio  ← GameAudioState
 *     .active_sounds[] ← SoundInstance pool (up to MAX_SIMULTANEOUS_SOUNDS)
 *     .title_seq       ← MusicSequencer  (dreamy title screen melody)
 *     .gameplay_seq    ← MusicSequencer  (upbeat gameplay melody)
 *     .active_music    ← 0=title, 1=gameplay, -1=none
 *
 * Wave types:
 *   WAVE_SQUARE   — short event sounds (blips, hits, sweeps) — digital/retro
 *   WAVE_TRIANGLE — sustained/looping sounds — smooth, no harsh clicks
 *
 * JS analogy: this is like an AudioWorklet's internal state — pure data,
 * no Web Audio API objects. The "processor" is game_get_audio_samples().
 */

#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H

#include <math.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Output Buffer
 * ═══════════════════════════════════════════════════════════════════════════
 * Platform fills this struct, game writes samples into it.
 * samples[] is interleaved stereo: L0, R0, L1, R1, ...
 */
typedef struct {
  int16_t *samples;       /* interleaved stereo: L0,R0,L1,R1,...  */
  int samples_per_second; /* usually 44100 or 48000               */
  int sample_count;       /* stereo pairs to generate this call   */
} AudioOutputBuffer;

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Effect Identifiers
 * ═══════════════════════════════════════════════════════════════════════════
 * Named by intent (not "beep1", "beep2").  Each maps to a SoundDef entry
 * in audio.c that fully describes the procedural tone.
 */
typedef enum {
  SOUND_NONE = 0,
  SOUND_CUP_FILL,        /* sweet rising chime when a cup reaches 100%       */
  SOUND_LEVEL_COMPLETE,  /* bright multi-note fanfare when all cups filled    */
  SOUND_TITLE_SELECT,    /* soft "ping" when selecting a level on the menu    */
  SOUND_GRAVITY_FLIP,    /* descending whoosh when gravity reverses           */
  SOUND_RESET,           /* short gentle tone on level restart                */
  SOUND_COUNT
} SOUND_ID;

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Instance
 * ═══════════════════════════════════════════════════════════════════════════
 * One entry in the active-sounds pool.  Holds all oscillator + envelope
 * state for one currently playing tone.
 *
 * CRITICAL: phase must NEVER be reset between frames — that creates a
 * discontinuity (click) in the waveform.  Only reset it at birth (slot init).
 *
 * fade_in_samples  — short ramp from 0→volume at the sound's start
 * fade_out_samples — short ramp from volume→0 at the sound's end
 * Both together form a trapezoidal envelope: no clicks at attack or release.
 */
#define MAX_SIMULTANEOUS_SOUNDS 8

typedef struct {
  SOUND_ID sound_id;
  float    phase;              /* oscillator phase [0.0, 1.0) — never reset */
  int      samples_remaining;  /* counts down to 0                          */
  int      total_samples;      /* samples at birth (for envelope math)      */
  int      fade_in_samples;    /* attack ramp length (~10 ms)               */
  int      fade_out_samples;   /* release ramp length (~10 ms)              */
  float    volume;             /* peak amplitude [0.0, 1.0]                 */
  float    frequency;          /* current pitch in Hz                       */
  float    frequency_slide;    /* Hz added per sample (linear pitch glide)  */
  float    pan_position;       /* -1 = left, 0 = center, 1 = right          */
} SoundInstance;

/* ═══════════════════════════════════════════════════════════════════════════
 * Tone Generator  (used for music voices)
 * ═══════════════════════════════════════════════════════════════════════════
 * Holds oscillator state for one continuous music voice.
 * current_volume is smoothed toward volume each sample to prevent clicks.
 *
 * Wave shape: WAVE_TRIANGLE — produces smooth ramps, no harsh polarity flip.
 * Formula: wave = 1.0f - fabsf(4.0f * phase - 2.0f)
 * Output: triangular wave in [-1, +1], frequency = same as square wave.
 */
typedef struct {
  float phase;          /* oscillator phase [0.0, 1.0)              */
  float frequency;      /* target pitch in Hz                       */
  float volume;         /* target amplitude [0.0, 1.0]              */
  float current_volume; /* smoothed (ramped) amplitude — click-free */
  float pan_position;   /* -1 = left, 0 = center, 1 = right        */
  int   is_playing;     /* 0 = note off (ramps to silence)          */
} ToneGenerator;

/* ═══════════════════════════════════════════════════════════════════════════
 * Music Sequencer  (simple pattern-based)
 * ═══════════════════════════════════════════════════════════════════════════
 * Cycles through MUSIC_NUM_PATTERNS × MUSIC_PATTERN_LENGTH steps.
 * Each step is a MIDI note (0 = rest, 60 = middle C, 69 = A4 = 440 Hz).
 * step_timer counts seconds; when ≥ step_duration the sequencer advances.
 *
 * volume_scale is used for smooth fade-in/fade-out when switching tracks.
 * It is ramped by game_audio_update and applied in game_get_audio_samples.
 */
#define MUSIC_PATTERN_LENGTH 16
#define MUSIC_NUM_PATTERNS   4

typedef struct {
  uint8_t       patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
  int           current_pattern;
  int           current_step;
  float         step_timer;       /* accumulates delta_time                */
  float         step_duration;    /* seconds per step                      */
  ToneGenerator tone;
  int           is_playing;       /* 1 = sequencer is active               */
  float         volume_scale;     /* 0.0→1.0 for fade-in/out between tracks*/
} MusicSequencer;

/* ═══════════════════════════════════════════════════════════════════════════
 * Game Audio State
 * ═══════════════════════════════════════════════════════════════════════════
 * Embed one of these in GameState.  The platform never touches it directly;
 * game.c manages it via game_play_sound / game_audio_update, and the
 * platform reads it indirectly through game_get_audio_samples().
 */
typedef struct {
  SoundInstance  active_sounds[MAX_SIMULTANEOUS_SOUNDS];

  /* Two music tracks: title screen and gameplay */
  MusicSequencer title_seq;    /* dreamy ambient — plays on PHASE_TITLE   */
  MusicSequencer gameplay_seq; /* upbeat pentatonic — plays during play   */
  int            active_music; /* 0=title, 1=gameplay, -1=silence         */

  float master_volume;     /* overall output scale [0.0, 1.0]  */
  float sfx_volume;        /* sound-effect bus scale           */
  float music_volume;      /* music bus scale                  */

  int samples_per_second;  /* set by platform at init time     */
} GameAudioState;

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline helpers
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Convert MIDI note number to frequency.  A4 (69) = 440 Hz. */
static inline float midi_to_freq(int note) {
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

/* Clamp a float sample to the int16_t range. */
static inline int16_t clamp_sample(float s) {
  if (s >  32767.0f) return  32767;
  if (s < -32768.0f) return -32768;
  return (int16_t)s;
}

/* Constant-power stereo pan.
 * pan -1 = full left, 0 = center, +1 = full right. */
static inline void calculate_pan_volumes(float pan, float *left, float *right) {
  if (pan <= 0.0f) { *left = 1.0f; *right = 1.0f + pan; }
  else             { *left = 1.0f - pan; *right = 1.0f;  }
}

/* Triangle wave: smooth, no abrupt polarity flip — use for music/looping.
 * Output range: [-1.0, +1.0].  Same as square in terms of CPU cost.
 * Rule: WAVE_SQUARE for short event sounds; WAVE_TRIANGLE for sustained/music. */
static inline float wave_triangle(float phase) {
  /* Maps phase [0,1] to a triangular shape:
   *   phase=0.00 → 0.0
   *   phase=0.25 → +1.0
   *   phase=0.50 → 0.0
   *   phase=0.75 → -1.0
   *   phase=1.00 → 0.0 */
  return 1.0f - fabsf(4.0f * phase - 2.0f);
}

#endif /* UTILS_AUDIO_H */
