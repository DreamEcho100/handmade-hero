/* ═══════════════════════════════════════════════════════════════════════════
 * utils/audio.h  —  Phase 2 Course Version
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * COURSE OVERVIEW
 * ───────────────
 * This header defines every audio type the game layer uses.  The platform
 * (e.g. main_x11.c, main_raylib.c) fills an AudioOutputBuffer every frame;
 * the game writes raw PCM samples into it through game_get_audio_samples().
 *
 * Key concepts introduced here (each annotated at first occurrence):
 *   • AudioOutputBuffer  — the bridge between platform and game
 *   • SoundInstance      — phase-accumulator oscillator
 *   • midi_to_freq()     — equal-temperament frequency formula
 *   • clamp_sample()     — int16 range clamping
 *   • calculate_pan_volumes() — linear stereo panning
 *
 * JS background note: if you have used the Web Audio API, think of this
 * file as implementing a tiny software synth that replaces the browser's
 * AudioContext entirely — no callbacks, no event loop, just math.
 */

#ifndef UTILS_AUDIO_H   /* JS: like `if (!window.__audioH)` duplicate-guard  */
#define UTILS_AUDIO_H   /* C:  include-guard — prevents double-inclusion     */

#include <math.h>      /* powf(), sinf() — math functions from the C stdlib */
#include <stdint.h>    /* int16_t, int64_t, uint8_t — fixed-width integers  */

/* ══════ AudioOutputBuffer ══════════════════════════════════════════════════
 *
 * WHO FILLS WHAT
 * ──────────────
 *  Platform fills:   samples, samples_per_second, sample_count
 *  Game writes into: samples  (game_get_audio_samples writes PCM data here)
 *
 * The platform allocates the `samples` memory and tells the game how large
 * it is (sample_count) and at what rate it should run (samples_per_second).
 * The game then fills the buffer with audio data before the platform sends
 * it to the sound card.
 *
 * MEMORY LAYOUT — Interleaved Stereo
 * ────────────────────────────────────
 *   Index:  0      1      2      3      4      5  ...
 *   Value:  L[0]   R[0]   L[1]   R[1]   L[2]   R[2] ...
 *
 *   So to write sample pair N:
 *     samples[N * 2 + 0] = left_sample;
 *     samples[N * 2 + 1] = right_sample;
 *
 * JS equivalent: like a Float32Array passed to
 *   AudioContext.createScriptProcessor().onaudioprocess
 * but as raw int16 (signed 16-bit integer) instead of float32.
 *
 * JS: AudioContext buffer → C: AudioOutputBuffer
 */
typedef struct {                       /* JS: like a plain object / class  */
  int16_t *samples;       /* Pointer to buffer; platform allocates, game writes.
                             int16_t = signed 16-bit integer (–32768..32767).
                             JS: Float32Array, but 16-bit integer format.     */
  int samples_per_second; /* Sample rate — usually 44100 or 48000 Hz.
                             44100 Hz = 44,100 amplitude snapshots per second.
                             CD quality. The higher, the better treble.       */
  int sample_count;       /* How many STEREO PAIRS the game must generate.
                             Buffer byte size = sample_count * 2 * sizeof(int16_t)
                             (× 2 for left + right channels, × 2 for int16).  */
} AudioOutputBuffer;

/* ══════ Sound Effect Identifiers ══════════════════════════════════════════
 *
 * JS: like a TypeScript `const enum` or string-union type.
 * C:  typedef enum — gives each constant a unique integer value starting at 0.
 *
 * WHY ENUMS INSTEAD OF RAW INTEGERS?
 *   Bad:  game_play_sound(audio, 3);   // What is 3?
 *   Good: game_play_sound(audio, SOUND_DROP); // Self-documenting!
 *   The compiler also catches typos and wrong-category bugs.
 *
 * JS: type SoundID = 'none' | 'move' | 'rotate' | ... (string union)
 * C:  typedef enum { SOUND_NONE = 0, SOUND_MOVE, ... } SOUND_ID;
 */
