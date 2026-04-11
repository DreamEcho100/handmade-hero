/* ═══════════════════════════════════════════════════════════════════════════
 * game/audio_demo.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Demo audio implementation.  Used during the Platform course only.
 *
 * LESSON 09 — AudioOutputBuffer, GameAudioState declarations.
 * LESSON 10 — SOUND_DEFS table, game_play_sound_at (free-slot + steal-oldest),
 *             full decay envelope + 2ms fade-in to prevent click artifacts.
 * LESSON 13 — MusicSequencer: MIDI patterns, dedicated MusicTone with
 *             smoothstep fade, sample-accurate step advancement.
 * LESSON 15 — This file is REMOVED in the final template.  Game courses
 *             replace it with game/audio.c that defines their own sounds.
 *
 * game_get_audio_samples()  — called by the AUDIO LOOP (drain callback).
 * game_audio_update()       — called by the GAME LOOP (once per frame).
 * game_play_sound_at()      — called anywhere to trigger a one-shot sound.
 *
 * Two-loop separation
 * ───────────────────
 * game_audio_update  advances SFX frequency slides at game-time (≈60 Hz).
 * game_get_audio_samples  synthesizes PCM on demand at audio-time (variable).
 * The music sequencer now advances inside game_get_audio_samples (per-sample),
 * so note boundaries are sample-accurate rather than frame-quantized.
 *
 * Click elimination strategy (adopted from games/snake/src/game/audio.c)
 * ───────────────────────────────────────────────────────────────────────
 * Three mechanisms work together to eliminate click/tick artifacts:
 *
 *   1. FADE-IN (per SFX voice): first 88 samples ramp amplitude 0→1 so the
 *      square wave doesn't jump abruptly from silence to full amplitude at
 *      the start of a note.
 *
 *   2. FULL DECAY ENVELOPE (per SFX voice): amplitude decreases from 1→0
 *      continuously over the entire note duration (samples_remaining /
 *      total_samples).  This replaces the older end-only AUDIO_FADE_FRAMES
 *      approach.  Combined with fade-in, the voice has a natural
 *      attack-decay shape that avoids discontinuities at both ends.
 *
 *   3. PERSISTENT PHASE + SMOOTHSTEP FADE (music tone): the single MusicTone
 *      oscillator preserves its phase across note boundaries, so there is no
 *      waveform discontinuity when one note changes to the next.  Amplitude
 *      transitions between notes use a smoothstep curve (t²(3−2t)) over
 *      MUSIC_FADE_SAMPLES = 441 frames (~10 ms), giving a smooth, click-free
 *      crossfade without noticeable blurring of note attacks.
 *
 * Volume levels
 * ─────────────
 * LESSON 16 — float PCM: scale factor is master_volume only (no ×16000).
 * Voices sum into mix_l/mix_r (float), then audio_clamp_sample() clips
 * any sum above ±1.0 before audio_write_sample() writes to the buffer.
 * With float output, clamping is soft and audible only at extreme levels.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include <string.h>

#include "../utils/audio-helpers.h"
#include "../utils/audio.h"

/* ── SOUND_DEFS ────────────────────────────────────────────────────────────
 * LESSON 10 — Static table of sound definitions.
 * Must match SoundID enum order in utils/audio.h.                         */
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    /* SOUND_TONE_LOW  */ {.frequency = 220.0f,
                           .freq_slide = 0.0f,
                           .volume = 0.5f,
                           .pan = 0.0f,
                           .duration_ms = 300},
    /* SOUND_TONE_MID  */
    {.frequency = 440.0f,
     .freq_slide = 0.0f,
     .volume = 0.5f,
     .pan = 0.0f,
     .duration_ms = 300},
    /* SOUND_TONE_HIGH */
    {.frequency = 880.0f,
     .freq_slide = 0.0f,
     .volume = 0.5f,
     .pan = -0.2f,
     .duration_ms = 200},
};

