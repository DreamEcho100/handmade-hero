/* ═══════════════════════════════════════════════════════════════════════════
 * audio.c  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * The platform-independent audio engine for the Snake game.
 * It generates raw PCM audio samples from a table of sound definitions.
 * No OS calls.  No OpenAL.  No SDL_mixer.  Just math.
 *
 * HOW AUDIO WORKS IN THIS ENGINE
 * ───────────────────────────────
 * Rather than loading .wav files, we SYNTHESIZE audio procedurally:
 *   • A sound is defined by (base frequency, target frequency, duration, volume).
 *   • Each active sound instance tracks: current phase, frequency, remaining samples.
 *   • game_get_audio_samples() mixes all active instances into a stereo int16 buffer.
 *   • The platform hands that buffer to ALSA (X11) or an AudioStream (Raylib).
 *
 * WHY PROCEDURAL SYNTHESIS?
 *   • No file I/O — audio works without any asset pipeline.
 *   • Predictable, tiny, zero-dependency.
 *   • Lets us teach the sample loop math inline (phase, frequency, fade).
 *   • Perfect for a course: students can modify frequencies and hear the effect.
 *
 * SOUND DESIGN (simple but effective for Snake):
 *   SOUND_FOOD_EATEN  — short rising blip: positive audio reinforcement.
 *   SOUND_GAME_OVER   — longer descending tone: "bad thing happened" cue.
 *
 * JS analogy: AudioContext + OscillatorNode, but we compute the samples
 * manually in a loop rather than delegating to the browser's audio graph.
 */

#include "game.h"  /* GameState, GameAudioState, AudioOutputBuffer, SOUND_ID */
#include "utils/math.h"  /* CLAMP */

/* ══════ SoundDef — local sound definition table ════════════════════════════
 *
 * SoundDef is PRIVATE to this file (not exposed in audio.h).
 * Why? The game layer only needs to call game_play_sound(); it doesn't need
 * to know the synthesis parameters.  Hiding the table keeps game.h clean.
 *
 * Fields:
 *   base_frequency      Hz at t=0 (start of the sound)
 *   target_frequency    Hz at t=end (slide destination)
 *   duration_ms         Total sound length in milliseconds
 *   volume              0.0..1.0 — scales amplitude
 *
 * FREQUENCY SLIDE:
 *   Each sample, frequency advances toward target_frequency via linear
 *   interpolation:
 *     frequency += (target_frequency - base_frequency) / total_samples
 *   For SOUND_FOOD_EATEN: 800 → 1200 Hz (rising blip — happy sound)
 *   For SOUND_GAME_OVER:  400 → 100  Hz (falling tone — sad sound)
 */
typedef struct {
    float base_frequency;    /* Starting frequency (Hz) */
    float target_frequency;  /* Ending frequency after slide (Hz) */
    int   duration_ms;       /* Sound length (milliseconds) */
    float volume;            /* 0.0..1.0 amplitude scale */
} SoundDef;
/* SOUND_DEFS — indexed by SOUND_ID enum (0 = SOUND_NONE = unused sentinel).
 *
 * C99 designated initializers: [SOUND_FOOD_EATEN] = { … }
 *   These let us write the table in enum order without worrying about
 *   implicit index arithmetic.  If we add a sound in the middle of the enum,
 *   only that entry needs updating.
 *
 * JS equivalent: Object.freeze({ [SOUND_FOOD_EATEN]: {...}, ... })
 */
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    [SOUND_NONE]       = { 0.0f,    0.0f,    0,   0.0f },  /* sentinel — never played */
    [SOUND_FOOD_EATEN] = { 800.0f, 1200.0f,  80,  0.4f },  /* quick rising blip        */
    [SOUND_GROW]       = { 220.0f,  330.0f,  30,  0.25f }, /* low subtle segment-extend blip */
    [SOUND_RESTART]    = { 300.0f, 1000.0f, 220,  0.5f },  /* rising sweep — restart jingle */
    [SOUND_GAME_OVER]  = { 400.0f,  100.0f, 500,  0.7f },  /* descending sad tone      */
};

/* ══════ game_audio_init ═════════════════════════════════════════════════════
 *
 * Initialize the audio system.
 * Called once in main() after samples_per_second is set by the platform.
 *
 * Currently this function just sets sensible volume defaults.
 * The samples_per_second field is set by the platform before this call.
 *
 * COURSE NOTE: Unlike Tetris, Snake has no music sequencer.  The audio
 * system is deliberately kept simple: sfx only, no looping tracks.
 * This makes audio.c much easier to follow in lessons 09–10.
 */