typedef enum {
  SOUND_NONE = 0,       /* Explicit 0 — also "no sound / sentinel"          */
  SOUND_MOVE,           /* Piece moved left or right                         */
  SOUND_ROTATE,         /* Piece was rotated                                 */
  SOUND_DROP,           /* Piece locked into the field                       */
  SOUND_LINE_CLEAR,     /* 1–3 lines completed and cleared                   */
  SOUND_TETRIS,         /* All 4 lines cleared at once (Tetris!)             */
  SOUND_LEVEL_UP,       /* Player crossed a level threshold                  */
  SOUND_GAME_OVER,      /* The new piece can't fit — game ends               */
  SOUND_RESTART,        /* Player pressed R to restart                       */
  SOUND_COUNT           /* Trick: last enum value = total count (no magic #) */
} SOUND_ID;

/* ══════ Sound Instance ══════════════════════════════════════════════════════
 *
 * Represents ONE currently-playing sound effect (procedural synthesis).
 * Up to MAX_SIMULTANEOUS_SOUNDS can be active at once.
 *
 * THE PHASE ACCUMULATOR PATTERN
 * ──────────────────────────────
 * `phase` is a value that crawls from 0.0 to 1.0 and wraps back:
 *
 *   phase += frequency * (1.0 / sample_rate);   // advance by one sample
 *   if (phase >= 1.0) phase -= 1.0;             // wrap around
 *
 * One full cycle of phase (0→1) = one full cycle of the waveform.
 *
 * For a 440 Hz tone at 44100 Hz sample rate:
 *   phase advance per sample = 440 / 44100 ≈ 0.00997
 *   samples per full cycle   = 44100 / 440 ≈ 100.2 samples
 *
 * To generate a square wave:
 *   wave = (phase < 0.5f) ? 1.0f : -1.0f;
 *
 * JS equivalent: Web Audio API OscillatorNode handles this internally.
 *   Here we implement the same math manually so we have full control.
 *
 * ⚠ CRITICAL: Never reset phase between buffer fills — causes click
 * ──────────────────────────────────────────────────────────────────
 * `phase` must carry over from one buffer fill to the next, or there will
 * be a discontinuity in the waveform → audible "click" / "pop".
 * Only reset phase when a new sound starts (inst->phase = 0.0f).
 */
#define MAX_SIMULTANEOUS_SOUNDS 4   /* Polyphony limit — tune as needed      */

typedef struct {
  SOUND_ID sound_id;      /* Which sound preset is currently playing          */

  float phase;            /* Phase accumulator: 0.0 to <1.0 (wraps).
                             ⚠ CRITICAL: Never reset between buffer fills —
                             causes click.  Only reset on new sound start.    */

  int samples_remaining;  /* Countdown: how many more sample-pairs to write.
                             When this reaches 0 the slot is free again.      */

  float volume;           /* Amplitude scalar: 0.0 = silent, 1.0 = full.
                             Applied before master_volume.                    */

  float frequency;        /* Current pitch in Hz.  Slides each sample if
                             frequency_slide != 0.                            */

  float frequency_slide;  /* Hz to add to frequency per sample.
                             > 0 = pitch rises, < 0 = pitch falls.
                             Produces the "swoosh" effect on DROP, GAME_OVER. */

  int total_samples;      /* Total duration in samples (set once at start).
                             Used to compute the fade-out envelope ratio.     */

  int fade_in_samples;    /* How many samples to ramp volume up from 0.
                             Prevents the "click" at attack (≈96 samples
                             = ~2 ms at 44100 Hz).                            */

  float pan_position;     /* Stereo position: −1.0 = full left,
                                               0.0 = centre,
                                               1.0 = full right.
                             Pieces near left wall play left, near right wall
                             play right — gives spatial feel.                 */
} SoundInstance;

/* ══════ Tone Generator (Background Music) ══════════════════════════════════
 *
 * A single continuous oscillator used by the music sequencer.
 * `current_volume` is smoothed toward `volume` each sample to prevent clicks
 * when notes start/stop.  This is the "volume ramp" technique.
 */
typedef struct {
  float phase;           /* Same phase-accumulator as SoundInstance (see above) */
  float frequency;       /* Current note frequency in Hz                        */
  float volume;          /* Target volume (set by sequencer on note start/stop)  */
  float pan_position;    /* Stereo pan: −1.0 left … 0.0 centre … 1.0 right      */
  float current_volume;  /* Smoothed volume — interpolates toward `volume` each
                            sample to avoid click on note transitions.
                            Think of it like CSS `transition: volume 2ms`.       */
  int is_playing;        /* 1 = a note is scheduled; 0 = rest / silence          */
} ToneGenerator;