/* ── DEMO MUSIC PATTERNS ────────────────────────────────────────────────────
 * LESSON 13 — MIDI note patterns for the demo music sequencer.
 *
 * ORIGINAL frequency-based melody (commented out for comparison):
 *   static const struct { float freq; int ms; } DEMO_MUSIC_ORIG[] = {
 *     { 261.63f, 200 },  // C4   step = 0.15s × 44100 ≈ 6615 samples
 *     { 293.66f, 200 },  // D4
 *     { 329.63f, 200 },  // E4
 *     { 349.23f, 200 },  // F4
 *     { 392.00f, 200 },  // G4
 *     { 440.00f, 200 },  // A4
 *     { 493.88f, 200 },  // B4
 *     { 523.25f, 400 },  // C5  (held for 2 steps)
 *     {   0.0f,  200 },  // rest
 *   };
 *
 * NEW: MIDI-based patterns (adapted from games/snake/src/game/audio.c).
 * MIDI note 0 = REST.  Standard numbering: C4=60, D4=62, E4=64, F4=65,
 * G4=67, A4=69, B4=71, C5=72.
 *
 * Each pattern has 16 steps.  Step duration is set in game_audio_init_demo
 * to 0.15 s (6615 samples), so one pattern lasts 2.4 s.
 *
 * SNAKE PATTERNS (same as games/snake/src/game/audio.c for comparison):
 *   SNAKE_A = {72,76,79,81,79,76,72,67, 0,74,76,79,76,74,72, 0}
 *   SNAKE_B = same as A in snake source
 *   SNAKE_C = {79, 0,76, 0,72,72,76,79,81, 0,79, 0,76, 0, 0, 0}
 *   SNAKE_D = {74, 0, 0,76,79, 0,76,74,72, 0, 0, 0, 0, 0, 0, 0}
 * Those are C5=72, E5=76, G5=79, A5=81, G4=67, D5=74, B4=71 (in MIDI).
 * Enable them by swapping DEMO_PATTERNS to SNAKE_PATTERNS below.          */

#define DEMO_PATTERN_LEN 16
#define DEMO_NUM_PATTERNS 2

/* Pattern A — C major scale ascending then descending */
static const uint8_t DEMO_PATTERN_A[DEMO_PATTERN_LEN] = {
    60, 62, 64, 65, 67, 69, 71, 72, /* C4 D4 E4 F4 G4 A4 B4 C5 */
    0,  72, 71, 69, 67, 65, 64, 62, /* rest, then descend */
};

/* Pattern B — C major and D minor arpeggios */
static const uint8_t DEMO_PATTERN_B[DEMO_PATTERN_LEN] = {
    60, 64, 67, 60, 64, 67, 60, 0, /* C-E-G (C major arpeggio) */
    62, 65, 69, 62, 65, 69, 62, 0, /* D-F-A (D minor arpeggio) */
};

/* ── Snake game patterns (uncomment DEMO_PATTERNS assignment below to use) ─
static const uint8_t SNAKE_PATTERN_A[DEMO_PATTERN_LEN] = {
  72, 76, 79, 81, 79, 76, 72, 67,   0, 74, 76, 79, 76, 74, 72,  0,
};
static const uint8_t SNAKE_PATTERN_B[DEMO_PATTERN_LEN] = {
  72, 76, 79, 81, 79, 76, 72, 67,   0, 74, 76, 79, 76, 74, 72,  0,
};
static const uint8_t SNAKE_PATTERN_C[DEMO_PATTERN_LEN] = {
  79,  0, 76,  0, 72, 72, 76, 79,  81,  0, 79,  0, 76,  0,  0,  0,
};
static const uint8_t SNAKE_PATTERN_D[DEMO_PATTERN_LEN] = {
  74,  0,  0, 76, 79,  0, 76, 74,  72,  0,  0,  0,  0,  0,  0,  0,
};
static const uint8_t *SNAKE_PATTERNS[4] = {
  SNAKE_PATTERN_A, SNAKE_PATTERN_B, SNAKE_PATTERN_C, SNAKE_PATTERN_D
};
── end snake patterns ────────────────────────────────────────────────── */