void game_audio_init(GameState *state) {
    GameAudioState *audio = &state->audio;
    audio->master_volume  = 0.8f;  /* 80% master — leaves headroom */
    audio->sfx_volume     = 1.0f;  /* Full SFX amplitude */
}

/* ══════ Music Sequencer ═════════════════════════════════════════════════════
 *
 * The melody is defined as a static table of (frequency, duration_ms) pairs.
 * A frequency of 0.0 is a REST — the oscillator outputs silence for that note.
 *
 * Each note is played using the same phase-accumulator square wave as the SFX,
 * but through a SEPARATE channel (not one of the active_sounds[] SFX slots).
 * This means music and SFX mix together without consuming polyphony budget.
 *
 * NOTE TRANSITIONS:
 *   Phase is RESET to 0.0 at each note boundary.  This means every note starts
 *   at the positive half-cycle (wave = +1).  A sudden jump from any amplitude
 *   to +1 would cause a click — prevented by the MUSIC_FADE_IN_SAMPLES ramp
 *   (see audio.h).  The SFX rule "never reset phase between fills" still applies
 *   to the FILL level; resetting at a NOTE boundary is intentional.
 *
 * MELODY:
 *   16-note loop in C major.  The frequencies are standard equal-temperament
 *   pitches.  Adding or changing notes here is the simplest course experiment.
 *
 *   C5=523.25  D5=587.33  E5=659.25  G5=783.99  A5=880.00  G4=392.00
 */
typedef struct { float freq; int dur_ms; } Note;

static const Note MELODY[] = {
    /* Bar 1 — ascending arpeggio */
    { 523.25f, 150 },  /* C5 */
    { 659.25f, 150 },  /* E5 */
    { 783.99f, 150 },  /* G5 */
    { 880.00f, 300 },  /* A5 (held) */
    /* Bar 2 — descending return */
    { 783.99f, 150 },  /* G5 */
    { 659.25f, 150 },  /* E5 */
    { 523.25f, 150 },  /* C5 */
    { 392.00f, 300 },  /* G4 (held) */
    /* Bar 3 — call phrase */
    {   0.0f,  150 },  /* rest */
    { 587.33f, 150 },  /* D5 */
    { 659.25f, 150 },  /* E5 */
    { 783.99f, 150 },  /* G5 */
    { 659.25f, 150 },  /* E5 */
    { 587.33f, 150 },  /* D5 */
    /* Bar 4 — resolution */
    { 523.25f, 300 },  /* C5 (held) */
    {   0.0f,  150 },  /* rest — brief pause before loop restarts */
};
#define MELODY_LEN  ((int)(sizeof(MELODY) / sizeof(MELODY[0])))

/* game_music_start — begin (or restart) background music from note 0.
 *
 * Sets music_note_idx = MELODY_LEN - 1 with music_note_samples_left = 0.
 * On the very first sample processed in game_get_audio_samples, the sequencer
 * sees samples_left ≤ 0 → advances to idx 0 → plays the first note.
 * This avoids a special "first frame" branch in the hot path.
 */
void game_music_start(GameAudioState *audio) {
    audio->music_enabled           = 1;
    audio->music_note_idx          = MELODY_LEN - 1; /* next advance → idx 0 */
    audio->music_note_samples_left = 0;              /* force advance on first sample */
    audio->music_fade_in           = 0;
    audio->music_phase             = 0.0f;
    audio->music_frequency         = 0.0f;
    audio->music_volume            = 0.20f;          /* softer than SFX */
}

