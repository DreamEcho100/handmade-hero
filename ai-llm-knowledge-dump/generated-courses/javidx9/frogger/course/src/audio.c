/* =============================================================================
 * audio.c — Software Sound Mixer for Frogger (Full Audio System)
 * =============================================================================
 *
 * THREE LAYERS:
 *   1. ONE-SHOT SFX   — game events (hop, squash, splash, home, win, etc.)
 *   2. BACKGROUND MUSIC — chiptune note sequencer (game_music_update)
 *   3. AMBIENT LOOPS  — traffic drone + river noise (game_update_ambients)
 *
 * SYNTHESIS PIPELINE (per audio frame):
 *   For each stereo frame fi:
 *     For each active SoundInstance si:
 *       raw  = wave(si)         — SQUARE / NOISE / TRIANGLE / SAWTOOTH
 *       env  = envelope(si)     — FADEOUT / TRAPEZOID / FLAT
 *       amp  = raw × env × def.volume × inst.vol_scale × master_volume
 *       L   += amp × pan_left
 *       R   += amp × pan_right
 *       advance phase, slide freq, decrement samples_remaining
 *     output[fi*2+0] = clamp(L * 32767)
 *     output[fi*2+1] = clamp(R * 32767)
 *   Compact pool — looping instances reset instead of being freed
 *
 * VOLUME HIERARCHY:
 *   amp = raw × envelope × def.volume × inst.vol_scale × master_volume
 *   SFX:     vol_scale = sfx_volume     (set at trigger time)
 *   Music:   vol_scale = music_volume   (set per note by sequencer)
 *   Ambient: vol_scale = 0..ambient_vol (updated each game frame by distance)
 *
 * MUSIC SEQUENCER:
 *   game_music_update(state, dt) advances a note index through MUSIC_NOTES[].
 *   Each note is a {frequency, duration} pair.  A zero frequency = rest.
 *   On note change, the SOUND_MUSIC slot's freq+total_samples are overwritten.
 *
 * AMBIENT DISTANCE MODEL:
 *   game_update_ambients(state) computes:
 *     traffic_vol = ambient_volume × 1/(1 + road_dist × 0.6)
 *     water_vol   = ambient_volume × 1/(1 + river_dist × 0.7)
 *   road lanes: 5-8, river lanes: 1-3.  vol_scale is lerped 20% per frame.
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include <math.h>    /* fabsf */
#include <stdlib.h>  /* rand  */
#include <string.h>  /* memset */

/* ══════ Wave Types ════════════════════════════════════════════════════════ */
#define WAVE_SQUARE    0  /* alternates ±1 every half period                 */
#define WAVE_NOISE     1  /* uniform random ±1 per sample (white noise)      */
/* WAVE_TRIANGLE — ramps linearly -1→+1→-1 each period.  Softer than square.
   s = 1.0f - fabsf(4.0f * phase - 2.0f)                                   */
#define WAVE_TRIANGLE  2
/* WAVE_SAWTOOTH — ramps +1→-1 each period.  Rich harmonic content, good for
   engine drones.  s = 1.0f - 2.0f * phase  (phase in [0,1))               */
#define WAVE_SAWTOOTH  3

/* ══════ Envelope Types ════════════════════════════════════════════════════
   FADEOUT   — linear decay 1→0 over full duration.  Hop, squash, splash.
   TRAPEZOID — 10% attack + 80% sustain + 10% decay.  Music notes, jingles.
   FLAT      — constant amplitude = 1.  Ambient drones (vol controlled
               externally via vol_scale updated each game frame).           */
#define ENVELOPE_FADEOUT    0
#define ENVELOPE_TRAPEZOID  1
#define ENVELOPE_FLAT       2

/* ══════ SoundDef — immutable design-time parameters ═══════════════════════
   One entry per SOUND_ID.  Mutable state lives in SoundInstance.          */
typedef struct {
    float base_freq;   /* starting frequency in Hz                         */
    float freq_slide;  /* Hz/second slide (negative = falling pitch)       */
    float duration;    /* loop/note duration in seconds                    */
    float volume;      /* per-sound amplitude scale [0, 1]                 */
    int   wave_type;
    int   envelope;
} SoundDef;

