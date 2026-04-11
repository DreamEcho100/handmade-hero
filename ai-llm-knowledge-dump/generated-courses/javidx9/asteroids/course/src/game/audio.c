/* ═══════════════════════════════════════════════════════════════════════════
 * game/audio.c — Asteroids Audio System
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * TWO-LOOP ARCHITECTURE
 * ─────────────────────
 * GAME LOOP (~60 Hz):  game_audio_update(audio, dt_ms)
 *   → Expires finished voices; cleans up the pool.
 *
 * AUDIO LOOP (callback): game_get_audio_samples(audio, buf, num_frames)
 *   → Reads ToneGenerator pool (read-mostly).
 *   → Synthesizes float32 PCM.  Advances phase, applies freq_slide/sample,
 *     decrements samples_remaining.
 *
 * FLOAT32 PCM
 * ───────────
 * Samples are floats in [-1.0, +1.0].  No int16 conversion needed.
 *   Raylib:  AudioStream bitsPerSample=32 + SND_PCM_FORMAT_FLOAT32.
 *   ALSA:    SND_PCM_FORMAT_FLOAT_LE.
 *
 * ENVELOPE TYPES
 * ──────────────
 * ENVELOPE_FADEOUT   — amplitude decays linearly 1→0 over the full duration.
 *                      Used for one-shot SFX (fire, explosions, death).
 * ENVELOPE_TRAPEZOID — 10% attack + 80% sustain + 10% decay.
 *                      Used for looped sounds (thrust, rotate).
 *                      game_sustain_sound() keeps samples_remaining ≥ 25%
 *                      so the voice stays in the sustain zone while a key
 *                      is held.  On key release, the remaining samples drain
 *                      naturally through the decay ramp → smooth fade-out.
 *
 * VOICE POOL
 * ──────────
 * MAX_SOUNDS voices in audio->voices[].  New voices steal the oldest active
 * slot when the pool is full (game_sustain_sound is exempt from stealing).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "game.h"
#include <math.h>   /* sinf, cosf, fabsf */
#include <string.h> /* memset */
#include <stdlib.h> /* rand   */

/* ── SOUND_DEFS ─────────────────────────────────────────────────────────────
 * One entry per SoundID (defined in utils/audio.h).
 * Order MUST match the SoundID enum.
 *
 * Columns: frequency(Hz), freq_slide(Hz/s), volume, pan, duration_ms,
 *          wave_type, envelope_type.
 *
 * SOUND DESIGN NOTES
 * ──────────────────
 * THRUST (120 Hz triangle, TRAPEZOID):
 *   Triangle = no abrupt polarity flips → smooth engine hum.
 *   120 Hz is audible on laptop speakers (60 Hz is not).
 *   TRAPEZOID enables seamless crossfade when game_sustain_sound retriggers.
 *
 * ROTATE (200 Hz triangle, TRAPEZOID):
 *   Quiet ambient feedback; distinct from the 120 Hz thrust.
 *
 * EXPLODE_* (NOISE, FADEOUT):
 *   White noise shaped by freq_slide gives natural boom/crack/snap.
 *
 * MUSIC_GAMEPLAY (110 Hz square, FADEOUT, 3 s):
 *   110 Hz = A2.  FADEOUT over 3 s is barely perceptible; game.c retriggers
 *   it when it expires → seamless background drone.
 *
 * MUSIC_RESTART (330 Hz square, ascending, 0.6 s):
 *   Quick ascending jingle on game restart.                               */
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
  /* SOUND_NONE */
  { 0.0f,    0.0f,     0.00f, 0.0f,  0,    WAVE_SQUARE,   ENVELOPE_FADEOUT    },
  /* SOUND_THRUST — 120 Hz triangle, 1.0 s, TRAPEZOID.  Vol 0.75. */
  { 120.0f,  0.0f,     0.75f, 0.0f,  1000, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID  },
  /* SOUND_FIRE — short high-pitched pew */
  { 1760.0f,-800.0f,   0.55f, 0.0f,  60,   WAVE_SQUARE,   ENVELOPE_FADEOUT    },
  /* SOUND_EXPLODE_LARGE */
  { 140.0f, -80.0f,    0.80f, 0.0f,  550,  WAVE_NOISE,    ENVELOPE_FADEOUT    },
  /* SOUND_EXPLODE_MEDIUM */
  { 220.0f, -120.0f,   0.70f, 0.0f,  350,  WAVE_NOISE,    ENVELOPE_FADEOUT    },
  /* SOUND_EXPLODE_SMALL */
  { 400.0f, -250.0f,   0.60f, 0.0f,  180,  WAVE_NOISE,    ENVELOPE_FADEOUT    },
  /* SOUND_SHIP_DEATH — descending whomp */
  { 55.0f,  -30.0f,    0.85f, 0.0f,  900,  WAVE_SQUARE,   ENVELOPE_FADEOUT    },
  /* SOUND_ROTATE — 200 Hz triangle, 0.6 s, TRAPEZOID.  Vol 0.50. */
  { 200.0f,  0.0f,     0.50f, 0.0f,  600,  WAVE_TRIANGLE, ENVELOPE_TRAPEZOID  },
  /* SOUND_MUSIC_GAMEPLAY — 110 Hz square drone (A2), 3 s, looped by game.c */
  { 110.0f,  0.0f,     0.20f, 0.0f,  3000, WAVE_SQUARE,   ENVELOPE_FADEOUT    },
  /* SOUND_MUSIC_RESTART — ascending jingle */
  { 330.0f,  200.0f,   0.50f, 0.0f,  600,  WAVE_SQUARE,   ENVELOPE_FADEOUT    },
};

