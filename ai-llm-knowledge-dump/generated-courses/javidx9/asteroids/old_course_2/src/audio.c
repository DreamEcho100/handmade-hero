/* =============================================================================
 * audio.c — Software Sound Mixer for Asteroids
 * =============================================================================
 *
 * Implements procedural audio synthesis (no WAV files).  All sounds are
 * generated mathematically from a table of SoundDef parameters, mixed
 * together in software, and written to a PCM output buffer each frame.
 *
 * SYNTHESIS PIPELINE (per frame):
 *   For each stereo output frame fi:
 *     mix_left = 0, mix_right = 0
 *     For each active SoundInstance si:
 *       raw = generate_wave(si)   (square / sawtooth / noise)
 *       raw *= linear_envelope(si)
 *       raw *= def.volume * sfx_volume * master_volume
 *       mix_left  += raw * si.pan_left
 *       mix_right += raw * si.pan_right
 *       advance si.phase by freq/samplerate
 *       slide si.freq by freq_slide/samplerate
 *     output[fi*2+0] = clamp(mix_left  * 32767)
 *     output[fi*2+1] = clamp(mix_right * 32767)
 *   Remove SoundInstances with samples_remaining <= 0
 *
 * JS equivalent:
 *   Comparable to a Web Audio API ScriptProcessorNode (deprecated) or
 *   AudioWorkletProcessor that fills a Float32Array per quantum.
 * =============================================================================
 */

#include "game.h"
#include <math.h>   /* sinf, fabsf */
#include <stdlib.h> /* rand   */
#include <string.h> /* memset */

/* ══════ Envelope Types ═════════════════════════════════════════════════════

   FADEOUT    (default) — amplitude decreases linearly from 1 to 0 over the
              sound's full duration.  Appropriate for one-shot SFX (fire, boom,
              death) where a natural decay feels right.

   TRAPEZOID  — short attack ramp (0→1) over the first 10% of duration,
              flat sustain for the middle 80%, then decay ramp (1→0) over
              the last 10%.

              Purpose: seamless looping.  When game.c retriggers a looping
              sound (thrust, rotate) at the moment the old instance enters
              its decay zone (remaining ≤ 10% of total), the new instance
              is simultaneously in its attack zone.  Because both ramps are
              linear and cover the same number of samples, they sum to
              exactly 1.0 throughout the crossfade:
                  old_amp  = 1 - (decay_progress)
                  new_amp  = attack_progress
                  sum      = 1.0 ✓

              Without this, a FADEOUT looping sound produces a perceptible
              amplitude pulse (loud→quiet→loud) at each retrigger — the
              "throbbing/spammy" feeling reported by the user.               */
#define ENVELOPE_FADEOUT 0
#define ENVELOPE_TRAPEZOID 1

/* ══════ Wave Types ════════════════════════════════════════════════════════ */
#define WAVE_SQUARE 0
#define WAVE_SAWTOOTH 1
#define WAVE_NOISE 2
/* WAVE_TRIANGLE — ramps linearly from -1→+1 then +1→-1 each period.
   Continuous, no abrupt edges.  s = 1 - |4·phase - 2|
   At phase 0.0: s = -1   At phase 0.25: s = 0   At phase 0.5: s = +1
   At phase 0.75: s = 0   At phase 1.0: s = -1                       */
#define WAVE_TRIANGLE 3

/* ══════ SoundDef — describes how to synthesise one sound type ═════════════

   Each SOUND_ID has a corresponding entry in SOUND_DEFS[].
   The SoundInstance holds the mutable per-play state (phase, freq, remaining);
   SoundDef holds the immutable design-time parameters.                    */
typedef struct {
  float base_freq;  /* starting oscillator frequency in Hz             */
  float freq_slide; /* Hz per second (negative = descending pitch)     */
  float duration;   /* total duration in seconds                       */
  float volume;     /* relative amplitude scale [0, 1]                 */
  int wave_type;    /* WAVE_SQUARE, WAVE_SAWTOOTH, WAVE_NOISE, WAVE_TRIANGLE */
  int envelope;     /* ENVELOPE_FADEOUT or ENVELOPE_TRAPEZOID          */
} SoundDef;

