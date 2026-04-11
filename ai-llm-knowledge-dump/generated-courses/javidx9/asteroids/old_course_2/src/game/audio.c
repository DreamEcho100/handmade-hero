/* =============================================================================
 * game/audio.c — Software Sound Mixer for Asteroids
 * =============================================================================
 *
 * LESSON 14 — Procedural audio synthesis (no WAV files).  All sounds are
 * generated mathematically from a table of SoundDef parameters, mixed
 * in software, and written to a PCM output buffer each frame.
 *
 * SYNTHESIS PIPELINE (per frame):
 *   For each stereo output frame fi:
 *     mix_left = 0, mix_right = 0
 *     For each active SoundInstance si:
 *       raw = generate_wave(si)   (square / sawtooth / noise / triangle)
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
 *   Comparable to a Web Audio API AudioWorkletProcessor that fills a
 *   Float32Array per quantum.
 * =============================================================================
 */

#include "game.h"
#include <math.h>   /* sinf, fabsf */
#include <stdlib.h> /* rand   */
#include <string.h> /* memset */

/* ══════ Envelope Types ═════════════════════════════════════════════════════
 *
 * LESSON 14 — Two envelope shapes for different sound types:
 *
 * FADEOUT    (default) — amplitude decreases linearly from 1 to 0.
 *            Best for: one-shot SFX (fire, boom, death) where natural decay
 *            feels right.
 *
 * TRAPEZOID  — 10% attack (0→1), 80% sustain (1.0), 10% decay (1→0).
 *            Purpose: seamless looping.  When game.c retriggers a looping
 *            sound (thrust, rotate) the old instance enters its decay zone
 *            (remaining ≤ 10% of total) while the new instance starts its
 *            attack zone.  Because both ramps are linear over the same
 *            window, they sum to exactly 1.0 throughout the crossfade:
 *              old_amp = 1 - decay_progress
 *              new_amp = attack_progress
 *              sum     = 1.0 ✓
 *            Without this, a FADEOUT looping sound produces a perceptible
 *            amplitude pulse (loud→quiet→loud) at each retrigger.         */
#define ENVELOPE_FADEOUT    0
#define ENVELOPE_TRAPEZOID  1

/* ══════ Wave Types ════════════════════════════════════════════════════════ */
#define WAVE_SQUARE    0
#define WAVE_SAWTOOTH  1
#define WAVE_NOISE     2
#define WAVE_TRIANGLE  3

/* ══════ SoundDef — describes how to synthesise one sound type ═════════════
 *
 * LESSON 14 — Each SOUND_ID has a corresponding SoundDef in SOUND_DEFS[].
 * SoundDef holds immutable design-time parameters; SoundInstance (in
 * audio.h) holds the mutable per-play state (phase, freq, remaining).    */
typedef struct {
    float base_freq;   /* starting oscillator frequency in Hz              */
    float freq_slide;  /* Hz per second (negative = descending pitch)      */
    float duration;    /* total duration in seconds                        */
    float volume;      /* relative amplitude scale [0, 1]                  */
    int   wave_type;   /* WAVE_SQUARE, SAWTOOTH, NOISE, or TRIANGLE        */
    int   envelope;    /* ENVELOPE_FADEOUT or ENVELOPE_TRAPEZOID           */
} SoundDef;