/* ── Internal helpers ───────────────────────────────────────────────────── */

/* Find the lowest-age (oldest) active voice for stealing.
 * Returns -1 if all slots are inactive (pool has room without stealing).  */
static int find_steal_candidate(const GameAudioState *audio) {
  int oldest_idx = -1;
  int oldest_age = 0x7FFFFFFF;
  for (int i = 0; i < MAX_SOUNDS; i++) {
    if (audio->voices[i].active &&
        audio->voices[i].age < oldest_age) {
      oldest_age = audio->voices[i].age;
      oldest_idx = i;
    }
  }
  return oldest_idx;
}

/* Allocate a free slot (inactive) or steal the oldest active voice.       */
static ToneGenerator *alloc_voice(GameAudioState *audio) {
  /* Try to find an inactive slot first */
  for (int i = 0; i < MAX_SOUNDS; i++) {
    if (!audio->voices[i].active) {
      return &audio->voices[i];
    }
  }
  /* Pool full — steal oldest */
  int idx = find_steal_candidate(audio);
  if (idx < 0) return &audio->voices[0]; /* fallback, should never happen */
  return &audio->voices[idx];
}

/* Generate one sample from a ToneGenerator.
 * phase_acc is in [0, 1); advances by freq/sample_rate per sample.        */
static float generate_sample(const ToneGenerator *g) {
  switch (g->wave_type) {
  case WAVE_SQUARE:
    return (g->phase_acc < 0.5f) ? 1.0f : -1.0f;
  case WAVE_SAWTOOTH:
    return 2.0f * g->phase_acc - 1.0f;
  case WAVE_NOISE:
    return ((float)(rand() & 0x7FFF) / 16383.5f) - 1.0f;
  case WAVE_TRIANGLE:
    /* Smooth ramp: -1 at 0, +1 at 0.5, -1 at 1.0.  Formula: 1-|4p-2|    */
    return 1.0f - fabsf(4.0f * g->phase_acc - 2.0f);
  default:
    return 0.0f;
  }
}

/* ── game_audio_init ────────────────────────────────────────────────────────
 * Set default volume levels; called from main() before asteroids_init().
 * Values here survive the memset inside asteroids_init because they are
 * saved/restored in that function.                                          */