static const uint8_t *DEMO_PATTERNS[DEMO_NUM_PATTERNS] = {
    DEMO_PATTERN_A, DEMO_PATTERN_B,
    /* Swap to SNAKE_PATTERNS above (and update DEMO_NUM_PATTERNS to 4)
     * to hear the snake game melody for comparison.                          */
};

static const uint8_t DEMO_PATTERN_C[DEMO_PATTERN_LEN] = {
    48, 55, 60, 55, 48, 55, 60, 55, 50, 57, 62, 57, 50, 57, 62, 57,
};

static const uint8_t DEMO_PATTERN_D[DEMO_PATTERN_LEN] = {
    72, 76, 79, 84, 79, 76, 72, 67, 74, 77, 81, 86, 81, 77, 74, 69,
};

static const uint8_t *LEVEL_PATTERNS[2] = {DEMO_PATTERN_A, DEMO_PATTERN_C};
static const uint8_t *POOL_PATTERNS[2] = {DEMO_PATTERN_B, DEMO_PATTERN_D};
static const uint8_t *SLAB_PATTERNS[2] = {DEMO_PATTERN_D, DEMO_PATTERN_B};

/* ── game_play_sound_at ─────────────────────────────────────────────────────
 * LESSON 10 — Trigger a one-shot sound effect.  Strategy:
 *   1. Find the first inactive voice.
 *   2. If none free, steal the voice with the lowest `age` (oldest started).
 *
 * `fade_in_samples = 88` (≈ 2 ms at 44100 Hz) prevents the sharp waveform
 * discontinuity at note-start that causes a click artifact.  The synthesis
 * loop in game_get_audio_samples ramps the envelope from 0→1 over this window.
 *
 * `total_samples` is set equal to `samples_remaining` so the synthesis loop
 * can compute the full-decay envelope ratio: env = remaining / total.      */
void game_play_sound_at(GameAudioState *audio, SoundID id) {
  if (id < 0 || id >= SOUND_COUNT)
    return;
  const SoundDef *def = &SOUND_DEFS[id];

  int target = -1;
  int oldest_age = -1;
  for (int i = 0; i < MAX_SOUNDS; i++) {
    if (!audio->voices[i].active) {
      target = i;
      break;
    }
    if (oldest_age < 0 || audio->voices[i].age < oldest_age) {
      oldest_age = audio->voices[i].age;
      target = i;
    }
  }
  if (target < 0)
    return;

  int total = (def->duration_ms < 0)
                  ? -1
                  : (def->duration_ms * audio->samples_per_second / 1000);

  ToneGenerator *v = &audio->voices[target];
  v->phase_acc = 0.0f;
  v->frequency = def->frequency;
  v->freq_slide = def->freq_slide;
  v->volume = def->volume;
  v->pan = def->pan;
  v->samples_remaining = total;
  v->total_samples = (total > 0) ? total : 0;
  v->fade_in_samples = (total > 0) ? 88 : 0; /* 2ms attack, anti-click. */
  v->active = 1;
  v->age = audio->next_age++;
}

