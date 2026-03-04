/* ═══════════════════════════════════════════════════════════════════════════
 * audio.c  —  Phase 2 Course Version
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * This is the game's entire audio engine — no .wav files, no third-party
 * library, no OS calls.  Everything here is pure math applied to a raw
 * buffer of int16 samples that the platform sends to the sound card.
 *
 * It implements three things:
 *   1. Procedural sound effects  — synthesised on the fly from frequency /
 *      duration / volume parameters stored in SOUND_DEFS[].
 *   2. A step sequencer          — cycles through four 16-step MIDI patterns
 *      to play the Tetris theme melody.
 *   3. A sample generator        — game_get_audio_samples() mixes all active
 *      sounds and music into the platform-provided AudioOutputBuffer.
 *
 * WHY NO .WAV FILES?
 * ──────────────────
 * Loading assets requires OS calls and file I/O — things the game layer must
 * not do on its own (Handmade Hero principle: the platform mediates I/O).
 * Procedural synthesis keeps audio self-contained inside this .c file:
 * any C compiler can build it on any platform, with zero external assets.
 * It's also a great way to see how sound actually works at the bit level.
 *
 * JS analogy: if you've used the Web Audio API, this file is roughly
 * equivalent to creating an AudioContext, several OscillatorNodes, a
 * GainNode, a StereoPannerNode, and a ScriptProcessorNode — except instead
 * of the browser managing all of that for you, we write every sample
 * ourselves in a tight loop.
 *
 * READING ORDER
 * ─────────────
 * 1. SOUND_DEFS[]           — the "presets" table
 * 2. TETRIS_PATTERN_*       — the MIDI note arrays
 * 3. game_audio_init()      — first-time setup
 * 4. game_play_sound_at()   — triggers a sound effect
 * 5. game_music_play/stop() — music on/off
 * 6. game_audio_update()    — advances the sequencer each frame
 * 7. game_get_audio_samples()— fills the PCM buffer (the hot path)
 */

/* JS: `import` statement | C: `#include` — pastes the header file in-place */
#include "game.h"   /* GameState, GameAudioState, AudioOutputBuffer, SOUND_ID */
#include <math.h>   /* powf() — we only need it indirectly via game.h, but    */
                    /* good practice to be explicit about what we use.         */
#include <stdio.h>  /* (not used at runtime; kept for potential debug printf) */
#include <string.h> /* memset(), memcpy() — standard memory utilities          */

/* JS: `if (!Math.PI)` guard | C: guard for M_PI which is POSIX, not C99 std */
#ifndef M_PI
#define M_PI 3.14159265358979323846  /* Ratio of circle circumference to diameter */
#endif


/* ══════ Sound Effect Definitions ══════════════════════════════════════════ */
/*
 * WHY A TABLE INSTEAD OF HARD-CODED VALUES?
 * ──────────────────────────────────────────
 * By storing every sound's parameters in one place, we can tweak any sound
 * by editing a single row — the playback code never changes.
 * This is the "data-driven" design pattern.
 *
 * JS equivalent:
 *   const SOUND_DEFS = {
 *     [SoundID.MOVE]: { frequency: 200, frequencyEnd: 150, durationMs: 50,  volume: 0.3 },
 *     ...
 *   };
 *
 * C:
 *   static const SoundDef SOUND_DEFS[SOUND_COUNT] = { ... };
 *
 * WHY `static const`?
 *   `const`   — these values must never change at runtime (read-only table).
 *   `static`  — the symbol is only visible in this .c file (like a module-
 *               private variable).  Prevents name collisions with other .c files.
 */

/* JS: `interface SoundDef { ... }` | C: `typedef struct` — named record type */
typedef struct {
  float frequency;      /* Starting pitch in Hz.  Controls the perceived tone.
                           Low Hz = low pitch (bass), high Hz = high pitch (treble).
                           Human hearing range ≈ 20 Hz – 20,000 Hz.              */

  float frequency_end;  /* Ending pitch in Hz.  If different from frequency, the
                           pitch slides from frequency → frequency_end over the
                           sound's duration.  This "sweep" gives each sound a
                           distinctive feel (e.g., SOUND_DROP swoops downward).  */

  float duration_ms;    /* How long the sound plays, in milliseconds.
                           Converted to sample count in game_play_sound_at().    */

  float volume;         /* Amplitude scalar: 0.0 = silent, 1.0 = full loudness.
                           This scales the raw wave before the master volume.    */
} SoundDef;