/* ══════ SOUND_DEFS — one entry per SOUND_ID ════════════════════════════════
   MUST match the SOUND_ID enum order in utils/audio.h.

   SOUND_NONE            — silence placeholder.
   SOUND_HOP             — crisp high tick on every grid step.
   SOUND_SQUASH          — percussive noise burst: car impact on road.
   SOUND_SPLASH          — descending noise: water/drowning death.
   SOUND_HOME_REACHED    — ascending triangle: one home cell filled.
   SOUND_WIN_JINGLE      — wide ascending sweep: all 5 homes reached.
   SOUND_GAME_OVER       — slow descending square: all lives lost.
   SOUND_LEVEL_START     — quick ascending arpeggio: level begins / restart.
   SOUND_MUSIC           — melodic note; freq overridden per note by sequencer.
   SOUND_AMBIENT_TRAFFIC — low sawtooth drone: road lanes 5-8.
   SOUND_AMBIENT_WATER   — white noise flow: river lanes 1-3.              */
static const SoundDef SOUND_DEFS[] = {
    /* SOUND_NONE            */ { 0.0f,    0.0f,    0.00f, 0.0f,  WAVE_SQUARE,    ENVELOPE_FADEOUT   },
    /* SOUND_HOP             */ { 900.0f, -300.0f,  0.06f, 0.50f, WAVE_SQUARE,    ENVELOPE_FADEOUT   },
    /* SOUND_SQUASH          */ { 150.0f,  0.0f,    0.20f, 0.80f, WAVE_NOISE,     ENVELOPE_FADEOUT   },
    /* SOUND_SPLASH          */ { 280.0f, -90.0f,   0.35f, 0.75f, WAVE_NOISE,     ENVELOPE_FADEOUT   },
    /* SOUND_HOME_REACHED    */ { 523.0f,  200.0f,  0.30f, 0.70f, WAVE_TRIANGLE,  ENVELOPE_TRAPEZOID },
    /* SOUND_WIN_JINGLE      */ { 523.0f,  580.0f,  0.55f, 0.75f, WAVE_TRIANGLE,  ENVELOPE_TRAPEZOID },
    /* SOUND_GAME_OVER       */ { 220.0f, -55.0f,   0.90f, 0.80f, WAVE_SQUARE,    ENVELOPE_FADEOUT   },
    /* SOUND_LEVEL_START     */ { 440.0f,  440.0f,  0.35f, 0.60f, WAVE_TRIANGLE,  ENVELOPE_TRAPEZOID },
    /* SOUND_MUSIC           */ { 659.0f,  0.0f,    0.20f, 0.40f, WAVE_TRIANGLE,  ENVELOPE_TRAPEZOID },
    /* SOUND_AMBIENT_TRAFFIC */ { 65.0f,   0.0f,    1.00f, 0.55f, WAVE_SAWTOOTH,  ENVELOPE_FLAT      },
    /* SOUND_AMBIENT_WATER   */ { 0.0f,    0.0f,    1.00f, 0.45f, WAVE_NOISE,     ENVELOPE_FLAT      },
};

/* ══════ Music Note Table ═══════════════════════════════════════════════════
   Pentatonic chiptune melody loosely inspired by the arcade original.
   Frequencies are standard 12-TET (equal temperament) pitches.
   Duration 0.14s/note at ~107 BPM; rest = frequency 0.

   Note names for reference:
     C5=523.25  E5=659.25  G5=783.99  A5=880.00
     B5=987.77  C6=1046.5

   The melody is 20 notes; it loops continuously during gameplay.          */
typedef struct { float freq; float dur; } MusicNote;

#define MUSIC_NOTE_COUNT 20
static const MusicNote MUSIC_NOTES[MUSIC_NOTE_COUNT] = {
    { 880.0f,   0.14f },  /* A5  */
    { 783.99f,  0.14f },  /* G5  */
    { 659.25f,  0.14f },  /* E5  */
    { 783.99f,  0.14f },  /* G5  */
    { 880.0f,   0.14f },  /* A5  */
    { 783.99f,  0.14f },  /* G5  */
    { 659.25f,  0.28f },  /* E5  (held longer) */
    { 0.0f,     0.14f },  /* rest */
    { 659.25f,  0.14f },  /* E5  */
    { 783.99f,  0.14f },  /* G5  */
    { 880.0f,   0.14f },  /* A5  */
    { 987.77f,  0.14f },  /* B5  */
    { 880.0f,   0.14f },  /* A5  */
    { 783.99f,  0.14f },  /* G5  */
    { 659.25f,  0.14f },  /* E5  */
    { 523.25f,  0.14f },  /* C5  */
    { 659.25f,  0.28f },  /* E5  (held longer) */
    { 0.0f,     0.14f },  /* rest */
    { 523.25f,  0.14f },  /* C5  */
    { 659.25f,  0.14f },  /* E5  */
};