void game_audio_init(GameAudioState *audio) {
  if (audio->master_volume == 0.0f) audio->master_volume = 1.0f;
  if (audio->sfx_volume    == 0.0f) audio->sfx_volume    = 1.0f;
  if (audio->music_volume  == 0.0f) audio->music_volume  = 0.5f;
}

/* ── game_play_sound_panned ─────────────────────────────────────────────────
 * Queue a new voice with a spatial pan.
 * pan: -1 = hard left, 0 = centre, +1 = hard right.                       */
void game_play_sound_panned(GameAudioState *audio, SoundID id, float pan) {
  if (id <= SOUND_NONE || id >= SOUND_COUNT) return;
  if (audio->samples_per_second <= 0)        return;

  const SoundDef *def = &SOUND_DEFS[id];
  if (def->duration_ms <= 0) return;

  int total = (def->duration_ms * audio->samples_per_second) / 1000;
  if (total <= 0) return;

  ToneGenerator *g = alloc_voice(audio);
  g->phase_acc        = 0.0f;
  g->frequency        = def->frequency;
  g->freq_slide       = def->freq_slide;
  g->volume           = def->volume;
  g->pan              = pan;
  g->samples_remaining = total;
  g->total_samples    = total;
  g->wave_type        = def->wave_type;
  g->envelope_type    = def->envelope_type;
  g->active           = 1;
  g->age              = audio->next_age++;
}

/* ── game_play_sound ─────────────────────────────────────────────────────── */
void game_play_sound(GameAudioState *audio, SoundID id) {
  game_play_sound_panned(audio, id, SOUND_DEFS[id].pan);
}

/* ── game_is_sound_playing ──────────────────────────────────────────────── */
int game_is_sound_playing(const GameAudioState *audio, SoundID id) {
  for (int i = 0; i < MAX_SOUNDS; i++) {
    /* Simple: compare frequency (unique per SoundID) as a proxy for id.
     * This is reliable since no two sounds share the same base_freq.       */
    if (audio->voices[i].active &&
        audio->voices[i].samples_remaining > 0 &&
        audio->voices[i].frequency == SOUND_DEFS[id].frequency) {
      return 1;
    }
  }
  return 0;
}

/* ── game_sustain_sound ─────────────────────────────────────────────────────
 * Keep a TRAPEZOID voice alive while a key is held.
 * Call once per game frame while the key is down; stop when released.
 *
 * If the sound is already playing: keep samples_remaining ≥ 25% of total
 * so the TRAPEZOID envelope stays in its sustain zone (amplitude = 1.0).
 * If not playing: start it (attack ramp plays on first trigger).           */
void game_sustain_sound(GameAudioState *audio, SoundID id) {
  if (id <= SOUND_NONE || id >= SOUND_COUNT) return;

  float ref_freq = SOUND_DEFS[id].frequency;

  for (int i = 0; i < MAX_SOUNDS; i++) {
    ToneGenerator *g = &audio->voices[i];
    if (g->active && g->samples_remaining > 0 &&
        g->frequency == ref_freq) {
      /* Keep above 25% of total → stays in TRAPEZOID sustain zone */
      int floor = g->total_samples / 4;
      if (g->samples_remaining < floor)
        g->samples_remaining = floor;
      return;
    }
  }
  /* Not playing → start it */
  game_play_sound(audio, id);
}

/* ── game_audio_update ──────────────────────────────────────────────────────
 * Called once per game frame (not per audio sample).
 * Marks voices with samples_remaining ≤ 0 as inactive.                    */
void game_audio_update(GameAudioState *audio, float dt_ms) {
  (void)dt_ms; /* frequency slide is applied per-sample in get_audio_samples */
  for (int i = 0; i < MAX_SOUNDS; i++) {
    if (audio->voices[i].active && audio->voices[i].samples_remaining <= 0) {
      audio->voices[i].active = 0;
    }
  }
}