/* ══════ SOUND_DEFS — one entry per SOUND_ID ════════════════════════════════
   Order must match the SOUND_ID enum in utils/audio.h.

   Columns: base_freq, freq_slide, duration, volume, wave_type, envelope

   ENVELOPE_FADEOUT   — one-shot SFX: amplitude decays naturally (1→0).
   ENVELOPE_TRAPEZOID — looping sounds: 10% attack + 80% sustain + 10% decay.
                        Crossfades seamlessly when the old instance enters its
                        decay zone and the new instance starts its attack —
                        their amplitudes always sum to 1.0.

   SOUND DESIGN:
     THRUST    — 60 Hz triangle, 0.5 s, TRAPEZOID, volume 0.20.
               Triangle wave has no abrupt polarity flips → smooth bass hum.
               TRAPEZOID crossfades with new instance when old enters decay.
     ROTATE    — 140 Hz triangle, 0.25 s, TRAPEZOID, volume 0.08.
               Very quiet ambient feedback; distinct from 110 Hz music drone.
     EXPLODE_* — WAVE_NOISE with FADEOUT; natural boom/crack/snap.
     MUSIC     — FADEOUT; 3-second fade is barely perceptible on a looping
   drone.
*/
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    /* SOUND_NONE */
    {0.0f, 0.0f, 0.00f, 0.00f, WAVE_SQUARE, ENVELOPE_FADEOUT},
    /* SOUND_THRUST — 120 Hz triangle, trapezoid.  Duration = 1.0 s.
       120 Hz sits in the speaker-audible bass range (60 Hz is inaudible
       on most laptop/small speakers).  Vol 0.75 = clearly heard engine hum.
       game_sustain_sound holds amplitude at 1.0 while UP is held.          */
    {120.0f, 0.0f, 1.00f, 0.75f, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID},
    /* SOUND_FIRE — short high-pitched pew                                  */
    {1760.0f, -800.0f, 0.06f, 0.55f, WAVE_SQUARE, ENVELOPE_FADEOUT},
    /* SOUND_EXPLODE_LARGE */
    {140.0f, -80.0f, 0.55f, 0.80f, WAVE_NOISE, ENVELOPE_FADEOUT},
    /* SOUND_EXPLODE_MEDIUM */
    {220.0f, -120.0f, 0.35f, 0.70f, WAVE_NOISE, ENVELOPE_FADEOUT},
    /* SOUND_EXPLODE_SMALL */
    {400.0f, -250.0f, 0.18f, 0.60f, WAVE_NOISE, ENVELOPE_FADEOUT},
    /* SOUND_SHIP_DEATH — descending whomp */
    {55.0f, -30.0f, 0.90f, 0.85f, WAVE_SQUARE, ENVELOPE_FADEOUT},
    /* SOUND_ROTATE — 200 Hz triangle, trapezoid.  Duration = 0.6 s.
       200 Hz is well within speaker range; distinct from the 120 Hz thrust
       and 110 Hz music.  Vol 0.50 = audible but not dominant.              */
    {200.0f, 0.0f, 0.60f, 0.50f, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID},
    /* SOUND_MUSIC_GAMEPLAY — 110 Hz square drone (A2), looped every 3 s   */
    {110.0f, 0.0f, 3.00f, 0.2f, WAVE_SQUARE, ENVELOPE_FADEOUT},
    /* SOUND_MUSIC_RESTART — ascending jingle on restart */
    {330.0f, 200.0f, 0.60f, 0.50f, WAVE_SQUARE, ENVELOPE_FADEOUT},
};

/* ══════ game_audio_init ════════════════════════════════════════════════════
   Set default volume levels if they haven't been set already.
   Called from main() BEFORE asteroids_init() so the values survive the
   memset inside asteroids_init (they're saved/restored there).            */
void game_audio_init(GameState *state) {
  if (state->audio.master_volume == 0.0f)
    state->audio.master_volume = 1.0f;
  if (state->audio.sfx_volume == 0.0f)
    state->audio.sfx_volume = 1.0f;
}

/* ══════ game_play_sound_panned ════════════════════════════════════════════
   Queue a new sound instance with a spatial pan value.
   pan: -1 = hard left, 0 = centre, +1 = hard right.
   If the pool is full, the request is silently dropped.                   */
void game_play_sound_panned(GameAudioState *audio, SOUND_ID id, float pan) {
  if (id <= SOUND_NONE || id >= SOUND_COUNT)
    return;
  if (audio->active_sound_count >= MAX_SIMULTANEOUS_SOUNDS)
    return;
  if (audio->samples_per_second <= 0)
    return;

  const SoundDef *def = &SOUND_DEFS[id];
  int total = (int)(def->duration * (float)audio->samples_per_second);
  if (total <= 0)
    return;

  SoundInstance *inst = &audio->active_sounds[audio->active_sound_count++];
  inst->sound_id = id;
  inst->samples_remaining = total;
  inst->total_samples = total;
  inst->phase = 0.0f;
  inst->freq = def->base_freq;

  calculate_pan_volumes(pan, &inst->pan_left, &inst->pan_right);
}