/* game_music_stop — silence the music channel. */
void game_music_stop(GameAudioState *audio) {
    audio->music_enabled = 0;
}


 /*
 * Trigger a sound effect with a stereo pan position.
 *
 * pan = -1.0  →  full left channel, silent right
 * pan =  0.0  →  equal power in both channels (centre)
 * pan = +1.0  →  full right channel, silent left
 *
 * PAN APPLICATION:
 *   calculate_pan_volumes(pan, &left_vol, &right_vol)  — see audio.h
 *   left_vol  = 0.5 - pan*0.5   → decreases as pan goes right
 *   right_vol = 0.5 + pan*0.5   → increases as pan goes right
 *   At pan=0: left=0.5, right=0.5 (equal power).
 *
 * SLOT ALLOCATION:
 *   Find the first inactive slot in active_sounds[].
 *   If all MAX_SIMULTANEOUS_SOUNDS slots are in use, the new sound is dropped
 *   (graceful degradation — better than stalling the game or corrupting state).
 *
 * total_samples = (samples_per_second * duration_ms) / 1000
 *   Converts millisecond duration to sample count for the current sample rate.
 *
 * fade_in_samples = total_samples / 10
 *   The first 10% of samples ramp up from 0 → full volume.
 *   Eliminates the click/pop that occurs when audio starts at non-zero amplitude.
 */
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound, float pan) {
    int i;
    SoundInstance *inst = NULL;

    if (sound <= SOUND_NONE || sound >= SOUND_COUNT) return;  /* guard */

    /* Find a free slot */
    for (i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
        if (audio->active_sounds[i].samples_remaining <= 0) {
            inst = &audio->active_sounds[i];
            break;
        }
    }
    if (!inst) return;  /* all slots occupied — drop this sound */

    {
        const SoundDef *def = &SOUND_DEFS[sound];
        int total = (audio->samples_per_second * def->duration_ms) / 1000;
        if (total <= 0) return;

        /* Initialise the instance */
        inst->phase              = 0.0f;
        inst->frequency          = def->base_frequency;
        /* frequency_slide: Hz added per sample to reach target_frequency.
         * Pre-computed so the sample loop is just: frequency += frequency_slide */
        inst->frequency_slide    = (def->target_frequency - def->base_frequency)
                                   / (float)total;
        inst->volume             = def->volume;
        inst->total_samples      = total;
        inst->samples_remaining  = total;
        inst->fade_in_samples    = total / 10;  /* first 10% = fade-in */
        inst->pan_position       = pan;         /* stored for per-sample pan calc */
        inst->sound_id           = sound;
    }
}

/* ══════ game_play_sound ═════════════════════════════════════════════════════
 *
 * Convenience wrapper — plays a sound centred (pan = 0.0).
 * Use this when the sound source has no spatial position.
 */
void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
    game_play_sound_at(audio, sound, 0.0f);
}