/* ══════ Internal helpers ═══════════════════════════════════════════════════

   find_sound_slot_by_id — returns the index of the first active SoundInstance
   with the given SOUND_ID, or -1 if none is found.  Used by ambient updaters
   and game_set_sound_vol() to locate a live looping instance.              */
static int find_sound_slot_by_id(GameAudioState *audio, SOUND_ID id) {
    int i;
    for (i = 0; i < audio->active_sound_count; i++) {
        if (audio->active_sounds[i].sound_id == id)
            return i;
    }
    return -1;
}

static void apply_pan(SoundInstance *inst, float pan) {
    float p = pan < -1.0f ? -1.0f : pan > 1.0f ? 1.0f : pan;
    inst->pan_left  = (p <= 0.0f) ? 1.0f : (1.0f - p);
    inst->pan_right = (p >= 0.0f) ? 1.0f : (1.0f + p);
}

/* ══════ game_play_sound ════════════════════════════════════════════════════
   Allocates a SoundInstance slot and initialises it.
   vol_scale:  pass sfx_volume for SFX, music_volume for music notes.
   looping:    0 = expire after duration; 1 = auto-restart (ambient).     */
static void play_sound_internal(GameState *state, SOUND_ID id, float pan,
                                float vol_scale, int looping) {
    if (id == SOUND_NONE || id >= SOUND_COUNT) return;
    if (state->audio.samples_per_second <= 0) return;

    GameAudioState *audio = &state->audio;
    const SoundDef *def   = &SOUND_DEFS[(int)id];

    /* Prefer a free slot; otherwise evict the slot with fewest samples left
       (skipping looping slots — they should not be evicted).               */
    int slot = -1;
    if (audio->active_sound_count < MAX_SIMULTANEOUS_SOUNDS) {
        slot = audio->active_sound_count++;
    } else {
        int min_remaining = 0x7fffffff;
        int k;
        for (k = 0; k < MAX_SIMULTANEOUS_SOUNDS; k++) {
            if (!audio->active_sounds[k].looping &&
                audio->active_sounds[k].samples_remaining < min_remaining) {
                min_remaining = audio->active_sounds[k].samples_remaining;
                slot = k;
            }
        }
        if (slot < 0) slot = 0;  /* fallback: evict slot 0 */
    }

    int total = (int)(def->duration * (float)audio->samples_per_second);
    if (total < 1) total = 1;

    SoundInstance *inst = &audio->active_sounds[slot];
    inst->sound_id          = id;
    inst->samples_remaining = total;
    inst->total_samples     = total;
    inst->phase             = 0.0f;
    inst->freq              = def->base_freq;
    inst->looping           = looping;
    inst->vol_scale         = vol_scale;
    apply_pan(inst, pan);
}

/* Public one-shot SFX trigger (used by game.c via trigger_sound macro).   */
void game_play_sound(GameState *state, SOUND_ID id, float pan) {
    play_sound_internal(state, id, pan, state->audio.sfx_volume, 0);
}

/* Start a looping ambient sound at an initial vol_scale (usually 0).      */
void game_play_looping(GameState *state, SOUND_ID id, float pan, float vol_scale) {
    /* Don't double-start — if already active, just update vol_scale       */
    int existing = find_sound_slot_by_id(&state->audio, id);
    if (existing >= 0) {
        state->audio.active_sounds[existing].vol_scale = vol_scale;
        return;
    }
    play_sound_internal(state, id, pan, vol_scale, 1);
}

/* Update vol_scale on an existing looping instance.                       */
void game_set_sound_vol(GameState *state, SOUND_ID id, float vol_scale) {
    int i = find_sound_slot_by_id(&state->audio, id);
    if (i >= 0) state->audio.active_sounds[i].vol_scale = vol_scale;
}