/* ── game_get_audio_samples ─────────────────────────────────────────────────
 * LESSON 09 (structure), LESSON 10 (SFX multi-voice), LESSON 13 (music).
 *
 * Called by the platform audio drain loop with `num_frames` ≤ max_sample_count.
 *
 * For each sample frame:
 *   SFX voices
 *   ──────────
 *   1. Advance phase by frequency / sample_rate.
 *   2. Compute square wave from phase.
 *   3. Apply full-decay envelope: env = samples_remaining / total_samples.
 *      This continuously ramps amplitude from 1.0→0.0 over the note.
 *   4. Fade-in: if within first fade_in_samples, multiply env by
 *      (samples_played / fade_in_samples) → prevents start-of-note click.
 *   5. Pan into left/right, accumulate into mix.
 *
 *   Music sequencer (sample-accurate)
 *   ───────────────────────────────────
 *   6. Decrement step_samples_remaining each sample.
 *   7. When it reaches 0, load next MIDI note from pattern.
 *   8. Look up frequency with audio_midi_to_freq().
 *   9. Preserve phase across notes (only reset from silence → note).
 *  10. Fade envelope via smoothstep: t²(3−2t) over MUSIC_FADE_SAMPLES.
 *
 *   Final mix
 *   ─────────
 *  11. Scale: mix × master_volume.
 *  12. Clamp to [-1, 1] with audio_clamp_sample(), write via
 * audio_write_sample().
 *  13. Write directly to buf->samples_buffer (interleaved L/R). */
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buf,
                            int num_frames) {
  int frames =
      (num_frames < buf->max_sample_count) ? num_frames : buf->max_sample_count;
  float inv_sr = 1.0f / (float)audio->samples_per_second;

  for (int i = 0; i < frames; i++) {
    float mix_l = 0.0f;
    float mix_r = 0.0f;

    /* ── SFX voices ─────────────────────────────────────────────────── */
    for (int v = 0; v < MAX_SOUNDS; v++) {
      ToneGenerator *gen = &audio->voices[v];
      if (!gen->active)
        continue;
      if (gen->samples_remaining == 0) {
        gen->active = 0;
        continue;
      }

      /* Advance phase accumulator. */
      gen->phase_acc += gen->frequency * inv_sr;
      if (gen->phase_acc >= 1.0f)
        gen->phase_acc -= 1.0f;

      float wave = audio_wave_square(gen->phase_acc);

      /* Full decay envelope: amplitude tracks from 1.0 → 0.0 as the voice
       * plays out.  Combined with fade-in below, this gives a natural
       * attack-decay shape with no abrupt discontinuities at either end.  */
      float env = 1.0f;
      if (gen->total_samples > 0) {
        env = (float)gen->samples_remaining / (float)gen->total_samples;

        /* Fade-in: ramp 0→1 over the first fade_in_samples to prevent
         * the waveform from jumping from silence to full amplitude.       */
        int played = gen->total_samples - gen->samples_remaining;
        if (gen->fade_in_samples > 0 && played < gen->fade_in_samples) {
          env *= (float)played / (float)gen->fade_in_samples;
        }
      }

      float sample = wave * gen->volume * env * audio->sfx_volume;

      float pan_l, pan_r;
      audio_calculate_pan(gen->pan, &pan_l, &pan_r);
      mix_l += sample * pan_l;
      mix_r += sample * pan_r;

      if (gen->samples_remaining > 0)
        gen->samples_remaining--;
    }

    /* ── Music sequencer (sample-accurate) ──────────────────────────── */
    if (audio->sequencer.playing) {
      MusicSequencer *seq = &audio->sequencer;
      MusicTone *tone = &seq->tone;

      /* Advance to next pattern step when the current step expires. */
      if (seq->step_samples_remaining <= 0) {
        const uint8_t *pat = seq->patterns[seq->current_pattern];
        uint8_t note = pat[seq->current_step];

        if (note > 0) {
          float new_freq = audio_midi_to_freq((int)note);
          /* Only reset phase when transitioning from silence.
           * Preserving phase across note-to-note changes eliminates the
           * waveform discontinuity that produces inter-note clicks.       */
          if (!tone->is_playing || tone->frequency == 0.0f) {
            tone->phase = 0.0f;
          }
          tone->frequency = new_freq;
          tone->is_playing = 1;
        } else {
          /* REST — start fade-out. */
          tone->is_playing = 0;
        }
        /* Begin smoothstep fade for both note-on and rest transitions. */
        tone->fade_samples_remaining = MUSIC_FADE_SAMPLES;

        /* Advance step position. */
        seq->step_samples_remaining = seq->step_duration_samples;
        seq->current_step++;
        if (seq->current_step >= seq->pattern_len) {
          seq->current_step = 0;
          seq->current_pattern =
              (seq->current_pattern + 1) % seq->pattern_count;
        }
      }
      seq->step_samples_remaining--;

      /* Smoothstep fade envelope: t²(3−2t).
       *   When is_playing=1: volume rises from 0→tone->volume (fade-in).
       *   When is_playing=0: volume falls from tone->volume→0 (fade-out).
       * The S-curve avoids the amplitude discontinuity of a linear ramp,
       * keeping the transition inaudible even with a square-wave source.  */
      if (tone->fade_samples_remaining > 0) {
        float t = 1.0f - (float)tone->fade_samples_remaining /
                             (float)MUSIC_FADE_SAMPLES;
        float smooth = t * t * (3.0f - 2.0f * t);
        float fade = tone->is_playing ? smooth : (1.0f - smooth);
        tone->current_volume = tone->volume * fade;
        tone->fade_samples_remaining--;
      } else {
        tone->current_volume = tone->is_playing ? tone->volume : 0.0f;
      }

      if (tone->current_volume > 0.0001f && tone->frequency > 0.0f) {
        float wave = audio_wave_square(tone->phase);
        float sample = wave * tone->current_volume * audio->music_volume;

        float pan_l, pan_r;
        audio_calculate_pan(tone->pan, &pan_l, &pan_r);
        mix_l += sample * pan_l;
        mix_r += sample * pan_r;

        tone->phase += tone->frequency * inv_sr;
        if (tone->phase >= 1.0f)
          tone->phase -= 1.0f;
      }
    }

    /* ── Final mix: scale and clamp ─────────────────────────────────── *
     * LESSON 16 — float PCM: multiply by master_volume (no 16000 scale).
     * Clamp to [-1, 1] in case multiple voices sum above the float range. */
    float scale = audio->master_volume;
    audio_write_sample(buf, i, audio_clamp_sample(mix_l * scale),
                       audio_clamp_sample(mix_r * scale));
  }

  /* Zero-fill any remaining capacity (silence). */
  if (frames < buf->max_sample_count) {
    int bps = audio_format_bytes_per_sample(buf->format);
    memset((char *)buf->samples_buffer + frames * bps, 0,
           (size_t)(buf->max_sample_count - frames) * (size_t)bps);
  }
}