/* ══════ game_get_audio_samples ══════════════════════════════════════════════
 *
 * Fill an AudioOutputBuffer with mixed PCM samples.
 * Called by the platform once per audio callback or frame.
 *
 * OUTPUT FORMAT: Interleaved stereo int16_t: [L, R, L, R, …]
 * Each pair of samples = one audio frame (one instant in time for both ears).
 *
 * THE SAMPLE LOOP — step by step
 * ──────────────────────────────
 * For each of `buffer->sample_count` stereo frames:
 *   1. For each active SoundInstance:
 *      a. Square wave:
 *           wave = (inst->phase < 0.5f) ? 1.0f : -1.0f
 *         Square waves are simple (no trig), bright-sounding, and audible.
 *      b. Fade-in envelope (avoids click at start):
 *           elapsed = total_samples - samples_remaining
 *           if (elapsed < fade_in_samples)
 *             fade_in = elapsed / (float)fade_in_samples   [0→1]
 *           else
 *             fade_in = 1.0f
 *      c. Fade-out envelope (avoids click at end):
 *           if (samples_remaining < fade_out_samples)  [~10ms]
 *             fade_out = samples_remaining / (float)fade_out_samples  [1→0]
 *           else
 *             fade_out = 1.0f
 *      d. Combined envelope:
 *           env = fade_in * fade_out * inst->volume * audio->sfx_volume
 *      e. Accumulate into left/right mix:
 *           mix_left  += wave * env * inst->pan_left
 *           mix_right += wave * env * inst->pan_right
 *      f. Advance phase:
 *           inst->phase += inst->frequency / (float)audio->samples_per_second
 *           if (inst->phase >= 1.0f) inst->phase -= 1.0f
 *      g. Advance frequency slide (linear interpolation):
 *           freq_step = (target - base) / total_samples
 *           inst->frequency += freq_step  (computed once at play time would be ideal,
 *           but computing per-sample is simpler to read in the course)
 *      h. Decrement samples_remaining; if 0 → slot is free next play
 *   2. Apply master volume and scale to int16 range [−32768, 32767]:
 *        scale = audio->master_volume * 16000.0f
 *        left_sample  = clamp_sample(mix_left  * scale)
 *        right_sample = clamp_sample(mix_right * scale)
 *   3. Write to output buffer:
 *        *out++ = left_sample
 *        *out++ = right_sample
 *
 * clamp_sample(x) is an inline from audio.h that clamps to [-32768, 32767].
 * Without clamping, integer overflow would wrap around and cause crackling.
 *
 * SCALE FACTOR 16000.0f:
 *   int16 max = 32767.  We use 16000 (≈ 49% of max) as a conservative headroom.
 *   This leaves room for mixing multiple sounds simultaneously without clipping.
 *   If you add more simultaneous sounds, reduce this value.
 */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
    GameAudioState *audio = &state->audio;
    int16_t *out     = buffer->samples;
    int      i, s;
    float    fade_out_samples = (float)(audio->samples_per_second / 100); /* ~10ms */

    for (s = 0; s < buffer->sample_count; s++) {
        float mix_left  = 0.0f;
        float mix_right = 0.0f;

        for (i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
            SoundInstance *inst = &audio->active_sounds[i];
            if (inst->samples_remaining <= 0) continue;

            /* Square wave oscillator */
            float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;

            /* Fade-in: ramp from 0→1 over the first fade_in_samples */
            int   elapsed  = inst->total_samples - inst->samples_remaining;
            float fade_in  = (inst->fade_in_samples > 0 && elapsed < inst->fade_in_samples)
                             ? (float)elapsed / (float)inst->fade_in_samples
                             : 1.0f;

            /* Fade-out: ramp from 1→0 over the last ~10ms */
            float fade_out = ((float)inst->samples_remaining < fade_out_samples)
                             ? (float)inst->samples_remaining / fade_out_samples
                             : 1.0f;

            float env = fade_in * fade_out * inst->volume * audio->sfx_volume;

            /* Apply stereo pan from stored pan_position */
            {
                float lv, rv;
                calculate_pan_volumes(inst->pan_position, &lv, &rv);
                mix_left  += wave * env * lv;
                mix_right += wave * env * rv;
            }

            /* Advance oscillator phase */
            inst->phase += inst->frequency / (float)audio->samples_per_second;
            if (inst->phase >= 1.0f) inst->phase -= 1.0f;

            /* Linear frequency slide (pre-computed per-sample Hz change) */
            inst->frequency += inst->frequency_slide;

            inst->samples_remaining--;
        }

        /* ── Music channel ─────────────────────────────────────────────────
         * Separate from SFX pool — doesn't consume a MAX_SIMULTANEOUS_SOUNDS slot.
         * Advances through MELODY[] note-by-note, looping back to index 0.
         *
         * NOTE TRANSITION when music_note_samples_left reaches 0:
         *   1. Advance music_note_idx (wraps with modulo).
         *   2. Load new note: frequency + duration.
         *   3. Reset phase to 0.0 so every note starts at the positive half-cycle.
         *   4. Set music_fade_in = MUSIC_FADE_IN_SAMPLES so the note ramps up
         *      smoothly, hiding the click that would otherwise occur from the
         *      phase reset.
         *
         * RESTS: frequency = 0.0 → phase doesn't advance, no output.
         *   The oscillator stays silent until the rest duration expires.
         */
        if (audio->music_enabled) {
            if (audio->music_note_samples_left <= 0) {
                /* Advance to next note (loop back to 0 after last) */
                audio->music_note_idx = (audio->music_note_idx + 1) % MELODY_LEN;
                audio->music_frequency         = MELODY[audio->music_note_idx].freq;
                audio->music_note_samples_left =
                    (audio->samples_per_second * MELODY[audio->music_note_idx].dur_ms) / 1000;
                if (audio->music_note_samples_left < 1)
                    audio->music_note_samples_left = 1; /* guard: never 0 */
                /* Reset phase at note boundary (see MUSIC_FADE_IN_SAMPLES comment) */
                audio->music_phase   = 0.0f;
                audio->music_fade_in = MUSIC_FADE_IN_SAMPLES;
            }

            if (audio->music_frequency > 0.0f) {
                /* Per-note fade-in: gain = 0 → 1 over MUSIC_FADE_IN_SAMPLES.
                 * When music_fade_in == MUSIC_FADE_IN_SAMPLES, gain = 0.
                 * When music_fade_in == 0,                   gain = 1. */
                float note_fade = (audio->music_fade_in > 0)
                                  ? (1.0f - (float)audio->music_fade_in
                                            / (float)MUSIC_FADE_IN_SAMPLES)
                                  : 1.0f;
                float wave = (audio->music_phase < 0.5f) ? 1.0f : -1.0f;
                float gain = wave * audio->music_volume * note_fade;
                mix_left  += gain;
                mix_right += gain;
            }

            /* Advance oscillator phase for music */
            audio->music_phase += audio->music_frequency / (float)audio->samples_per_second;
            if (audio->music_phase >= 1.0f) audio->music_phase -= 1.0f;

            if (audio->music_fade_in > 0) audio->music_fade_in--;
            audio->music_note_samples_left--;
        }

        /* Scale and write interleaved stereo samples */
        {
            float scale = audio->master_volume * 16000.0f;
            *out++ = clamp_sample(mix_left  * scale);
            *out++ = clamp_sample(mix_right * scale);
        }
    }
}
