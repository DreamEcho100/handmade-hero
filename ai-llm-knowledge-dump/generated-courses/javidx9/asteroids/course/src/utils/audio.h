#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/audio.h — Audio System Types for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Two-loop audio architecture
 * ───────────────────────────
 * The platform backend runs TWO independent loops:
 *
 *   GAME LOOP  (≈60 Hz):  game_audio_update(state, dt_ms)
 *     → advances MusicSequencer, applies volume envelopes, removes
 *       expired voices.  Mutates `GameAudioState`.
 *
 *   AUDIO LOOP (callback/drain-loop):  game_get_audio_samples(state, buf, n)
 *     → reads `GameAudioState` (read-mostly), synthesizes PCM samples, fills
 *       `AudioOutputBuffer`.
 *
 * By separating "advance state" from "synthesize samples" we avoid stutter.
 *
 * Float32 PCM
 * ───────────
 * Samples are float in [-1.0, +1.0].  No int16_t scaling needed.
 *   Raylib: bitsPerSample=32, AUDIO_FORMAT_FLOAT32.
 *   ALSA:   SND_PCM_FORMAT_FLOAT_LE.
 *
 * Asteroids SOUND_IDs
 * ────────────────────
 * Asteroids replaces the platform-backend demo tones with game-specific
 * sounds defined in game/audio.c.  The SoundDef table (SOUND_DEFS) lives
 * in game/audio.c; this header only defines the enum.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include <string.h>   /* memset */

/* ── PCM constants ────────────────────────────────────────────────────────
 * 44100 Hz, stereo, float32: AUDIO_BYTES_PER_FRAME = 8 (2 × 4 bytes).   */
#define AUDIO_SAMPLE_RATE     44100
#define AUDIO_CHANNELS        2
#define AUDIO_BYTES_PER_FRAME (AUDIO_CHANNELS * sizeof(float))

/* Chunk size for stream-fill loop.
 * 4096 frames ≈ 93 ms at 44100 Hz.  Matches Raylib's
 * SetAudioStreamBufferSizeDefault(4096).                                   */
#define AUDIO_CHUNK_SIZE      4096

/* ── AudioOutputBuffer ────────────────────────────────────────────────────
 * Flat interleaved stereo format: [L0, R0, L1, R1, L2, R2, ...]
 * Each pair (Li, Ri) is one "sample frame".
 * `sample_count` is the max frames the buffer can hold.
 * `max_sample_count` is a safety cap: never write more than this.         */
typedef struct {
  float   *samples;         /* Allocated by platform on startup.           */
  int      sample_count;    /* Frames allocated (capacity).                */
  int      max_sample_count;/* Safety cap; set = sample_count on init.     */
} AudioOutputBuffer;

/* Float32 write helper.
 * Writes float L/R values in [-1.0, +1.0] directly to the interleaved
 * buffer at frame `frame_index`.                                           */
static inline void audio_write_sample(AudioOutputBuffer *buf,
                                      int frame_index,
                                      float l, float r) {
  buf->samples[frame_index * AUDIO_CHANNELS + 0] = l;
  buf->samples[frame_index * AUDIO_CHANNELS + 1] = r;
}

/* ── Wave and envelope types ──────────────────────────────────────────────
 * Used by both SoundDef and ToneGenerator.                                */
#define WAVE_SQUARE    0   /* Retro square: +1 below 50%, -1 above.       */
#define WAVE_SAWTOOTH  1   /* Linear ramp -1 → +1 per period.             */
#define WAVE_NOISE     2   /* Random sample each frame (explosion rumble). */
#define WAVE_TRIANGLE  3   /* Smooth ramp: -1→+1→-1 per period (no clicks).*/

#define ENVELOPE_FADEOUT   0 /* Linear decay: amplitude 1→0 over full duration. */
#define ENVELOPE_TRAPEZOID 1 /* Attack 10% + sustain 80% + decay 10%.           */