/* ══════ Music Sequencer ══════════════════════════════════════════════════════
 *
 * A simple step sequencer: cycles through 4 patterns of 16 steps each.
 * Each step is a MIDI note number (or 0 for rest).
 * step_duration controls tempo (e.g., 0.15s/step ≈ 100 BPM at 16th notes).
 *
 * Pattern layout:
 *   patterns[0] = Tetris Theme A  (main melody)
 *   patterns[1] = Tetris Theme B
 *   patterns[2] = Tetris Theme C
 *   patterns[3] = Tetris Theme D
 *
 * MIDI note numbers (JS: same standard as Web MIDI API):
 *   60 = Middle C (C4), 69 = A4 (440 Hz), 76 = E5
 *   Each semitone step up = +1; each octave up = +12.
 */
#define MUSIC_PATTERN_LENGTH 16   /* Steps per pattern (one bar of 16ths)   */
#define MUSIC_NUM_PATTERNS   4    /* How many patterns in the sequence       */

typedef struct {
  uint8_t patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
                            /* 2D array: [pattern_index][step_index]
                               JS: number[][]
                               Values: MIDI note (0 = rest, 60 = C4, etc.)  */

  int current_pattern;      /* Which of the 4 patterns is active [0..3]     */
  int current_step;         /* Which of the 16 steps we're on [0..15]       */
  float step_timer;         /* Seconds elapsed in current step               */
  float step_duration;      /* Seconds per step (controls tempo)             */
  ToneGenerator tone;       /* The one oscillator that plays the melody      */
  int is_playing;           /* 0 = music stopped, 1 = music running          */
} MusicSequencer;

/* ══════ Game Audio State ════════════════════════════════════════════════════
 *
 * Embedded directly in GameState (no hidden globals, no singletons).
 * The platform calls game_get_audio_samples() with a pointer to GameState,
 * which then accesses &state->audio.
 *
 * Handmade Hero principle: All state is visible.
 *   No hidden static variables inside functions — everything lives here.
 */
typedef struct {
  /* ── Sound Effects ── */
  SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
                            /* Ring-buffer of playing SFX.  Slots are reused
                               when samples_remaining reaches 0.              */
  int active_sound_count;   /* How many slots are currently in use.           */

  /* ── Music ── */
  MusicSequencer music;     /* The background music sequencer (one instance)  */

  /* ── Volume Controls ── */
  float master_volume;      /* Overall output scale [0.0 .. 1.0].
                               Applied last, after mixing all sources.        */
  float sfx_volume;         /* Sound-effects sub-mix volume [0.0 .. 1.0]     */
  float music_volume;       /* Background music sub-mix volume [0.0 .. 1.0]  */

  /* ── Sample Rate (set by platform) ── */
  int samples_per_second;   /* Set once during game_audio_init().
                               Used to convert durations (ms) into sample
                               counts.  Never changes at runtime.             */
} GameAudioState;

/* ══════ Audio Helper Functions ═════════════════════════════════════════════
 *
 * `static inline` — JS: equivalent to a small inlined utility function.
 *   `static` = only visible in this translation unit (not exported globally).
 *   `inline`  = hint to compiler to paste the body at call sites (no call overhead).
 *   Both together: a private utility function that compiles away entirely.
 */

/* ── midi_to_freq ────────────────────────────────────────────────────────────
 * Convert a MIDI note number to a frequency in Hz.
 *
 * FORMULA:  freq = 440 × 2^((note − 69) / 12)
 *
 * WHY 69?
 *   MIDI note 69 is A4 — the standard tuning reference pitch (440 Hz).
 *   Subtracting 69 makes A4 the "zero point" of the exponent.
 *
 * WHY DIVIDE BY 12?
 *   There are 12 semitones per octave in equal temperament.
 *   Each octave doubles the frequency, so one semitone = 2^(1/12) ≈ 1.059.
 *   Dividing by 12 distributes that doubling evenly across all 12 steps.
 *
 * EXAMPLES:
 *   midi_to_freq(69) = 440 × 2^0     = 440.0 Hz  (A4)
 *   midi_to_freq(81) = 440 × 2^1     = 880.0 Hz  (A5, one octave up)
 *   midi_to_freq(57) = 440 × 2^(−1)  = 220.0 Hz  (A3, one octave down)
 *   midi_to_freq(60) = 440 × 2^(−9/12) ≈ 261.6 Hz (C4 / Middle C)
 *
 * JS equivalent: same formula works directly in JS/TS:
 *   const midiToFreq = (note: number) => 440 * Math.pow(2, (note - 69) / 12);
 */