/*
 * SOUND_DEFS — the complete "preset" table for all sound effects.
 *
 * Indexed by SOUND_ID enum values (from utils/audio.h).
 * C99 "designated initializer" syntax: [SOUND_MOVE] = {...}
 * means "put this entry at the SOUND_MOVE index".
 * This is safer than positional initialisation — if the enum order changes,
 * the correct entry still ends up at the right index.
 *
 * JS equivalent:
 *   const SOUND_DEFS: Record<SoundID, SoundDef> = {
 *     [SoundID.NONE]:       { frequency:   0, frequencyEnd:    0, durationMs:   0, volume: 0.0 },
 *     [SoundID.MOVE]:       { frequency: 200, frequencyEnd:  150, durationMs:  50, volume: 0.3 },
 *     ...
 *   };
 *
 * WHY PROCEDURAL SOUNDS?
 * ───────────────────────
 * Instead of loading "move.wav" from disk, we generate the waveform in real
 * time from these four numbers.  Benefits:
 *   • Zero asset files — easier to share, no file loading code needed.
 *   • Instant variation — changing a number changes the sound immediately.
 *   • Educationally transparent — you can hear and see exactly how each
 *     parameter (frequency, duration, volume) shapes the sound.
 *
 * SOUND DESIGN NOTES (why each sound has the values it does):
 *   SOUND_MOVE       200→150 Hz, 50 ms   — short, low-pitch "tick" → quiet, unobtrusive
 *   SOUND_ROTATE     300→400 Hz, 80 ms   — rising chirp → feels like a spin-up
 *   SOUND_DROP       150→50 Hz, 100 ms   — falling sweep → piece thuds down
 *   SOUND_LINE_CLEAR 400→800 Hz, 200 ms  — rising sweep → rewarding "zing"
 *   SOUND_TETRIS     300→1200 Hz, 400 ms — dramatic rise → biggest reward
 *   SOUND_LEVEL_UP   440→880 Hz, 300 ms  — perfect octave jump → musical fanfare
 *   SOUND_GAME_OVER  400→100 Hz, 500 ms  — long descending sweep → game over gloom
 *   SOUND_RESTART    600→1200 Hz, 200 ms — quick bright rise → fresh start
 */
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    [SOUND_NONE]       = {0,   0,    0,   0.0f},
    [SOUND_MOVE]       = {200, 150,  50,  0.3f},
    [SOUND_ROTATE]     = {300, 400,  80,  0.3f},
    [SOUND_DROP]       = {150, 50,   100, 0.5f},
    [SOUND_LINE_CLEAR] = {400, 800,  200, 0.6f},
    [SOUND_TETRIS]     = {300, 1200, 400, 0.8f},
    [SOUND_LEVEL_UP]   = {440, 880,  300, 0.5f},
    [SOUND_GAME_OVER]  = {400, 100,  500, 0.7f},
    [SOUND_RESTART]    = {600, 1200, 200, 0.5f},
};


/* ══════ Tetris Theme Patterns ══════════════════════════════════════════════ */
/*
 * WHAT ARE THESE ARRAYS?
 * ───────────────────────
 * Each array is one "bar" (measure) of the Tetris theme, encoded as MIDI
 * note numbers.  The music sequencer in game_audio_update() reads one entry
 * per time step and tells the ToneGenerator which pitch to play.
 *
 * JS: const TETRIS_PATTERN_A: number[] = [76, 71, 72, ...];
 * C:  static const uint8_t TETRIS_PATTERN_A[MUSIC_PATTERN_LENGTH] = {...};
 *
 * JS: `number[]` | C: `uint8_t[]` — array of unsigned 8-bit integers.
 *   uint8_t stores values 0..255, which covers the whole MIDI range (0..127)
 *   with room to spare, and uses only 1 byte per note → compact.
 *
 * MIDI NOTE NUMBERS — what the numbers mean:
 * ───────────────────────────────────────────
 * MIDI uses integers to represent pitches:
 *   0  = C-1  (very low, below bass guitar)
 *   60 = C4   (Middle C — the C in the middle of a piano)
 *   69 = A4   (concert A — 440 Hz, orchestra tuning reference)
 *   76 = E5   (the opening note of Tetris Theme A)
 *   127= G9   (very high, above standard piano range)
 *
 * Each semitone (adjacent piano key) = +1.
 * One octave up = +12.
 *
 * So 76 (E5) and 64 (E4) are the same note one octave apart.
 *
 * midi_to_freq() in utils/audio.h converts these integers to Hz:
 *   midi_to_freq(69) = 440.0 Hz  (A4)
 *   midi_to_freq(76) = 659.3 Hz  (E5)
 *
 * WHY 0 = REST?
 * ─────────────
 * MIDI note 0 is a legal note (C-1, barely audible), but we repurpose it as
 * a sentinel meaning "silence / rest" because the Tetris melody never uses
 * notes that low.  In game_audio_update(), if the current step's note is 0,
 * we set tone.is_playing = 0 (mute the tone) rather than setting a frequency.
 *
 * JS equivalent: like using `null` in an array to mean "no note here".
 *
 * PATTERNS A–D — the four sections of the Tetris melody:
 *   Pattern A: the iconic opening phrase (76 71 72 74 72 71 69 69 72 76 ...)
 *   Pattern B: a sparser middle section (rests on beats 2 & 4 via 0s)
 *   Pattern C: a variant ascending/descending line
 *   Pattern D: a closing cadence with many rests
 *
 * The sequencer (MusicSequencer in utils/audio.h) cycles through A → B → C → D
 * → A → B → ... for continuous looping.
 */

