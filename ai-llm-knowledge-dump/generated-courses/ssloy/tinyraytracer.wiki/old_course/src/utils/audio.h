#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/audio.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 09 — AudioOutputBuffer, interleaved stereo layout, phase
 *             accumulator, GameAudioState, audio_write_sample helper.
 *
 * LESSON 10 — ToneGenerator, SoundDef, SoundPool (free-slot + steal-oldest).
 *
 * LESSON 13 — MusicSequencer, MusicNote.
 *
 * Two-loop audio architecture
 * ───────────────────────────
 * The platform backend runs TWO independent loops:
 *
 *   GAME LOOP  (≈60 Hz):  game_audio_update(state, dt)
 *     → advances the MusicSequencer, applies volume envelopes, removes
 *       expired voices.  Mutates `GameAudioState`.
 *
 *   AUDIO LOOP (callback/drain-loop):  game_get_audio_samples(state, buf, n)
 *     → reads `GameAudioState` (read-mostly), synthesizes PCM samples, fills
 *       `AudioOutputBuffer`.
 *
 * By separating "advance state" from "synthesize samples" we avoid stutter:
 * the audio loop never has to wait for game logic.
 *
 * JS analogy:
 *   GAME LOOP  ≈ updating an `AudioWorkletProcessor`'s internal state dict
 *   AUDIO LOOP ≈ the `process()` callback that fills Float32Arrays
 *
 * audio_write_sample helper
 * ──────────────────────────
 * LESSON 09 — Wraps the int16_t conversion in one place.  When a future
 * course upgrades to float PCM, only this helper changes.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include <string.h>   /* memset */

/* ── PCM constants ────────────────────────────────────────────────────────
 * LESSON 09 — Why 44100 Hz and 16-bit signed (CD quality, widest compat).  */
#define AUDIO_SAMPLE_RATE     44100
#define AUDIO_CHANNELS        2
#define AUDIO_BYTES_PER_FRAME (AUDIO_CHANNELS * sizeof(int16_t))

/* LESSON 11/12 — Chunk size for stream-fill loop.
 * 4096 frames ≈ 93 ms at 44100 Hz.  Matches Raylib's
 * SetAudioStreamBufferSizeDefault(4096).                                   */
#define AUDIO_CHUNK_SIZE      4096

/* ── AudioOutputBuffer ────────────────────────────────────────────────────
 * LESSON 09 — Flat int16_t array in interleaved stereo format:
 *   [L0, R0, L1, R1, L2, R2, ...]
 * Each pair (Li, Ri) is one "sample frame".
 * `sample_count` is the max frames the buffer can hold.
 * `max_sample_count` is a safety cap: never write more than this.         */
typedef struct {
  int16_t *samples;         /* Allocated by platform on startup.           */
  int      sample_count;    /* Frames allocated (capacity).                */
  int      max_sample_count;/* Safety cap; set = sample_count on init.     */
} AudioOutputBuffer;

/* LESSON 09 — Format-agnostic write helper.
 * Converts float L/R values in [-1.0, +1.0] to int16_t and writes the
 * interleaved pair at frame `frame_index`.
 *
 * Wrapping the conversion here means: if we ever switch to float32 PCM,
 * only this function changes — not every synthesis loop.                   */
static inline void audio_write_sample(AudioOutputBuffer *buf,
                                      int frame_index,
                                      float l, float r) {
  /* Clamp to [-1, 1] then scale. */
  if (l >  1.0f) l =  1.0f;
  if (l < -1.0f) l = -1.0f;
  if (r >  1.0f) r =  1.0f;
  if (r < -1.0f) r = -1.0f;
  buf->samples[frame_index * AUDIO_CHANNELS + 0] = (int16_t)(l * 32767.0f);
  buf->samples[frame_index * AUDIO_CHANNELS + 1] = (int16_t)(r * 32767.0f);
}

/* ── ToneGenerator ────────────────────────────────────────────────────────
 * LESSON 10 — One active SFX voice in the voice pool.
 *
 * `phase_acc`:        0.0–1.0 accumulator; advances by freq/sample_rate per
 *                     sample.  Wraps at 1.0 to stay in [0, 1).
 *
 * `frequency`:        Hz.  Set from SoundDef; may slide (freq_slide != 0).
 *
 * `freq_slide`:       Hz/second change applied each game-loop tick.
 *                     Enables simple pitch sweeps (laser, coin, etc.).
 *
 * `volume`:           0.0–1.0 amplitude scale.
 *
 * `pan`:              -1.0 (full left) to +1.0 (full right). 0 = centre.
 *
 * `samples_remaining`: frames left to render.  Decrements to 0, then voice
 *                     deactivates.  -1 = loop forever (no envelope).
 *
 * `total_samples`:    Set = samples_remaining when voice is triggered.
 *                     Used to compute the full-decay envelope:
 *                       env = samples_remaining / total_samples
 *                     0 = no envelope (used for infinite/-1 voices).
 *
 * `fade_in_samples`:  Ramp amplitude from 0→1 over first N samples.
 *                     88 samples ≈ 2 ms at 44100 Hz.  Prevents the abrupt
 *                     waveform jump at note-start from producing a click.
 *                     0 = instant start (no fade-in).
 *
 * `age`:              Monotonic counter.  Used by steal-oldest policy.     */
typedef struct {
  float phase_acc;
  float frequency;
  float freq_slide;       /* Hz per second, applied by game_audio_update.  */
  float volume;
  float pan;
  int   samples_remaining;
  int   total_samples;    /* Set on trigger; used for decay envelope.       */
  int   fade_in_samples;  /* 88 = 2ms attack; 0 = instant.                 */
  int   active;
  int   age;
} ToneGenerator;