/* ── game_audio_update ──────────────────────────────────────────────────────
 * LESSON 10/13 — Called once per game frame from the platform main loop.
 *
 * Now only handles SFX frequency slides.  The music sequencer advances
 * inside game_get_audio_samples (per-sample) rather than here (per-frame),
 * giving sample-accurate note timing.
 *
 * dt_ms: elapsed milliseconds since the previous game frame.              */
void game_audio_update(GameAudioState *audio, float dt_ms) {
  for (int v = 0; v < MAX_SOUNDS; v++) {
    ToneGenerator *gen = &audio->voices[v];
    if (!gen->active || gen->freq_slide == 0.0f)
      continue;
    gen->frequency += gen->freq_slide * (dt_ms / 1000.0f);
    if (gen->frequency < 20.0f)
      gen->frequency = 20.0f; /* clamp to audible */
  }
}

void game_audio_clear_voices(GameAudioState *audio) {
  if (!audio)
    return;
  memset(audio->voices, 0, sizeof(audio->voices));
  audio->next_age = 0;
}

void game_audio_apply_scene_profile(GameAudioState *audio, int scene_index) {
  MusicSequencer *seq;
  const uint8_t **patterns = DEMO_PATTERNS;
  int pattern_count = DEMO_NUM_PATTERNS;
  float music_volume = 0.30f;
  float sfx_volume = 1.0f;
  float tone_volume = 0.30f;
  float step_seconds = 0.15f;

  if (!audio)
    return;

  switch (scene_index) {
  case 0:
    patterns = DEMO_PATTERNS;
    pattern_count = DEMO_NUM_PATTERNS;
    music_volume = 0.18f;
    sfx_volume = 0.85f;
    tone_volume = 0.24f;
    step_seconds = 0.18f;
    break;
  case 1:
    patterns = LEVEL_PATTERNS;
    pattern_count = 2;
    music_volume = 0.22f;
    sfx_volume = 0.90f;
    tone_volume = 0.25f;
    step_seconds = 0.14f;
    break;
  case 2:
    patterns = POOL_PATTERNS;
    pattern_count = 2;
    music_volume = 0.16f;
    sfx_volume = 1.0f;
    tone_volume = 0.22f;
    step_seconds = 0.10f;
    break;
  case 3:
    patterns = SLAB_PATTERNS;
    pattern_count = 2;
    music_volume = 0.34f;
    sfx_volume = 0.92f;
    tone_volume = 0.34f;
    step_seconds = 0.09f;
    break;
  default:
    break;
  }

  game_audio_clear_voices(audio);
  seq = &audio->sequencer;
  seq->patterns = patterns;
  seq->pattern_count = pattern_count;
  seq->pattern_len = DEMO_PATTERN_LEN;
  seq->current_pattern = 0;
  seq->current_step = 0;
  seq->step_duration_samples =
      (int)(step_seconds * (float)audio->samples_per_second);
  seq->step_samples_remaining = 0;
  seq->tone.phase = 0.0f;
  seq->tone.frequency = 0.0f;
  seq->tone.volume = tone_volume;
  seq->tone.current_volume = 0.0f;
  seq->tone.pan = 0.0f;
  seq->tone.is_playing = 0;
  seq->tone.fade_samples_remaining = 0;
  seq->playing = 1;
  audio->music_volume = music_volume;
  audio->sfx_volume = sfx_volume;
}