/* ══════ SOUND_DEFS — one entry per SOUND_ID ════════════════════════════════
 *
 * LESSON 14 — Order must match the SOUND_ID enum in utils/audio.h.
 * Columns: base_freq, freq_slide, duration, volume, wave_type, envelope
 *
 * SOUND DESIGN NOTES:
 *   THRUST    — 120 Hz triangle, TRAPEZOID, vol 0.75.  Triangle = smooth
 *               bass hum.  TRAPEZOID crossfades seamlessly on retrigger.
 *   ROTATE    — 200 Hz triangle, TRAPEZOID, vol 0.50.  Distinct from thrust.
 *   EXPLODE_* — WAVE_NOISE with FADEOUT; natural boom/crack/snap.
 *   MUSIC     — 110 Hz square drone (A2), looped every 3s via FADEOUT.    */
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    /* SOUND_NONE */
    {   0.0f,     0.0f, 0.00f, 0.00f, WAVE_SQUARE,    ENVELOPE_FADEOUT    },
    /* SOUND_THRUST — smooth bass hum, seamlessly looped */
    { 120.0f,     0.0f, 1.00f, 0.75f, WAVE_TRIANGLE,  ENVELOPE_TRAPEZOID  },
    /* SOUND_FIRE — short high-pitched pew */
    {1760.0f,  -800.0f, 0.06f, 0.55f, WAVE_SQUARE,    ENVELOPE_FADEOUT    },
    /* SOUND_EXPLODE_LARGE */
    { 140.0f,   -80.0f, 0.55f, 0.80f, WAVE_NOISE,     ENVELOPE_FADEOUT    },
    /* SOUND_EXPLODE_MEDIUM */
    { 220.0f,  -120.0f, 0.35f, 0.70f, WAVE_NOISE,     ENVELOPE_FADEOUT    },
    /* SOUND_EXPLODE_SMALL */
    { 400.0f,  -250.0f, 0.18f, 0.60f, WAVE_NOISE,     ENVELOPE_FADEOUT    },
    /* SOUND_SHIP_DEATH — descending whomp */
    {  55.0f,   -30.0f, 0.90f, 0.85f, WAVE_SQUARE,    ENVELOPE_FADEOUT    },
    /* SOUND_ROTATE — 200 Hz triangle, seamlessly looped */
    { 200.0f,     0.0f, 0.60f, 0.50f, WAVE_TRIANGLE,  ENVELOPE_TRAPEZOID  },
    /* SOUND_MUSIC_GAMEPLAY — 110 Hz drone (A2), looped every 3s */
    { 110.0f,     0.0f, 3.00f, 0.20f, WAVE_SQUARE,    ENVELOPE_FADEOUT    },
    /* SOUND_MUSIC_RESTART — ascending jingle on restart */
    { 330.0f,   200.0f, 0.60f, 0.50f, WAVE_SQUARE,    ENVELOPE_FADEOUT    },
};

/* ══════ game_audio_init ════════════════════════════════════════════════════
 *
 * LESSON 14 — Set default volume levels if not already set.
 * Call from main() BEFORE asteroids_init() so values survive the memset
 * inside asteroids_init (they are saved/restored there).                  */
void game_audio_init(GameState *state) {
    if (state->audio.master_volume == 0.0f)
        state->audio.master_volume = 1.0f;
    if (state->audio.sfx_volume == 0.0f)
        state->audio.sfx_volume = 1.0f;
}

/* ══════ game_play_sound_panned ════════════════════════════════════════════
 *
 * Queue a new sound instance with spatial panning.
 * pan: -1 = hard left, 0 = centre, +1 = hard right.
 * Silently dropped if the pool is full.                                   */
void game_play_sound_panned(GameAudioState *audio, SOUND_ID id, float pan) {
    if (id <= SOUND_NONE || id >= SOUND_COUNT)     return;
    if (audio->active_sound_count >= MAX_SIMULTANEOUS_SOUNDS) return;
    if (audio->samples_per_second <= 0)             return;

    const SoundDef *def = &SOUND_DEFS[id];
    int total = (int)(def->duration * (float)audio->samples_per_second);
    if (total <= 0) return;

    SoundInstance *inst = &audio->active_sounds[audio->active_sound_count++];
    inst->sound_id          = id;
    inst->samples_remaining = total;
    inst->total_samples     = total;
    inst->phase             = 0.0f;
    inst->freq              = def->base_freq;
    calculate_pan_volumes(pan, &inst->pan_left, &inst->pan_right);
}

/* ══════ game_play_sound ════════════════════════════════════════════════════
 * Convenience wrapper: play at centre pan (equal L/R).                    */
void game_play_sound(GameAudioState *audio, SOUND_ID id) {
    game_play_sound_panned(audio, id, 0.0f);
}

/* ══════ game_is_sound_playing ══════════════════════════════════════════════
 * Returns 1 if any instance of `id` is still active.
 * Used to guard music re-trigger (don't stack gameplay + restart jingle). */
int game_is_sound_playing(const GameAudioState *audio, SOUND_ID id) {
    for (int i = 0; i < audio->active_sound_count; i++) {
        if (audio->active_sounds[i].sound_id == id &&
            audio->active_sounds[i].samples_remaining > 0)
            return 1;
    }
    return 0;
}

/* ══════ game_sustain_sound ══════════════════════════════════════════════════
 *
 * LESSON 14 — Sustain a looping sound while a key is held.
 * Call once per game frame while the key is down.
 *
 * Behaviour:
 *   • Not playing → starts it (TRAPEZOID attack ramp on first trigger).
 *   • Playing      → clamps samples_remaining ≥ 25% of total so the
 *                    TRAPEZOID envelope stays in the sustain zone (amp=1.0).
 *
 * On key release (stop calling):
 *   • samples_remaining drains naturally to 0.
 *   • TRAPEZOID decay (last 10%) produces a smooth fade-out.              */