/* JS: const | C: static const — file-private, read-only, compile-time constant */
static const uint8_t TETRIS_PATTERN_A[MUSIC_PATTERN_LENGTH] = {
    /* E5  B4  C5  D5  C5  B4  A4  A4  C5  E5  D5  C5  B4  B4  C5  D5 */
    76, 71, 72, 74, 72, 71, 69, 69, 72, 76, 74, 72, 71, 71, 72, 74,
};

static const uint8_t TETRIS_PATTERN_B[MUSIC_PATTERN_LENGTH] = {
    /* E5  --  C5  --  A4  A4  C5  D5  E5  --  C5  --  A4  --  --  -- */
    /* (0 = rest; the rests give this section a more syncopated feel)   */
    76, 0, 72, 0, 69, 69, 72, 74, 76, 0, 72, 0, 69, 0, 0, 0,
};

static const uint8_t TETRIS_PATTERN_C[MUSIC_PATTERN_LENGTH] = {
    /* D5  --  D5  E5  F#5 --  E5  D5  C5  --  C5  D5  E5  --  D5  C5 */
    74, 0, 74, 76, 78, 0, 76, 74, 72, 0, 72, 74, 76, 0, 74, 72,
};

static const uint8_t TETRIS_PATTERN_D[MUSIC_PATTERN_LENGTH] = {
    /* B4  --  --  C5  D5  --  C5  B4  A4  --  --  --  --  --  --  -- */
    /* (long rest at end lets Pattern A's opening feel fresh again)     */
    71, 0, 0, 72, 74, 0, 72, 71, 69, 0, 0, 0, 0, 0, 0, 0,
};


/* ══════ Initialization ══════════════════════════════════════════════════════ */
/*
 * game_audio_init — set up the audio state before any sound can play.
 *
 * Called once at startup from game_init() (game.c).
 * Everything that needs a known starting value is set here — no magic
 * defaults hidden inside the struct definition.
 *
 * JS equivalent:
 *   class AudioEngine {
 *     constructor(samplesPerSecond: number) {
 *       this.samplesPerSecond = samplesPerSecond;
 *       this.masterVolume = 0.8;
 *       ...
 *     }
 *   }
 *
 * @param audio            Pointer to the GameAudioState embedded in GameState.
 * @param samples_per_second  Sample rate provided by the platform (e.g., 48000).
 */
void game_audio_init(GameAudioState *audio, int samples_per_second) {
  /* JS: `Object.assign(this, {})` zeroed | C: zero the whole struct at once.
   * memset fills every byte with 0 — all floats become 0.0f, all ints become 0,
   * all pointers become NULL.  This is a cheap and safe "reset all fields".    */
  memset(audio, 0, sizeof(GameAudioState));

  /* Store the sample rate so duration-to-samples conversion works everywhere. */
  audio->samples_per_second = samples_per_second;

  /* Volume controls: master at 80 % so there's headroom; music quieter than
   * SFX so gameplay feedback cuts through the background melody.              */
  audio->master_volume = 0.8f;
  audio->sfx_volume    = 1.0f;
  audio->music_volume  = 0.5f;

  /* Step duration: how many seconds each sequencer step lasts.
   * 0.15 s/step × 16 steps/bar ≈ 2.4 s/bar ≈ 100 BPM at 16th notes.
   * Adjust this single value to speed up or slow down the entire melody.     */
  audio->music.step_duration = 0.15f;

  /* Music tone: target volume 0.3 (quiet background), start current_volume at
   * 0.0 so the first note fades in smoothly rather than starting full-blast.  */
  audio->music.tone.volume         = 0.3f;
  audio->music.tone.current_volume = 0.0f;
  audio->music.tone.pan_position   = 0.0f; /* Music centred in the stereo field */

  /* Copy the static pattern arrays into the sequencer's working buffer.
   * JS: [...TETRIS_PATTERN_A] | C: memcpy — byte-by-byte copy of the array.
   * We copy instead of pointing directly at the static arrays so the sequencer
   * could theoretically modify patterns at runtime (e.g., for a higher-speed
   * variant) without touching the original constants.                          */
  memcpy(audio->music.patterns[0], TETRIS_PATTERN_A, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[1], TETRIS_PATTERN_B, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[2], TETRIS_PATTERN_C, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[3], TETRIS_PATTERN_D, MUSIC_PATTERN_LENGTH);
}