/* ── ToneGenerator ────────────────────────────────────────────────────────
 * One active SFX voice in the voice pool.
 *
 * `phase_acc`:        0.0–1.0 accumulator; advances by freq/sample_rate per
 *                     sample.  Wraps at 1.0 to stay in [0, 1).
 *
 * `frequency`:        Hz.  Set from SoundDef; may slide (freq_slide != 0).
 *
 * `freq_slide`:       Hz/second change applied per sample in synthesis.
 *                     Enables simple pitch sweeps (laser, ship death, etc.).
 *
 * `volume`:           0.0–1.0 amplitude scale.
 *
 * `pan`:              -1.0 (full left) to +1.0 (full right). 0 = centre.
 *                     Asteroids explosions pan based on asteroid screen X.
 *
 * `samples_remaining`: frames left to render.  Decrements to 0, then voice
 *                     deactivates.
 *
 * `total_samples`:    Set = samples_remaining when voice is triggered.
 *                     Used for decay envelope: env = remaining / total.
 *
 * `wave_type`:        WAVE_SQUARE / SAWTOOTH / NOISE / TRIANGLE.
 *
 * `envelope_type`:    ENVELOPE_FADEOUT or ENVELOPE_TRAPEZOID.
 *
 * `age`:              Monotonic counter.  Used by steal-oldest policy.     */
typedef struct {
  float phase_acc;
  float frequency;
  float freq_slide;       /* Hz per second (applied per sample in synthesis).*/
  float volume;
  float pan;
  int   samples_remaining;
  int   total_samples;    /* Set on trigger; used for decay envelope.        */
  int   wave_type;        /* WAVE_SQUARE / SAWTOOTH / NOISE / TRIANGLE.      */
  int   envelope_type;    /* ENVELOPE_FADEOUT or ENVELOPE_TRAPEZOID.         */
  int   active;
  int   age;
} ToneGenerator;

/* ── SoundDef ─────────────────────────────────────────────────────────────
 * Static description of one sound effect.  Lives in a const table
 * (SOUND_DEFS[]) in game/audio.c.                                         */
typedef struct {
  float frequency;
  float freq_slide;     /* Hz per second.                                  */
  float volume;
  float pan;            /* Centre pan for one-shots; overridden by panned. */
  int   duration_ms;    /* Duration in milliseconds.                       */
  int   wave_type;      /* WAVE_SQUARE / SAWTOOTH / NOISE / TRIANGLE.      */
  int   envelope_type;  /* ENVELOPE_FADEOUT or ENVELOPE_TRAPEZOID.         */
} SoundDef;

/* ── Asteroids Sound IDs ─────────────────────────────────────────────────
 * Enum indices into SOUND_DEFS[] defined in game/audio.c.
 *
 * SOUND_THRUST        — low rumble while thrusting (looped by game.c)
 * SOUND_FIRE          — short ping on bullet fire
 * SOUND_EXPLODE_LARGE — deep boom when a large asteroid is split
 * SOUND_EXPLODE_MEDIUM — mid crack when a medium asteroid is split
 * SOUND_EXPLODE_SMALL — high snap when a small asteroid is destroyed
 * SOUND_SHIP_DEATH    — descending whomp when player is killed
 * SOUND_ROTATE        — subtle whir while rotating (looped by game.c)
 * SOUND_MUSIC_GAMEPLAY — background drone during play (looped)
 * SOUND_MUSIC_RESTART  — ascending jingle on game restart
 */
typedef enum {
  SOUND_NONE = 0,
  SOUND_THRUST,
  SOUND_FIRE,
  SOUND_EXPLODE_LARGE,
  SOUND_EXPLODE_MEDIUM,
  SOUND_EXPLODE_SMALL,
  SOUND_SHIP_DEATH,
  SOUND_ROTATE,
  SOUND_MUSIC_GAMEPLAY,
  SOUND_MUSIC_RESTART,
  SOUND_COUNT           /* always last — used as array length              */
} SoundID;

/* ── Sound pool ───────────────────────────────────────────────────────────
 * Asteroids can have many simultaneous explosions.
 * 16 slots provides safe head-room for worst-case scenarios.              */
#define MAX_SOUNDS 16