void game_sustain_sound(GameAudioState *audio, SOUND_ID id) {
    if (id <= SOUND_NONE || id >= SOUND_COUNT) return;
    for (int i = 0; i < audio->active_sound_count; i++) {
        if (audio->active_sounds[i].sound_id == id &&
            audio->active_sounds[i].samples_remaining > 0)
        {
            int floor = audio->active_sounds[i].total_samples / 4;
            if (audio->active_sounds[i].samples_remaining < floor)
                audio->active_sounds[i].samples_remaining = floor;
            return;
        }
    }
    game_play_sound(audio, id);
}

/* ══════ generate_sample — synthesise one sample for a SoundInstance ═══════
 *
 * LESSON 14 — Four wave shapes, each with a distinct harmonic character:
 *
 * SQUARE:    phase < 0.5 → +1, else → -1.  Rich harmonics, "retro" sound.
 * SAWTOOTH:  2*phase - 1.  Bright, buzzy.
 * NOISE:     rand() → scaled to [-1, +1].  Useful for explosions.
 * TRIANGLE:  1 - |4*phase - 2|.  Smooth, no abrupt edges.  Best for loops.
 *
 * Returns a float in approximately [-1, +1].                              */
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
        s = ((float)(rand() & 0x7FFF) / 16383.5f) - 1.0f;
        break;
    case WAVE_TRIANGLE:
        /* Continuous ramp: -1→+1→-1 per period.  Formula: 1 - |4*phase - 2|
           At phase 0.0: -1   0.25: 0   0.5: +1   0.75: 0   1.0: -1        */
        s = 1.0f - fabsf(4.0f * inst->phase - 2.0f);
        break;
    default:
        s = 0.0f;
    }
    return s;
}

/* ══════ game_get_audio_samples ═════════════════════════════════════════════
 *
 * LESSON 14 — Fill `out` with sample_count stereo PCM frames (int16, interleaved).
 * Called once per audio period; the platform writes the result to ALSA or
 * the Raylib AudioStream.
 *
 * PERFORMANCE NOTE: Two nested loops — O(sample_count × active_sounds).
 * With sample_count ≈ 1024 and active_sounds ≤ 16: ~16 K iterations/frame.
 * Completely negligible on any modern CPU.                                */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out) {
    memset(out->samples, 0, (size_t)(out->sample_count * 2) * sizeof(int16_t));

    GameAudioState *audio = &state->audio;
    if (audio->active_sound_count == 0) return;

    float master = audio->master_volume * audio->sfx_volume;

    for (int fi = 0; fi < out->sample_count; fi++) {
        float mix_left = 0.0f, mix_right = 0.0f;

        for (int si = 0; si < audio->active_sound_count; si++) {
            SoundInstance *inst = &audio->active_sounds[si];
            if (inst->samples_remaining <= 0) continue;

            const SoundDef *def = &SOUND_DEFS[inst->sound_id];

            float raw = generate_sample(def, inst);

            /* Amplitude envelope.
               progress: 0.0 = first sample, 1.0 = last sample.
               ENVELOPE_FADEOUT:   amplitude = 1 - progress (linear decay).
               ENVELOPE_TRAPEZOID: attack [0, 0.1), sustain [0.1, 0.9],
                                   decay  (0.9, 1.0].                      */
            float progress =
                1.0f - ((float)inst->samples_remaining /
                        (float)inst->total_samples);
            float envelope;
            if (def->envelope == ENVELOPE_TRAPEZOID) {
                if      (progress < 0.10f) envelope = progress / 0.10f;
                else if (progress > 0.90f) envelope = (1.0f - progress) / 0.10f;
                else                       envelope = 1.0f;
            } else {
                envelope = 1.0f - progress;  /* FADEOUT */
            }

            float scaled = raw * envelope * def->volume * master;
            mix_left  += scaled * inst->pan_left;
            mix_right += scaled * inst->pan_right;

            /* Advance oscillator phase */
            inst->phase += inst->freq / (float)out->samples_per_second;
            while (inst->phase >= 1.0f) inst->phase -= 1.0f;

            /* Slide frequency (pitch bends) */
            inst->freq += def->freq_slide / (float)out->samples_per_second;
            if (inst->freq < 10.0f) inst->freq = 10.0f;

            inst->samples_remaining--;
        }

        out->samples[fi * 2 + 0] = clamp_sample(mix_left  * 32767.0f);
        out->samples[fi * 2 + 1] = clamp_sample(mix_right * 32767.0f);
    }

    /* Remove finished sounds (swap-with-tail, O(n)) */
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