/* ══════ Play Sound Effect ══════════════════════════════════════════════════ */
/*
 * game_play_sound_at — trigger a sound effect at a specific stereo position.
 *
 * HOW IT WORKS — SLOT ALLOCATION
 * ────────────────────────────────
 * The audio state has a fixed-size array active_sounds[MAX_SIMULTANEOUS_SOUNDS].
 * Each slot is a SoundInstance struct.  A slot is "free" when its
 * samples_remaining <= 0 (it has finished playing or was never used).
 *
 * We search for a free slot:
 *   • Found one  → use it.  This is the happy path.
 *   • None free  → steal slot 0 (the oldest / first slot).  This prevents
 *                  silence: it is better to cut off the oldest sound than
 *                  to silently drop the new one.
 *
 * JS: in Web Audio you'd call oscillator.start() and the browser manages
 * voices.  Here we manage the "polyphony" ourselves.
 *
 * WHY inst->phase = 0.0f HERE (not in the sample loop)?
 * ───────────────────────────────────────────────────────
 * The phase is reset to 0 only once, at the exact moment a new sound starts.
 * Inside game_get_audio_samples() the phase is NEVER reset — it ticks
 * continuously sample by sample.  Resetting phase mid-stream would create
 * a discontinuity → audible click.  Resetting it here is safe because this
 * function is called from the game update thread, before the next audio fill.
 *
 * @param audio         Pointer to the game's audio state.
 * @param sound         Which sound to play (SOUND_MOVE, SOUND_DROP, etc.).
 * @param pan_position  Stereo pan: −1.0 = hard left, 0.0 = centre, 1.0 = hard right.
 */
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound,
                        float pan_position) {
  /* Guard: SOUND_NONE is a sentinel (silence), not a real sound.
   * Out-of-range IDs would index outside SOUND_DEFS[] → undefined behaviour. */
  if (sound == SOUND_NONE || sound >= SOUND_COUNT)
    return;

  /* ── Find a free slot ── */
  /* JS: let slot = activeSounds.findIndex(s => s.samplesRemaining <= 0);     */
  int slot = -1;                            /* -1 = "not found yet"           */
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    if (audio->active_sounds[i].samples_remaining <= 0) {
      /* This slot is idle — its previous sound has finished (or it was never
       * used because memset zeroed samples_remaining in game_audio_init).    */
      slot = i;
      break; /* Stop searching; we only need one free slot.                   */
    }
  }

  /* ── Fallback: steal the oldest slot when all are busy ── */
  /* When all MAX_SIMULTANEOUS_SOUNDS voices are active (full polyphony),
   * slot is still -1.  Rather than dropping the new sound entirely, we evict
   * slot 0.  Slot 0 was filled first and is therefore the "oldest" sound —
   * a reasonable heuristic for which sound matters least now.                */
  if (slot < 0) {
    slot = 0; /* Steal slot 0 — oldest sound gets cut off */
  }

  /* ── Initialise the chosen slot ── */
  const SoundDef    *def  = &SOUND_DEFS[sound];          /* Read-only preset  */
  SoundInstance     *inst = &audio->active_sounds[slot]; /* Slot to write to  */

  inst->sound_id  = sound;
  inst->phase     = 0.0f;   /* ← CRITICAL: reset phase to the start of a wave
                               cycle so the new sound begins from a known state.
                               This is ONLY done here, never inside the audio
                               sample loop.  See ⚠ note in game_get_audio_samples. */

  inst->frequency = def->frequency;  /* Start pitch from the preset            */
  inst->volume    = def->volume;     /* Amplitude from the preset              */

  /* Convert millisecond duration to a sample count.
   * formula: samples = (ms / 1000) × samples_per_second
   * JS: Math.round(durationMs / 1000 * samplesPerSecond)
   * The (int) cast truncates the float — a fraction of a sample is negligible. */
  inst->samples_remaining =
      (int)(def->duration_ms * audio->samples_per_second / 1000.0f);
  inst->total_samples = inst->samples_remaining; /* Save the original duration
                                                    for the fade-out envelope.  */

  /* Fade-in window: ~2 ms at 48 kHz.
   * WHY 96 SAMPLES?
   *   96 samples / 48000 Hz = 0.002 s = 2 ms.
   *   A hard start (phase = 0 → immediate full amplitude) causes a "click"
   *   (DC offset discontinuity).  Ramping volume from 0 → target over 2 ms
   *   smooths the attack and removes the click without perceptible softening
   *   of the sound's onset.
   * The fade-in envelope is applied in game_get_audio_samples().             */
  inst->fade_in_samples = 96;

  /* Clamp pan to the valid range [−1, +1].  Input might come from game logic
   * that computes a ratio (e.g., column / field_width) and could slightly
   * exceed ±1 due to floating-point rounding.                                */
  if (pan_position < -1.0f) pan_position = -1.0f;
  if (pan_position >  1.0f) pan_position =  1.0f;
  inst->pan_position = pan_position;

  /* Frequency slide: how many Hz to add per sample to sweep from start to end.
   * Per-sample slide = (end_freq − start_freq) / total_samples.
   * Over the full duration the pitch travels exactly from frequency → frequency_end.
   * If frequency_end == 0 (SOUND_NONE) or duration is zero, no slide.        */
  if (def->frequency_end > 0 && inst->samples_remaining > 0) {
    inst->frequency_slide =
        (def->frequency_end - def->frequency) / inst->samples_remaining;
  } else {
    inst->frequency_slide = 0;
  }
}