/* ══════ game_music_update ══════════════════════════════════════════════════
   Advances the background music sequencer by dt seconds.
   Called from game_update() every game frame.

   Each note in MUSIC_NOTES[] is triggered as a fresh SOUND_MUSIC instance.
   If the current note is a rest (freq == 0) no sound is started and the
   existing note is left to fade naturally.                                  */
void game_music_update(GameState *state, float dt) {
    GameAudioState *audio = &state->audio;
    if (!audio->music_playing) return;

    audio->music_note_timer -= dt;
    if (audio->music_note_timer > 0.0f) return;

    /* Advance to the next note, wrapping around */
    audio->music_note_idx = (audio->music_note_idx + 1) % MUSIC_NOTE_COUNT;
    const MusicNote *note = &MUSIC_NOTES[audio->music_note_idx];
    audio->music_note_timer = note->dur;

    if (note->freq <= 0.0f) return; /* rest — no new note */

    /* Trigger a music note.  We find or allocate a SOUND_MUSIC slot and
       override its frequency and duration to match the current note.       */
    int sps = audio->samples_per_second;
    if (sps <= 0) return;

    GameState *s = state;
    play_sound_internal(s, SOUND_MUSIC, 0.0f, audio->music_volume, 0);

    /* Override freq and total_samples with the note's actual values       */
    int slot = find_sound_slot_by_id(audio, SOUND_MUSIC);
    if (slot >= 0) {
        int note_samples = (int)(note->dur * (float)sps);
        if (note_samples < 1) note_samples = 1;
        audio->active_sounds[slot].freq              = note->freq;
        audio->active_sounds[slot].total_samples     = note_samples;
        audio->active_sounds[slot].samples_remaining = note_samples;
    }
}

/* ══════ game_update_ambients ═══════════════════════════════════════════════
   Modulates ambient volume based on the frog's distance from road/river.

   Road lanes: 5, 6, 7, 8  (row index from top, 0 = home row)
   River lanes: 1, 2, 3

   Volume formula:
     road_vol  = ambient_volume × 1 / (1 + road_dist  × 0.6)
     water_vol = ambient_volume × 1 / (1 + water_dist × 0.7)

   A gentle 20% lerp toward the target each call prevents instant pops.    */
void game_update_ambients(GameState *state) {
    GameAudioState *audio = &state->audio;
    if (audio->ambient_volume <= 0.0f) return;

    float fy = state->frog_y;

    /* Distance to nearest road lane (5-8) */
    float road_dist = 9999.0f;
    int rl;
    for (rl = 5; rl <= 8; rl++) {
        float d = fabsf(fy - (float)rl);
        if (d < road_dist) road_dist = d;
    }
    float road_vol = audio->ambient_volume / (1.0f + road_dist * 0.6f);

    /* Distance to nearest river lane (1-3) */
    float river_dist = 9999.0f;
    int wl;
    for (wl = 1; wl <= 3; wl++) {
        float d = fabsf(fy - (float)wl);
        if (d < river_dist) river_dist = d;
    }
    float water_vol = audio->ambient_volume / (1.0f + river_dist * 0.7f);

    /* Lerp vol_scale toward target (20% step = gentle fade over ~5 frames) */
    int ts = find_sound_slot_by_id(audio, SOUND_AMBIENT_TRAFFIC);
    if (ts >= 0) {
        float cur = audio->active_sounds[ts].vol_scale;
        audio->active_sounds[ts].vol_scale = cur + (road_vol - cur) * 0.20f;
    }
    int ws = find_sound_slot_by_id(audio, SOUND_AMBIENT_WATER);
    if (ws >= 0) {
        float cur = audio->active_sounds[ws].vol_scale;
        audio->active_sounds[ws].vol_scale = cur + (water_vol - cur) * 0.20f;
    }
}