/* ── MusicTone ───────────────────────────────────────────────────────────
 * Dedicated oscillator for background music.
 *
 * Phase is PRESERVED across note boundaries (not reset each note).
 * This prevents waveform discontinuity at note changes → no clicking.
 *
 * Volume fades smoothly between notes using smoothstep: t² × (3 − 2t)
 * over MUSIC_FADE_SAMPLES frames.                                         */
typedef struct {
  float phase;                  /* [0, 1) oscillator phase.                 */
  float frequency;              /* Hz. 0 = silence.                         */
  float volume;                 /* Target amplitude (0–1).                  */
  float current_volume;         /* Smoothstep-interpolated working volume.  */
  float pan;                    /* -1 = left, 0 = centre, +1 = right.       */
  int   is_playing;             /* 1 = tone active; 0 = fading to silence.  */
  int   fade_samples_remaining; /* Countdown for smoothstep transition.     */
} MusicTone;

/* Smoothstep fade length: 441 samples ≈ 10 ms at 44100 Hz.               */
#define MUSIC_FADE_SAMPLES 441

/* ── MusicSequencer ──────────────────────────────────────────────────────
 * MIDI-note step sequencer for background music.
 *
 * Advances INSIDE game_get_audio_samples (per-sample, not per game frame).
 * This is "sample-accurate" sequencing: note boundaries happen exactly on
 * the correct sample.
 *
 * Patterns are arrays of MIDI note numbers (uint8_t).
 *   0  = REST (silence for that step).
 *   60 = C4 (261.63 Hz), 69 = A4 (440 Hz), 72 = C5 (523.25 Hz), etc.   */
typedef struct {
  const uint8_t **patterns;      /* Array of pattern pointers.              */
  int             pattern_len;   /* Steps per pattern.                      */
  int             pattern_count; /* Number of patterns.                     */
  int             current_pattern;
  int             current_step;
  int             step_duration_samples; /* Samples per step (set at init). */
  int             step_samples_remaining;/* Countdown to next step.         */
  MusicTone       tone;          /* The single persistent music oscillator. */
  int             playing;       /* 1 = sequencer active.                   */
} MusicSequencer;

/* ── GameAudioState ──────────────────────────────────────────────────────
 * Top-level audio state shared between game loop and audio loop.
 *
 * `voices`             — SFX voice pool (ToneGenerator, one per active effect).
 * `sequencer`          — Background music sequencer with dedicated MusicTone.
 * `samples_per_second` — Set at init from AUDIO_SAMPLE_RATE.
 * `master_volume`      — Global output scale.
 * `sfx_volume`         — Per-category volume for sound effects (0–1).
 * `music_volume`       — Per-category volume for background music (0–1).
 *
 * Embed this struct inside GameState; pass a pointer to
 * game_get_audio_samples and game_audio_update.                           */
typedef struct {
  ToneGenerator  voices[MAX_SOUNDS];
  int            next_age;           /* Monotonic counter for steal-oldest. */
  MusicSequencer sequencer;
  int            samples_per_second; /* Set at init from AUDIO_SAMPLE_RATE. */
  float          master_volume;      /* Global output scale (0–1).          */
  float          sfx_volume;         /* Per-category SFX volume (0–1).      */
  float          music_volume;       /* Per-category music volume (0–1).    */
} GameAudioState;

/* ── PlatformAudioConfig ─────────────────────────────────────────────────
 * Platform-specific audio context passed through platform_audio_* functions.
 * Backends populate this struct in platform_audio_init().                 */
typedef struct {
  int sample_rate;    /* Always AUDIO_SAMPLE_RATE (44100).                  */
  int channels;       /* Always AUDIO_CHANNELS (2).                         */
  int chunk_size;     /* Platform ring-buffer chunk size (AUDIO_CHUNK_SIZE).*/

  /* ---- Raylib backend ---- */
  void *raylib_stream;    /* AudioStream* allocated by raylib backend.      */

  /* ---- X11/ALSA backend ---- */
  void *alsa_pcm;         /* snd_pcm_t* allocated by x11 audio backend.    */
  int   alsa_buffer_size; /* hw_buffer_size returned by snd_pcm_hw_params.  */
} PlatformAudioConfig;

#endif /* UTILS_AUDIO_H */