/*
 * game_play_sound — convenience wrapper: play a sound effect centred (pan = 0).
 *
 * Most internal callers (UI sounds, level-up) don't need spatial panning;
 * they call this instead of game_play_sound_at() with an explicit pan.
 *
 * Keeping this function thin (one line) ensures there is only ONE place that
 * actually sets up a SoundInstance — game_play_sound_at().
 */
void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
  game_play_sound_at(audio, sound, 0.0f); /* 0.0f = dead centre              */
}


/* ══════ Music Control ══════════════════════════════════════════════════════ */
/*
 * game_music_play — start the Tetris theme from the beginning.
 *
 * Resets all sequencer counters so playback always starts at Pattern A,
 * Step 0 — useful for a clean restart after GAME_OVER.
 *
 * current_volume is reset to 0 so the first note fades in (no click).
 */
void game_music_play(GameAudioState *audio) {
  audio->music.is_playing      = 1;
  audio->music.current_pattern = 0;  /* Start at Pattern A */
  audio->music.current_step    = 0;  /* Start at the first note of Pattern A */
  audio->music.step_timer      = 0;  /* Reset the step countdown              */
  audio->music.tone.current_volume = 0.0f; /* Fade in from silence            */
  audio->music.tone.pan_position   = 0.0f; /* Keep music centred always       */
}

/*
 * game_music_stop — silence the background music.
 *
 * Sets both is_playing flags.  The sample loop in game_get_audio_samples()
 * skips music entirely when audio->music.is_playing == 0, so the tone
 * generator goes quiet on the next buffer fill.
 */
void game_music_stop(GameAudioState *audio) {
  audio->music.is_playing    = 0;
  audio->music.tone.is_playing = 0;
}


/* ══════ Music Sequencer Update ══════════════════════════════════════════════ */
/*
 * game_audio_update — advance the step sequencer by delta_time seconds.
 *
 * Called once per game frame from game_update() (game.c).
 * This is NOT the audio sample loop — it is the "musical brain" that
 * decides which note should be playing at any given moment.
 *
 * HOW THE SEQUENCER WORKS
 * ────────────────────────
 * step_timer accumulates delta_time each frame.
 * When step_timer >= step_duration, one step has elapsed:
 *   1. Read the MIDI note at patterns[current_pattern][current_step].
 *   2. If note > 0: set the tone's frequency and mark it as playing.
 *      If note == 0: mark the tone as NOT playing (rest).
 *   3. Advance current_step.  When step 15 is passed, wrap to step 0 and
 *      advance to the next pattern (cycling A → B → C → D → A → ...).
 *
 * JS equivalent:
 *   setInterval(() => {
 *     const note = patterns[currentPattern][currentStep];
 *     if (note) { tone.frequency.value = midiToFreq(note); tone.start(); }
 *     else        tone.stop();
 *     if (++currentStep >= 16) { currentStep = 0; currentPattern = (currentPattern+1) % 4; }
 *   }, stepDuration * 1000);
 *
 * @param audio       Pointer to the game's audio state.
 * @param delta_time  Seconds elapsed since the last frame.
 */