/* ── SoundDef ─────────────────────────────────────────────────────────────
 * LESSON 10 — Static description of one sound effect.  Lives in a const
 * table (SOUND_DEFS[]) in game/audio_demo.c (or a game's audio.c).       */
typedef struct {
  float frequency;
  float freq_slide;     /* Hz per second (applied by game_audio_update).   */
  float volume;
  float pan;
  int   duration_ms;    /* Duration in milliseconds; -1 = loop forever.   */
} SoundDef;

/* ── Sound IDs ───────────────────────────────────────────────────────────
 * LESSON 10 — Enum indices into SOUND_DEFS[].
 * Game courses extend this enum with their own sounds.                    */
typedef enum {
  SOUND_TONE_LOW   = 0,  /* Low tone demo (220 Hz A3)                      */
  SOUND_TONE_MID   = 1,  /* Mid tone demo (440 Hz A4)                      */
  SOUND_TONE_HIGH  = 2,  /* High tone demo (880 Hz A5)                     */
  SOUND_COUNT
} SoundID;

/* ── Sound pool ───────────────────────────────────────────────────────────
 * LESSON 10 — Fixed-size voice pool.  When all slots are taken, the oldest
 * voice is stolen (lowest sound quality impact for short demo effects).   */
#define MAX_SOUNDS 8

/* ── MusicTone ───────────────────────────────────────────────────────────
 * LESSON 13 — Dedicated oscillator for background music.
 *
 * Unlike SFX voices (ToneGenerator), this is a SINGLE persistent tone that
 * the music sequencer drives note-by-note.  Key properties:
 *
 *   • Phase is PRESERVED across note boundaries (not reset each note).
 *     This prevents a waveform discontinuity at every note change, which
 *     is the primary cause of clicking in naive music implementations.
 *     Phase is only reset when coming from complete silence (frequency==0).
 *
 *   • Volume fades smoothly between notes using a smoothstep curve:
 *       t² × (3 − 2t)
 *     over MUSIC_FADE_SAMPLES frames.  This avoids the abrupt amplitude
 *     jump that occurs when instantly cutting one note and starting another.
 *
 *   • `current_volume` is the live, fade-adjusted working amplitude.
 *     The synth loop reads this, not `volume` directly.                    */
typedef struct {
  float phase;                  /* [0, 1) oscillator phase.                 */
  float frequency;              /* Hz. 0 = silence.                         */
  float volume;                 /* Target amplitude (0–1).                  */
  float current_volume;         /* Smoothstep-interpolated working volume.  */
  float pan;                    /* -1 = left, 0 = centre, +1 = right.       */
  int   is_playing;             /* 1 = tone active; 0 = fading to silence.  */
  int   fade_samples_remaining; /* Countdown for smoothstep transition.     */
} MusicTone;

/* Smoothstep fade length: 441 samples ≈ 10 ms at 44100 Hz.
 * Long enough to be click-free; short enough to not blur note attacks.    */
#define MUSIC_FADE_SAMPLES 441

/* ── MusicSequencer ──────────────────────────────────────────────────────
 * LESSON 13 — MIDI-note step sequencer for background music.
 *
 * Advances INSIDE game_get_audio_samples (per-sample, not per game frame).
 * This is "sample-accurate" sequencing: note boundaries happen exactly on
 * the correct sample, not up to 16.7 ms late as frame-based sequencing
 * would produce.
 *
 * Patterns are arrays of MIDI note numbers (uint8_t).
 *   0  = REST (silence for that step).
 *   60 = C4 (261.63 Hz), 69 = A4 (440 Hz), 72 = C5 (523.25 Hz), etc.
 * Use audio_midi_to_freq() from utils/audio-helpers.h to convert.
 *
 * JS analogy: a Web Audio Tone.Transport scheduler, but driven by a simple
 * sample counter instead of a Web Audio clock.                             */
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
 * LESSON 09 — Top-level audio state shared between game loop and audio loop.
 *
 * `voices`             — SFX voice pool (ToneGenerator, one per active effect).
 * `sequencer`          — Background music sequencer with dedicated MusicTone.
 * `samples_per_second` — Set at init from AUDIO_SAMPLE_RATE.  Stored here so
 *                        sample-accurate timing calculations don't depend on
 *                        a #define (game courses may use a different rate).
 * `master_volume`      — Global output scale.  Reduce to prevent clipping
 *                        when many voices play simultaneously.
 * `sfx_volume`         — Per-category volume for sound effects (0–1).
 * `music_volume`       — Per-category volume for background music (0–1).
 *
 * GAME COURSES: embed this struct inside your own GameState, then pass a
 * pointer to game_get_audio_samples and game_audio_update.               */
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
 * LESSON 11/12 — Platform-specific audio context passed through the
 * platform_audio_* functions declared in platform.h.
 *
 * Backends populate this struct in platform_audio_init().                 */
typedef struct {
  int sample_rate;    /* Always AUDIO_SAMPLE_RATE (44100).                  */
  int channels;       /* Always AUDIO_CHANNELS (2).                         */
  int chunk_size;     /* Platform ring-buffer chunk size (AUDIO_CHUNK_SIZE).*/

  /* ---- Raylib backend (lesson 11) ---- */
  /* Fields used by platforms/raylib/main.c (opaque to other backends).    */
  void *raylib_stream;    /* AudioStream* allocated by raylib backend.      */

  /* ---- X11/ALSA backend (lesson 12) ---- */
  /* Fields used by platforms/x11/audio.c (opaque to other backends).      */
  void *alsa_pcm;         /* snd_pcm_t* allocated by x11 audio backend.    */
  int   alsa_buffer_size; /* hw_buffer_size returned by snd_pcm_hw_params.  */
} PlatformAudioConfig;

#endif /* UTILS_AUDIO_H */