static inline float midi_to_freq(int note) {
  /* powf = floating-point power function (float version of Math.pow) */
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

/* ── clamp_sample ────────────────────────────────────────────────────────────
 * Clamp a float sample to the valid int16_t range before writing to buffer.
 *
 * WHY CLAMP?
 *   int16_t stores values from −32768 to 32767 (2^15 range).
 *   When multiple sounds mix together, their amplitudes add.
 *   The sum can exceed ±32767.  Writing an out-of-range value would wrap
 *   around and produce a harsh digital distortion ("clipping").
 *   Clamping is gentler — it saturates instead of wrapping.
 *
 * RANGE:
 *   int16_t min = −32768  (−2^15)
 *   int16_t max =  32767  ( 2^15 − 1)
 *
 * JS equivalent:
 *   const clampSample = (s: number) => Math.max(-32768, Math.min(32767, s));
 */
static inline int16_t clamp_sample(float sample) {
  if (sample >  32767.0f) return  32767;  /* Saturate positive overflow  */
  if (sample < -32768.0f) return -32768;  /* Saturate negative overflow  */
  return (int16_t)sample;                 /* Safe cast: value fits int16 */
}

/* ── calculate_pan_volumes ───────────────────────────────────────────────────
 * Compute per-channel gain factors from a pan position.
 *
 * LINEAR PANNING MATH
 * ────────────────────
 * We use a simple linear (not constant-power) pan law:
 *
 *   pan < 0  → full left,   right = 1 + pan   (reduces right)
 *   pan = 0  → left = 1,   right = 1           (centre — equal)
 *   pan > 0  → left = 1 − pan,  full right
 *
 * Example  pan = −0.5:   left_vol = 1.0,   right_vol = 0.5
 * Example  pan =  0.0:   left_vol = 1.0,   right_vol = 1.0
 * Example  pan = +0.5:   left_vol = 0.5,   right_vol = 1.0
 *
 * NOTE: Constant-power panning (sin/cos law) maintains perceived loudness
 * at all pan positions.  Linear panning is simpler and sounds fine for SFX.
 *
 * OUTPUT: *left_vol and *right_vol are set; multiply each channel sample by
 * these values before writing to the interleaved buffer.
 *
 * JS equivalent: Web Audio API StereoPannerNode does this automatically.
 *   Here we implement it manually for full transparency.
 *
 * @param pan       Input pan: −1.0 = full left, 0.0 = centre, 1.0 = full right
 * @param left_vol  Output: gain for left channel  [0.0 .. 1.0]
 * @param right_vol Output: gain for right channel [0.0 .. 1.0]
 */
static inline void calculate_pan_volumes(float pan, float *left_vol,
                                         float *right_vol) {
  if (pan <= 0.0f) {
    *left_vol  = 1.0f;       /* Left channel always full when panned left/centre */
    *right_vol = 1.0f + pan; /* pan is ≤ 0, so this reduces right (e.g. −0.5 → 0.5) */
  } else {
    *left_vol  = 1.0f - pan; /* Reduces left when panned right                    */
    *right_vol = 1.0f;       /* Right channel always full when panned right        */
  }
}

/* ── FIELD_CENTER ────────────────────────────────────────────────────────────
 * The horizontal centre of the playable field (excluding wall columns 0 & 11).
 * Used by calculate_piece_pan() in game.c to map piece X position to stereo pan.
 *
 * Playable columns: 1 … (FIELD_WIDTH − 2)
 *   Number of playable columns = FIELD_WIDTH − 2
 *   Centre index               = (FIELD_WIDTH − 2) / 2.0 + 1.0
 *
 * Example: FIELD_WIDTH = 12
 *   Playable columns: 1 … 10  (10 columns)
 *   Centre: (12 − 2) / 2.0 + 1.0 = 5.0 + 1.0 = 6.0
 */
#define FIELD_CENTER  ((FIELD_WIDTH - 2) / 2.0f + 1.0f)

#endif /* UTILS_AUDIO_H */