void game_audio_trigger_scene_enter(GameAudioState *audio, int scene_index) {
  if (!audio)
    return;
  switch (scene_index) {
  case 0:
    game_play_sound_at(audio, SOUND_TONE_LOW);
    break;
  case 1:
    game_play_sound_at(audio, SOUND_TONE_MID);
    break;
  case 2:
    game_play_sound_at(audio, SOUND_TONE_HIGH);
    break;
  case 3:
    game_play_sound_at(audio, SOUND_TONE_HIGH);
    game_play_sound_at(audio, SOUND_TONE_LOW);
    break;
  default:
    break;
  }
}

/* ── game_audio_init_demo ───────────────────────────────────────────────────
 * LESSON 13 — Initialize GameAudioState with the demo music sequencer.
 * Call once after platform_audio_init().
 *
 * step_duration_samples = 0.15 s × 44100 Hz = 6615 samples.
 * One full 16-step pattern = 6615 × 16 = 105840 samples ≈ 2.4 s.         */
void game_audio_init_demo(GameAudioState *audio) {
  memset(audio, 0, sizeof(*audio));

  audio->samples_per_second = AUDIO_SAMPLE_RATE;
  audio->master_volume = 0.8f;
  audio->sfx_volume = 1.0f;
  audio->music_volume = 0.3f;

  MusicSequencer *seq = &audio->sequencer;
  seq->patterns = DEMO_PATTERNS;
  seq->pattern_len = DEMO_PATTERN_LEN;
  seq->pattern_count = DEMO_NUM_PATTERNS;
  seq->current_pattern = 0;
  seq->current_step = 0;
  /* 150 ms per step: fast enough to hear the melody, slow enough to be clear.
   */
  seq->step_duration_samples = (int)(0.15f * (float)AUDIO_SAMPLE_RATE);
  seq->step_samples_remaining = 0; /* Fire immediately on first sample. */
  seq->tone.volume = 0.3f;
  seq->tone.current_volume = 0.0f;
  seq->tone.pan = 0.0f;
  seq->playing = 1;
}