/* ── game_get_audio_samples ─────────────────────────────────────────────────
 * Fill buf->samples with num_frames stereo PCM frames (float32, interleaved).
 * Called from the platform audio drain loop (not the game loop).
 *
 * Synthesis pipeline per frame:
 *   mix_L = 0, mix_R = 0
 *   For each active voice:
 *     raw     = generate_sample(voice)         (waveform)
 *     env     = compute_envelope(voice)         (FADEOUT or TRAPEZOID)
 *     scaled  = raw × env × voice.volume × master
 *     pan_L   = cos((pan+1)/2 × π/2)²           (power-pan law)
 *     pan_R   = cos((1-pan)/2 × π/2)²
 *     mix_L  += scaled × pan_L
 *     mix_R  += scaled × pan_R
 *     advance phase by freq/sample_rate
 *     slide freq by freq_slide/sample_rate
 *     samples_remaining--
 *   buf[fi*2+0] = clamp(mix_L)
 *   buf[fi*2+1] = clamp(mix_R)                                             */
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buf,
                            int num_frames) {
  /* Zero output buffer first (silence) */
  memset(buf->samples, 0, (size_t)num_frames * AUDIO_BYTES_PER_FRAME);

  if (audio->samples_per_second <= 0) return;

  float sps_inv = 1.0f / (float)audio->samples_per_second;
  float master  = audio->master_volume * audio->sfx_volume;

  for (int fi = 0; fi < num_frames; fi++) {
    float mix_l = 0.0f, mix_r = 0.0f;

    for (int vi = 0; vi < MAX_SOUNDS; vi++) {
      ToneGenerator *g = &audio->voices[vi];
      if (!g->active || g->samples_remaining <= 0) continue;

      /* ── Waveform ─────────────────────────────────────────────────── */
      float raw = generate_sample(g);

      /* ── Envelope ─────────────────────────────────────────────────── */
      /* progress: 0 = first sample, 1 = last sample. */
      float progress = 1.0f - ((float)g->samples_remaining /
                                (float)g->total_samples);
      float env;
      if (g->envelope_type == ENVELOPE_TRAPEZOID) {
        if (progress < 0.10f)
          env = progress / 0.10f;        /* 0 → 1 (attack)   */
        else if (progress > 0.90f)
          env = (1.0f - progress) / 0.10f; /* 1 → 0 (decay)    */
        else
          env = 1.0f;                    /* sustain           */
      } else {
        env = 1.0f - progress;           /* linear fade-out   */
      }

      /* ── Volume + pan ─────────────────────────────────────────────── */
      float scaled = raw * env * g->volume * master;

      /* Equal-power pan: pan -1=left, 0=centre, +1=right.
       * pan_l = cos((pan+1)/2 × π/2)², pan_r = sin(...)²
       * Simplified: angle = (pan+1)/2 × π/2                           */
      float angle  = (g->pan + 1.0f) * 0.5f * 1.5707963f; /* π/4 at pan=0 */
      float pan_l  = cosf(angle);
      float pan_r  = sinf(angle);
      mix_l += scaled * pan_l;
      mix_r += scaled * pan_r;

      /* ── Advance oscillator ───────────────────────────────────────── */
      g->phase_acc += g->frequency * sps_inv;
      while (g->phase_acc >= 1.0f) g->phase_acc -= 1.0f;

      /* freq_slide: Hz per second → Hz per sample */
      g->frequency += g->freq_slide * sps_inv;
      if (g->frequency < 10.0f) g->frequency = 10.0f;

      g->samples_remaining--;
    }

    /* ── Write stereo frame ───────────────────────────────────────────── */
    /* Clamp to [-1.0, +1.0] to prevent clipping artifacts */
    mix_l = (mix_l >  1.0f) ?  1.0f : (mix_l < -1.0f) ? -1.0f : mix_l;
    mix_r = (mix_r >  1.0f) ?  1.0f : (mix_r < -1.0f) ? -1.0f : mix_r;
    audio_write_sample(buf, fi, mix_l, mix_r);
  }

  /* ── Expire finished voices ────────────────────────────────────────── */
  for (int vi = 0; vi < MAX_SOUNDS; vi++) {
    if (audio->voices[vi].active && audio->voices[vi].samples_remaining <= 0) {
      audio->voices[vi].active = 0;
    }
  }
}