void game_audio_update(GameAudioState *audio, float delta_time) {
  if (!audio->music.is_playing)
    return; /* Nothing to update if music is stopped */

  /* JS: `const seq = audio.music;` — but in C we take a pointer to avoid
   * retyping `audio->music.` on every line.
   * JS: reference type (object) | C: pointer — `seq->field` reads through it */
  MusicSequencer *seq = &audio->music;

  /* Accumulate elapsed time toward the next step boundary */
  seq->step_timer += delta_time;

  /* Has a full step elapsed? (could be multiple steps if frame rate is slow) */
  if (seq->step_timer >= seq->step_duration) {
    /* Subtract (don't zero) to carry remainder into the next step.
     * Zeroing would cause tempo drift on variable frame rates.               */
    seq->step_timer -= seq->step_duration;

    /* Read the current note from the active pattern.
     * JS: const note = seq.patterns[seq.currentPattern][seq.currentStep];   */
    uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];

    /* Set the tone frequency, or silence if this step is a rest (note == 0). */
    if (note > 0) {
      seq->tone.frequency  = midi_to_freq(note); /* Convert MIDI → Hz         */
      seq->tone.is_playing = 1;                  /* Signal: play this note     */
    } else {
      seq->tone.is_playing = 0; /* Rest — sample loop will ramp volume to 0   */
    }

    /* Advance to the next step */
    seq->current_step++;
    if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
      seq->current_step = 0; /* Wrap back to the start of the pattern          */
      /* Advance to the next pattern, wrapping A→B→C→D→A cyclically.
       * JS: (currentPattern + 1) % MUSIC_NUM_PATTERNS                        */
      seq->current_pattern = (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
    }
  }
}