/* ══════ game_play_sound ════════════════════════════════════════════════════
   Convenience wrapper: play at centre pan (both channels equal).          */
void game_play_sound(GameAudioState *audio, SOUND_ID id) {
  game_play_sound_panned(audio, id, 0.0f);
}

/* ══════ game_is_sound_playing ══════════════════════════════════════════════
   Returns 1 if any instance of `id` is still active (samples_remaining > 0).
   Used by game.c to guard music re-trigger (don't stack gameplay + restart). */
int game_is_sound_playing(const GameAudioState *audio, SOUND_ID id) {
  for (int i = 0; i < audio->active_sound_count; i++) {
    if (audio->active_sounds[i].sound_id == id &&
        audio->active_sounds[i].samples_remaining > 0) {
      return 1;
    }
  }
  return 0;
}

/* ══════ game_sustain_sound ══════════════════════════════════════════════════
   Sustain a looping sound while a key is held.  Call once per game frame
   while the key is down; stop calling it when the key is released.

   Behaviour:
     • Not playing → starts it (TRAPEZOID attack ramp plays on first trigger).
     • Playing      → keeps samples_remaining ≥ 25% of total so the TRAPEZOID
                      envelope stays in the sustain zone (amplitude = 1.0).

   On key release (stop calling):
     • samples_remaining drains naturally to 0.
     • TRAPEZOID decay zone (last 10% of duration) produces a smooth fade-out.
     • For 1.0s thrust:  fade-out ≈ 100ms  ("engine winding down")
     • For 0.6s rotate:  fade-out ≈  60ms  ("quick stop")

   Why 25% floor?
     progress = 1 - remaining/total.  At 25% remaining → progress = 75%,
     solidly in sustain (10%–90%).  Each audio push of 2048 samples drops
     remaining by 2048.  At 25% of 44100 = 11025 we have ~5 audio pushes of
     headroom (5 × 2048 = 10240 < 11025) before needing a refill.
     Since the game refills every frame (~16ms, ~1 push between frames),
     this keeps the sound continuously in sustain with no audible variation. */
void game_sustain_sound(GameAudioState *audio, SOUND_ID id) {
  if (id <= SOUND_NONE || id >= SOUND_COUNT)
    return;
  for (int i = 0; i < audio->active_sound_count; i++) {
    if (audio->active_sounds[i].sound_id == id &&
        audio->active_sounds[i].samples_remaining > 0) {
      /* Keep above 25% of total → stays in TRAPEZOID sustain zone   */
      int floor = audio->active_sounds[i].total_samples / 4;
      if (audio->active_sounds[i].samples_remaining < floor)
        audio->active_sounds[i].samples_remaining = floor;
      return;
    }
  }
  /* Sound not playing → start it; attack ramp will play on this first
     trigger, giving the engine-start / spool-up feeling.               */
  game_play_sound(audio, id);
}

/* ══════ generate_sample — synthesise one sample for a SoundInstance ═══════
 *
 * Returns a float in approximately [-1, +1].
 *
 * SQUARE WAVE:
 *   phase in [0, 1) repeating.  phase < 0.5 → +1, phase ≥ 0.5 → -1.
 *   Produces a rich harmonic spectrum; sounds "retro" and electronic.
 *   JS: (phase < 0.5) ? 1 : -1
 *
 * SAWTOOTH WAVE:
 *   Ramps linearly from -1 to +1 over [0,1), then jumps back.
 *   2*phase - 1.
 *
 * NOISE:
 *   Random value in [-1, 1] per sample.  Shaped by frequency-dependent
 *   filtering approximation (lower freq → smoother noise via phase-based
 *   sampling — we sample a new random value only at each oscillator
 *   "zero crossing" to simulate band-limited noise).
 *   Simpler: pure rand per sample for the explosion sounds.
 */
static float generate_sample(const SoundDef *def, SoundInstance *inst) {
  float s;
  switch (def->wave_type) {
  case WAVE_SQUARE:
    s = (inst->phase < 0.5f) ? 1.0f : -1.0f;
    break;
  case WAVE_SAWTOOTH:
    s = 2.0f * inst->phase - 1.0f;
    break;
  case WAVE_NOISE:
    /* rand() returns [0, RAND_MAX].  Scale to [-1, +1].          */
    s = ((float)(rand() & 0x7FFF) / 16383.5f) - 1.0f;
    break;
  case WAVE_TRIANGLE:
    /* Continuous ramp: -1 at phase=0, +1 at phase=0.5, -1 at 1.0.
       Formula: 1 - |4·phase - 2|
       No abrupt edges — far smoother than square or sawtooth.
       Use this for looping background sounds (thrust, rotate).   */
    s = 1.0f - fabsf(4.0f * inst->phase - 2.0f);
    break;
  default:
    s = 0.0f;
  }
  return s;
}