/* ══════ game_get_audio_samples ═════════════════════════════════════════════
   Fills audio_out->samples with the next batch of synthesised stereo PCM.
   Called by the platform audio callback every ~23 ms (44100 Hz / 1024 frames).

   Volume formula (changed from original):
     OLD: amp = raw × envelope × def.volume × sfx_volume × master_volume
     NEW: amp = raw × envelope × def.volume × inst.vol_scale × master_volume
   vol_scale encodes which audio layer the instance belongs to (SFX / music /
   ambient) so each layer can be scaled independently without extra logic.  */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *audio_out) {
    GameAudioState *audio = &state->audio;

    /* Zero the output buffer */
    int total_samples = audio_out->sample_count * 2;
    int si;
    for (si = 0; si < total_samples; si++)
        audio_out->samples[si] = 0;

    for (si = 0; si < audio->active_sound_count; si++) {
        SoundInstance *inst = &audio->active_sounds[si];
        if (inst->samples_remaining <= 0) continue;

        const SoundDef *def = &SOUND_DEFS[(int)inst->sound_id];
        float sps           = (float)audio_out->samples_per_second;
        float phase_advance = (def->wave_type == WAVE_NOISE) ? 0.0f :
                              inst->freq / sps;

        int fi;
        for (fi = 0; fi < audio_out->sample_count; fi++) {
            if (inst->samples_remaining <= 0) break;

            /* ── Wave generation ──────────────────────────────── */
            float raw;
            switch (def->wave_type) {
                case WAVE_SQUARE:
                    raw = (inst->phase < 0.5f) ? 1.0f : -1.0f;
                    break;
                case WAVE_NOISE:
                    raw = ((float)(rand() % 32767) / 16383.5f) - 1.0f;
                    break;
                case WAVE_TRIANGLE:
                    raw = 1.0f - fabsf(4.0f * inst->phase - 2.0f);
                    break;
                case WAVE_SAWTOOTH:
                    raw = 1.0f - 2.0f * inst->phase;
                    break;
                default:
                    raw = 0.0f;
                    break;
            }

            /* ── Envelope ─────────────────────────────────────── */
            float progress = 1.0f - (float)inst->samples_remaining /
                                    (float)inst->total_samples;
            float envelope;
            switch (def->envelope) {
                case ENVELOPE_TRAPEZOID:
                    if (progress < 0.10f)
                        envelope = progress / 0.10f;
                    else if (progress < 0.90f)
                        envelope = 1.0f;
                    else
                        envelope = (1.0f - progress) / 0.10f;
                    break;
                case ENVELOPE_FLAT:
                    envelope = 1.0f;
                    break;
                default: /* ENVELOPE_FADEOUT */
                    envelope = 1.0f - progress;
                    break;
            }

            /* ── Mix — vol_scale carries per-layer volume ─────── */
            float amp = raw * envelope * def->volume
                      * inst->vol_scale * audio->master_volume;

            float l = (float)audio_out->samples[fi * 2 + 0] / 32767.0f
                      + amp * inst->pan_left;
            float r = (float)audio_out->samples[fi * 2 + 1] / 32767.0f
                      + amp * inst->pan_right;

            if (l < -1.0f) l = -1.0f; if (l > 1.0f) l = 1.0f;
            if (r < -1.0f) r = -1.0f; if (r > 1.0f) r = 1.0f;

            audio_out->samples[fi * 2 + 0] = (int16_t)(l * 32767.0f);
            audio_out->samples[fi * 2 + 1] = (int16_t)(r * 32767.0f);

            /* ── Advance oscillator ───────────────────────────── */
            inst->phase += phase_advance;
            if (inst->phase >= 1.0f) inst->phase -= 1.0f;

            /* ── Frequency slide (skip noise; skip flat envelopes) ── */
            if (def->wave_type != WAVE_NOISE && def->envelope != ENVELOPE_FLAT) {
                inst->freq += def->freq_slide / sps;
                if (inst->freq < 20.0f) inst->freq = 20.0f;
            }

            inst->samples_remaining--;
        }
    }

    /* ── Compact: looping instances reset; expired one-shots removed ── */
    int write = 0;
    for (si = 0; si < audio->active_sound_count; si++) {
        SoundInstance *inst = &audio->active_sounds[si];
        if (inst->samples_remaining > 0) {
            if (write != si) audio->active_sounds[write] = *inst;
            write++;
        } else if (inst->looping) {
            /* Reset looping instance for the next cycle */
            const SoundDef *def = &SOUND_DEFS[(int)inst->sound_id];
            int total = (int)(def->duration * (float)audio->samples_per_second);
            if (total < 1) total = 1;
            inst->samples_remaining = total;
            inst->total_samples     = total;
            inst->freq              = def->base_freq;
            inst->phase             = 0.0f;
            if (write != si) audio->active_sounds[write] = *inst;
            write++;
        }
    }
    audio->active_sound_count = write;
}