/* ══════ Generate Audio Samples ═════════════════════════════════════════════ */
/*
 * game_get_audio_samples — fill the platform's audio buffer with PCM data.
 *
 * Handmade Hero principle: Audio phase is continuous.
 * ────────────────────────────────────────────────────
 * This function is the "hot path" of the audio engine.  It is called by the
 * platform every audio callback (typically every ~5–20 ms, depending on
 * buffer size).  All it does is produce numbers — no OS calls, no allocation.
 *
 * Key principle (from Handmade Hero): sound generation is purely a math
 * problem.  The platform provides a buffer; we fill it sample by sample.
 * inst->phase ticks continuously — resetting during generation causes a click.
 *
 * WHY SAMPLE-BY-SAMPLE, NOT BUFFER-BY-BUFFER?
 * ─────────────────────────────────────────────
 * We process one stereo pair at a time (the inner loop body) rather than
 * generating all sound-effect samples first and all music samples second.
 * This matters when effects interact with the sequencer (they don't here),
 * but more importantly it keeps the mixing logic simple and readable:
 * for each sample, sum all contributors, then write left & right.
 *
 * BUFFER FORMAT (reminder from utils/audio.h):
 *   Interleaved stereo int16:  [L0, R0, L1, R1, L2, R2, ...]
 *   Written via: *out++ = left_sample; *out++ = right_sample;
 *
 * JS equivalent: this function is the onaudioprocess callback body:
 *   scriptProcessor.onaudioprocess = (e) => {
 *     const left  = e.outputBuffer.getChannelData(0);
 *     const right = e.outputBuffer.getChannelData(1);
 *     for (let s = 0; s < bufferSize; s++) {
 *       let mixL = 0, mixR = 0;
 *       // ... add SFX and music contributions ...
 *       left[s]  = mixL * masterVolume;
 *       right[s] = mixR * masterVolume;
 *     }
 *   };
 *
 * @param state   Full game state (audio lives at state->audio).
 * @param buffer  Platform-provided output buffer to fill.
 */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
  GameAudioState *audio = &state->audio;

  /* JS: let out = buffer.samples; — pointer that walks forward through the buffer */
  int16_t *out = buffer->samples;

  int   sample_count = buffer->sample_count;

  /* Pre-compute the inverse sample rate once; multiplying is cheaper than
   * dividing inside the inner loop.
   * JS: const invSampleRate = 1 / buffer.samplesPerSecond;                   */
  float inv_sample_rate = 1.0f / (float)buffer->samples_per_second;

  /* Zero the output buffer first.  If no sounds are active, we output silence.
   * sample_count * 2 = total int16 values (left + right per pair).
   * JS: buffer.samples.fill(0);                                               */
  memset(out, 0, sample_count * 2 * sizeof(int16_t));

  /* Volume ramp speed for music note transitions.
   * Applied per-sample to current_volume → smooths note on/off transitions.
   * 0.002 per sample at 48 kHz → full 0→1 transition in 500 samples ≈ 10 ms.
   * This is the "volume ramp" technique — equivalent to a CSS `transition`
   * on the gain property of a Web Audio GainNode.                            */
  const float VOLUME_RAMP_SPEED = 0.002f;

  /* ── Main Sample Loop ─────────────────────────────────────────────────── */
  /* JS: for (let s = 0; s < sampleCount; s++) { ... }                        */
  for (int s = 0; s < sample_count; s++) {

    /* Accumulators for the mixed stereo signal, in normalised float range.
     * All contributions are summed here before being scaled and written.
     * JS: let mixedLeft = 0, mixedRight = 0;                                 */
    float mixed_left  = 0.0f;
    float mixed_right = 0.0f;

    /* ─── Sound Effects (SFX) ─────────────────────────────────────────── */
    /* JS: for (const inst of activeSounds) { ... }                           */
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
      /* JS: const inst = audio.activeSounds[i]; | C: pointer to the struct   */
      SoundInstance *inst = &audio->active_sounds[i];

      /* Skip idle slots — samples_remaining <= 0 means this voice is done.  */
      if (inst->samples_remaining <= 0)
        continue;

      /* ── Square wave oscillator ──
       * A square wave alternates between +1 and −1 at the given frequency.
       * We use phase (0.0 → 1.0) to decide which half-cycle we're in:
       *   phase < 0.5  → first half → output +1 (positive cycle)
       *   phase >= 0.5 → second half → output −1 (negative cycle)
       *
       * JS Web Audio OscillatorNode type 'square' does exactly this.
       * Here we implement it with a ternary expression for full transparency.
       *
       * JS: const wave = inst.phase < 0.5 ? 1.0 : -1.0;
       * C:  ternary operator — same syntax as JS                             */
      float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;

      /* ── Fade-out envelope ──
       * env decays linearly from 1.0 (at sound start) to 0.0 (at end).
       *
       *   env = samples_remaining / total_samples
       *
       * When samples_remaining == total_samples (very first sample): env = 1.0
       * When samples_remaining == 1 (very last sample):              env ≈ 0.0
       *
       * This prevents the harsh "click" that would occur if a square wave
       * stopped abruptly at full amplitude (waveform discontinuity → click).
       *
       * JS: const env = inst.samplesRemaining / inst.totalSamples;           */
      float env = (float)inst->samples_remaining / (float)inst->total_samples;

      /* ── Fade-in envelope ──
       * samples_played counts how far into the sound we are.
       * During the first fade_in_samples samples, multiply env by a 0→1 ramp.
       *
       * WHY FADE IN?
       *   Setting phase = 0 in game_play_sound_at() means the wave starts at
       *   +1 (first half-cycle of square wave).  Without a fade-in, the
       *   sudden jump from 0 to full amplitude still creates a click at t=0.
       *   The 96-sample (~2 ms) fade-in ramps past that discontinuity so
       *   smoothly the ear does not notice.
       *
       * JS: const samplesPlayed = inst.totalSamples - inst.samplesRemaining;
       *     if (samplesPlayed < inst.fadeInSamples) env *= samplesPlayed / inst.fadeInSamples; */
      int samples_played = inst->total_samples - inst->samples_remaining;
      if (samples_played < inst->fade_in_samples) {
        float fade_in = (float)samples_played / (float)inst->fade_in_samples;
        env *= fade_in; /* Scale the envelope by the fade-in ramp [0..1]      */
      }

      /* ── Per-sample contribution ──
       * Final SFX sample = waveform × voice volume × envelope × sfx sub-mix  */
      float sample = wave * inst->volume * env * audio->sfx_volume;

      /* ── Stereo panning ──
       * calculate_pan_volumes() (utils/audio.h) maps pan_position → gain pair.
       * Multiply the mono sample by each channel's gain before mixing.
       * JS: StereoPannerNode does this; here we call the math function directly. */
      float left_vol, right_vol;
      calculate_pan_volumes(inst->pan_position, &left_vol, &right_vol);

      mixed_left  += sample * left_vol;
      mixed_right += sample * right_vol;

      /* ── Phase accumulator ──
       * Advance the oscillator by exactly one sample period.
       *
       *   phase += frequency * (1 / sample_rate)
       *
       * For 440 Hz at 48000 Hz sample rate:
       *   advance = 440 / 48000 ≈ 0.00917 per sample
       *   full cycle = 48000 / 440 ≈ 109 samples
       *
       * When phase crosses 1.0, subtract 1.0 (don't reset to 0!) to preserve
       * the fractional part — this keeps the timing exact.
       *
       * ⚠ CRITICAL: inst->phase is NEVER reset during buffer generation.
       * Only reset when a NEW sound starts (in game_play_sound_at()).
       * Resetting here would create a waveform discontinuity mid-playback
       * → audible click/pop.
       *
       * Handmade Hero principle: Audio phase is continuous.
       * inst->phase ticks continuously — resetting during generation causes a click.
       *
       * JS: Web Audio OscillatorNode manages phase internally and never resets it. */
      inst->phase += inst->frequency * inv_sample_rate;
      if (inst->phase >= 1.0f)
        inst->phase -= 1.0f; /* Wrap: keep phase in [0, 1) without losing fraction */

      /* ── Frequency slide ──
       * Add frequency_slide (Hz/sample) so pitch sweeps over the duration.
       * Positive slide = rising pitch; negative = falling.
       * After sliding, frequency approaches frequency_end from the preset.   */
      inst->frequency += inst->frequency_slide;

      /* ── Consume one sample from the remaining budget ──
       * When this reaches 0 the slot becomes free for the next sound.        */
      inst->samples_remaining--;
    }

    /* ─── Background Music ────────────────────────────────────────────── */
    if (audio->music.is_playing) {
      ToneGenerator *tone = &audio->music.tone;

      /* ── Volume ramp (prevents click on note transitions) ──
       * The sequencer (game_audio_update) sets tone->is_playing and
       * tone->frequency each step.  Instead of snapping current_volume
       * instantly to 0 or tone->volume, we nudge it by VOLUME_RAMP_SPEED
       * per sample.  This is a first-order low-pass filter on volume.
       *
       * target_volume = 0 when resting (is_playing == 0),
       *               = tone->volume when a note is active.
       *
       * JS: think of it like `currentVolume += (target - currentVolume) * 0.002`
       * but with a linear step instead of exponential approach.
       * CSS analogy: `transition: volume 10ms linear`.                       */
      float target_volume = tone->is_playing ? tone->volume : 0.0f;

      if (tone->current_volume < target_volume) {
        tone->current_volume += VOLUME_RAMP_SPEED;
        if (tone->current_volume > target_volume)
          tone->current_volume = target_volume; /* Don't overshoot            */
      } else if (tone->current_volume > target_volume) {
        tone->current_volume -= VOLUME_RAMP_SPEED;
        if (tone->current_volume < 0.0f)
          tone->current_volume = 0.0f;          /* Clamp at zero; never negative */
      }

      /* Only generate samples when the volume is non-negligible.
       * The 0.0001f threshold avoids wasting time on effectively-silent math. */
      if (tone->current_volume > 0.0001f || tone->is_playing) {
        /* Same square wave as SFX, using the music tone's own phase         */
        float wave   = (tone->phase < 0.5f) ? 1.0f : -1.0f;
        float sample = wave * tone->current_volume * audio->music_volume;

        /* Music is centred (pan_position = 0.0 set at init), but the same
         * panning code applies so the music path is consistent with SFX.    */
        float left_vol, right_vol;
        calculate_pan_volumes(tone->pan_position, &left_vol, &right_vol);

        mixed_left  += sample * left_vol;
        mixed_right += sample * right_vol;

        /* ⚠ CRITICAL: tone->phase is NEVER reset during buffer generation.
         * The music tone plays continuously across frames — its phase must
         * carry over between buffer fills, or note transitions will click.
         *
         * Handmade Hero principle: Audio phase is continuous.
         * inst->phase ticks continuously — resetting during generation causes a click. */
        tone->phase += tone->frequency * inv_sample_rate;
        if (tone->phase >= 1.0f)
          tone->phase -= 1.0f;
      }
    }

    /* ─── Final Mix: float → int16 ───────────────────────────────────────── */
    /* Scale the normalised float mix (roughly −1..+1 range) to the int16
     * range (−32768..+32767).
     *
     * WHY 16000 instead of 32767?
     *   Using the full range would cause clipping whenever two simultaneous
     *   sounds each contribute near 1.0.  16000 leaves ~6 dB of headroom so
     *   multiple sounds can mix without distortion.
     *   In practice: 2 sounds at 0.8 volume = 2 × 0.8 × 16000 = 25600, well
     *   within the 32767 ceiling.
     *
     * master_volume is applied here — last in the signal chain — so it scales
     * the already-mixed signal uniformly.
     *
     * JS: const scaledLeft = mixedLeft * masterVolume * 16000;               */
    mixed_left  *= audio->master_volume * 16000.0f;
    mixed_right *= audio->master_volume * 16000.0f;

    /* Write the stereo pair into the interleaved buffer.
     * clamp_sample() (utils/audio.h) ensures no int16 overflow.
     *
     * *out++ is the dereference-then-advance idiom:
     *   1. Write to the memory address `out` points at.
     *   2. Advance `out` to point at the next int16.
     * JS: buffer.samples[s * 2 + 0] = clampSample(mixedLeft);
     *     buffer.samples[s * 2 + 1] = clampSample(mixedRight);               */
    *out++ = clamp_sample(mixed_left);  /* Left  channel of stereo pair s     */
    *out++ = clamp_sample(mixed_right); /* Right channel of stereo pair s     */
  }
}