/* ══════ game_get_audio_samples ═════════════════════════════════════════════
 *
 * Fill `out` with sample_count stereo PCM frames (int16, interleaved).
 * Called once per platform frame; the platform writes the result to ALSA
 * or the Raylib AudioStream.
 *
 * PERFORMANCE NOTE: Two nested loops — O(sample_count × active_sounds).
 * With sample_count ≈ 1024 and active_sounds ≤ 8, this is ~8192 iterations
 * per frame.  Completely negligible on any modern CPU.
 */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out) {
  /* Zero output buffer first (silence = 0 for int16) */
  memset(out->samples, 0, (size_t)(out->sample_count * 2) * sizeof(int16_t));

  GameAudioState *audio = &state->audio;
  if (audio->active_sound_count == 0)
    return;

  float master = audio->master_volume * audio->sfx_volume;

  /* ── Per-frame loop: one iteration = one stereo output frame ─────── */
  for (int fi = 0; fi < out->sample_count; fi++) {
    float mix_left = 0.0f;
    float mix_right = 0.0f;

    /* ── Mix all active sounds into this frame ──────────────────── */
    for (int si = 0; si < audio->active_sound_count; si++) {
      SoundInstance *inst = &audio->active_sounds[si];
      if (inst->samples_remaining <= 0)
        continue;

      const SoundDef *def = &SOUND_DEFS[inst->sound_id];

      /* Generate oscillator sample */
      float raw = generate_sample(def, inst);

      /* Amplitude envelope.
         progress: 0.0 = first sample, 1.0 = last sample.
         JS: const p = 1 - (remaining / total);

         ENVELOPE_FADEOUT: amplitude = 1 - progress (linear decay).
         ENVELOPE_TRAPEZOID:
           [0, 0.10) → attack:  amplitude ramps 0 → 1
           [0.10, 0.90] → sustain: amplitude = 1
           (0.90, 1.0] → decay:  amplitude ramps 1 → 0
         Looping sounds (THRUST, ROTATE) retrigger when the old
         instance enters its decay zone (remaining ≤ 10% = threshold).
         The new instance is simultaneously in its attack zone.
         Both ramps are linear over the same window → sum = 1.0.    */
      float progress =
          1.0f - ((float)inst->samples_remaining / (float)inst->total_samples);
      float envelope;
      if (def->envelope == ENVELOPE_TRAPEZOID) {
        if (progress < 0.10f)
          envelope = progress / 0.10f; /* 0 → 1 */
        else if (progress > 0.90f)
          envelope = (1.0f - progress) / 0.10f; /* 1 → 0 */
        else
          envelope = 1.0f; /* sustain */
      } else {
        envelope = 1.0f - progress; /* FADEOUT */
      }

      /* Apply volume chain: waveform → envelope → def.vol → master */
      float scaled = raw * envelope * def->volume * master;

      /* Panned left/right contribution */
      mix_left += scaled * inst->pan_left;
      mix_right += scaled * inst->pan_right;

      /* Advance oscillator phase */
      inst->phase += inst->freq / (float)out->samples_per_second;
      while (inst->phase >= 1.0f)
        inst->phase -= 1.0f;

      /* Slide frequency (allows pitch bends/descents) */
      inst->freq += def->freq_slide / (float)out->samples_per_second;
      if (inst->freq < 10.0f)
        inst->freq = 10.0f; /* floor at 10 Hz */

      inst->samples_remaining--;
    }

    /* Convert float mix to int16 (scale by 32767 and clamp) */
    out->samples[fi * 2 + 0] = clamp_sample(mix_left * 32767.0f);
    out->samples[fi * 2 + 1] = clamp_sample(mix_right * 32767.0f);
  }

  /* ── Remove finished sounds (swap-with-tail) ────────────────────── */
  int i = 0;
  while (i < audio->active_sound_count) {
    if (audio->active_sounds[i].samples_remaining <= 0) {
      audio->active_sounds[i] =
          audio->active_sounds[audio->active_sound_count - 1];
      audio->active_sound_count--;
    } else {
      i++;
    }
  }
}
