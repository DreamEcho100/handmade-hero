/*
 * audio.c  —  Sugar, Sugar | Procedural Audio Engine
 *
 * All synthesis is C + math — no audio file I/O, no platform dependencies.
 *
 * Public surface (declared in game.h):
 *   void game_audio_init(GameAudioState*, int samples_per_second)
 *   void game_audio_update(GameAudioState*, float dt)
 *   void game_play_sound(GameAudioState*, SOUND_ID)
 *   void game_music_play_title(GameAudioState*)
 *   void game_music_play_gameplay(GameAudioState*)
 *   void game_music_stop(GameAudioState*)
 *   void game_get_audio_samples(GameState*, AudioOutputBuffer*)
 *
 * Synthesis rules (following course-builder.md §Audio):
 *   • WAVE_SQUARE   for short event SFX (< 0.3 s)  — digital/retro
 *   • WAVE_TRIANGLE for sustained / music voices    — smooth, no click
 *   • Trapezoidal envelope: short fade-in AND fade-out on every sound
 *   • Music cross-fade: volume_scale ramps 0→1 on start, 1→0 on stop
 *   • Volumes 0.7–1.0 (start high, tune down after confirming audible)
 *
 * JS analogy: game_get_audio_samples() ≈ AudioWorkletProcessor.process().
 * It is called by the platform far more often than game_audio_update()
 * (at 44100 Hz / 2048 spp ≈ ~21× per game frame).  Keep it allocation-free.
 */

#include "game.h"
#include "utils/audio.h"

#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Definitions
 * ═══════════════════════════════════════════════════════════════════════════
 * One entry per SOUND_ID (indexed by SOUND_ID enum value).
 * All values are in SI (Hz, seconds) — NOT samples — so they are
 * sample-rate-independent.
 */
typedef struct {
  float frequency;        /* Hz for the first half of the sound        */
  float frequency_end;    /* Hz for the second half (linear glide to)  */
  float duration;         /* total sound length in seconds             */
  float fade_in_time;     /* attack ramp length in seconds             */
  float fade_out_time;    /* release ramp length in seconds            */
  float volume;           /* peak amplitude [0.0, 1.0]                 */
  float pan;              /* stereo position: -1 left, 0 center, +1 right */
} SoundDef;

/* SOUND_NONE placeholder must occupy index 0 */
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
  /* SOUND_NONE        */ { 0.f,   0.f,   0.f,   0.f,   0.f,   0.f,  0.f },

  /* SOUND_CUP_FILL    — sweet rising chime, like a glass filling with syrup */
  /* Two-step: rise from 660→1320 over 0.25 s (the slide is set below)       */
  [SOUND_CUP_FILL]     = { 660.f, 1320.f, 0.25f, 0.012f, 0.05f, 0.90f, 0.f },

  /* SOUND_LEVEL_COMPLETE — bright 3-note ascending arpeggio                  */
  /* Implemented as one long high tone + slide; simpler than multi-note       */
  [SOUND_LEVEL_COMPLETE] = { 880.f, 1760.f, 0.55f, 0.010f, 0.12f, 1.00f, 0.f },

  /* SOUND_TITLE_SELECT — soft ping on menu hover/select                      */
  [SOUND_TITLE_SELECT] = { 523.f, 523.f,  0.14f, 0.008f, 0.06f, 0.80f, 0.f },

  /* SOUND_GRAVITY_FLIP — descending whoosh when gravity reverses             */
  [SOUND_GRAVITY_FLIP] = { 880.f, 220.f,  0.30f, 0.010f, 0.08f, 0.85f, 0.f },

  /* SOUND_RESET        — gentle soft blip on level restart                   */
  [SOUND_RESET]        = { 330.f, 330.f,  0.12f, 0.010f, 0.05f, 0.75f, 0.f },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Music Patterns — MIDI note numbers
 * ═══════════════════════════════════════════════════════════════════════════
 * 0 = rest.  Notes are MIDI: 60=C4, 62=D4, 64=E4, 67=G4, 69=A4 etc.
 *
 * TITLE MUSIC  — C major pentatonic (C,D,E,G,A), dreamy, ~58 BPM, wide range
 * Notes used: 60=C4, 62=D4, 64=E4, 67=G4, 69=A4, 72=C5, 74=D5, 76=E5
 *
 * GAMEPLAY MUSIC — A minor pentatonic (A,C,D,E,G), upbeat, ~84 BPM
 * Notes used: 57=A3, 60=C4, 62=D4, 64=E4, 67=G4, 69=A4, 72=C5, 74=D5
 */

/* Title — 4 patterns × 16 steps, ~58 BPM (step = 0.26 s) */
static const uint8_t TITLE_PATTERNS[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH] = {
  /* pattern 0: gentle opening arpeggio */
  { 60, 0, 64, 0, 67, 0, 72, 0,  69, 0, 67, 0, 64, 0, 0, 0 },
  /* pattern 1: melodic phrase */
  { 72, 0, 74, 0, 72, 0, 69, 0,  67, 0, 0, 0, 64, 0, 62, 0 },
  /* pattern 2: ascending sparkle */
  { 60, 62, 64, 67, 69, 72, 74, 76,  74, 72, 69, 0, 67, 64, 0, 0 },
  /* pattern 3: resolving phrase */
  { 69, 0, 67, 0, 72, 0, 69, 0,  64, 0, 62, 0, 60, 0, 0, 0 },
};

/* Gameplay — 4 patterns × 16 steps, ~84 BPM (step = 0.18 s) */
static const uint8_t GAMEPLAY_PATTERNS[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH] = {
  /* pattern 0: bouncy A-minor pentatonic hook */
  { 69, 0, 72, 0, 74, 0, 72, 69,  0, 67, 0, 64, 0, 62, 0, 0 },
  /* pattern 1: call-and-response */
  { 62, 64, 67, 0, 69, 0, 72, 0,  74, 0, 72, 69, 67, 0, 0, 0 },
  /* pattern 2: higher energy */
  { 74, 72, 74, 0, 76, 0, 74, 72,  69, 0, 72, 0, 74, 0, 0, 0 },
  /* pattern 3: settling down */
  { 69, 0, 67, 64, 62, 0, 64, 67,  69, 0, 67, 0, 69, 0, 0, 0 },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * game_audio_init  —  called once at startup
 * ═══════════════════════════════════════════════════════════════════════════
 * Zero-initialise then set the sample rate and default volumes.
 * Must be called before any other audio function.
 */
void game_audio_init(GameAudioState *audio, int samples_per_second) {
  memset(audio, 0, sizeof(*audio));
  audio->samples_per_second = samples_per_second;

  audio->master_volume = 0.90f;
  audio->sfx_volume    = 0.85f;
  audio->music_volume  = 0.55f;  /* music bus — comfortable background level */

  audio->active_music  = -1;     /* -1 = silence until game_music_play_* */

  /* Load title sequencer patterns */
  for (int p = 0; p < MUSIC_NUM_PATTERNS; ++p)
    for (int s = 0; s < MUSIC_PATTERN_LENGTH; ++s)
      audio->title_seq.patterns[p][s] = TITLE_PATTERNS[p][s];
  audio->title_seq.step_duration = 0.26f;    /* ~58 BPM */
  audio->title_seq.tone.volume    = 0.70f;
  audio->title_seq.tone.current_volume = 0.0f;
  audio->title_seq.tone.pan_position   = 0.0f;
  audio->title_seq.volume_scale   = 0.0f;

  /* Load gameplay sequencer patterns */
  for (int p = 0; p < MUSIC_NUM_PATTERNS; ++p)
    for (int s = 0; s < MUSIC_PATTERN_LENGTH; ++s)
      audio->gameplay_seq.patterns[p][s] = GAMEPLAY_PATTERNS[p][s];
  audio->gameplay_seq.step_duration = 0.18f; /* ~84 BPM */
  audio->gameplay_seq.tone.volume    = 0.70f;
  audio->gameplay_seq.tone.current_volume = 0.0f;
  audio->gameplay_seq.tone.pan_position   = 0.0f;
  audio->gameplay_seq.volume_scale   = 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_play_sound  —  trigger a one-shot SFX
 * ═══════════════════════════════════════════════════════════════════════════
 * Finds a free slot in active_sounds[], initialises it from SOUND_DEFS[],
 * and lets game_get_audio_samples() mix it in automatically.
 *
 * If all slots are busy the oldest sound is evicted (FIFO-ish: slot 0 is
 * the most recently claimed, so we overwrite slot MAX-1).
 */
void game_play_sound(GameAudioState *audio, SOUND_ID id) {
  if (id <= SOUND_NONE || id >= SOUND_COUNT) return;

  const SoundDef *def = &SOUND_DEFS[id];
  if (def->duration <= 0.f) return;

  /* Find a free slot (samples_remaining == 0 means idle) */
  int slot = -1;
  int oldest = 0;
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; ++i) {
    if (audio->active_sounds[i].samples_remaining <= 0) { slot = i; break; }
    /* track "oldest" as backup eviction target (least samples remaining) */
    if (audio->active_sounds[i].samples_remaining <
        audio->active_sounds[oldest].samples_remaining) oldest = i;
  }
  if (slot < 0) slot = oldest; /* evict oldest if pool is full */

  int total        = (int)(def->duration    * audio->samples_per_second);
  int fade_in      = (int)(def->fade_in_time  * audio->samples_per_second);
  int fade_out     = (int)(def->fade_out_time * audio->samples_per_second);
  float slide_per_sample = (def->frequency_end - def->frequency) / (float)total;

  SoundInstance *s     = &audio->active_sounds[slot];
  s->sound_id          = id;
  s->phase             = 0.0f;  /* reset only at birth */
  s->samples_remaining = total;
  s->total_samples     = total;
  s->fade_in_samples   = fade_in;
  s->fade_out_samples  = fade_out;
  s->volume            = def->volume;
  s->frequency         = def->frequency;
  s->frequency_slide   = slide_per_sample;
  s->pan_position      = def->pan;
}

/* Pan-positioned variant — same as game_play_sound but overrides pan */
void game_play_sound_at(GameAudioState *audio, SOUND_ID id, float pan) {
  game_play_sound(audio, id);
  /* Overwrite pan on the slot we just claimed */
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; ++i) {
    if (audio->active_sounds[i].sound_id == id &&
        audio->active_sounds[i].samples_remaining > 0) {
      audio->active_sounds[i].pan_position = pan;
      break;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Music control helpers
 * ═══════════════════════════════════════════════════════════════════════════
 * volume_scale is ramped (not snapped) in game_audio_update to avoid clicks.
 */
void game_music_play_title(GameAudioState *audio) {
  if (!audio->title_seq.is_playing) {
    audio->title_seq.is_playing    = 1;
    audio->title_seq.current_pattern = 0;
    audio->title_seq.current_step    = 0;
    audio->title_seq.step_timer      = 0.0f;
    /* volume_scale ramps up in game_audio_update */
  }
  audio->active_music = 0;
  /* Request gameplay to fade out */
  audio->gameplay_seq.is_playing = 0;
}

void game_music_play_gameplay(GameAudioState *audio) {
  if (!audio->gameplay_seq.is_playing) {
    audio->gameplay_seq.is_playing    = 1;
    audio->gameplay_seq.current_pattern = 0;
    audio->gameplay_seq.current_step    = 0;
    audio->gameplay_seq.step_timer      = 0.0f;
  }
  audio->active_music = 1;
  audio->title_seq.is_playing = 0;
}

void game_music_stop(GameAudioState *audio) {
  audio->title_seq.is_playing    = 0;
  audio->gameplay_seq.is_playing = 0;
  audio->active_music = -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_audio_update  —  advance sequencer (called once per game frame)
 * ═══════════════════════════════════════════════════════════════════════════
 * Advances the active music sequencer by dt seconds.
 * Also smoothly ramps volume_scale for cross-fading between tracks.
 *
 * This is game-frame-rate work only (≈60 Hz).
 * Synthesis is done in game_get_audio_samples() at audio-buffer rate.
 */
static void update_sequencer(MusicSequencer *seq, float dt) {
  /* Fade volume_scale toward target: 1.0 if playing, 0.0 if stopped */
  float target = seq->is_playing ? 1.0f : 0.0f;
  float rate   = dt * 3.0f;  /* full fade in/out in ~0.33 s */
  if (seq->volume_scale < target) {
    seq->volume_scale += rate;
    if (seq->volume_scale > 1.0f) seq->volume_scale = 1.0f;
  } else if (seq->volume_scale > target) {
    seq->volume_scale -= rate;
    if (seq->volume_scale < 0.0f) seq->volume_scale = 0.0f;
  }

  if (!seq->is_playing) return;

  seq->step_timer += dt;
  while (seq->step_timer >= seq->step_duration) {
    seq->step_timer -= seq->step_duration;
    seq->current_step++;
    if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
      seq->current_step = 0;
      seq->current_pattern = (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
    }
    uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];
    if (note == 0) {
      seq->tone.is_playing = 0;
    } else {
      seq->tone.frequency  = midi_to_freq(note);
      seq->tone.is_playing = 1;
    }
  }
}

void game_audio_update(GameAudioState *audio, float dt) {
  update_sequencer(&audio->title_seq,    dt);
  update_sequencer(&audio->gameplay_seq, dt);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_get_audio_samples  —  the hot path (audio-buffer rate, ~21× per frame)
 * ═══════════════════════════════════════════════════════════════════════════
 * Fills buf->samples with interleaved stereo int16 samples.
 * Called by the platform (Raylib AudioStream callback or ALSA write loop)
 * independently from the game-update loop.
 *
 * The function is entirely allocation-free.  All state is in GameAudioState.
 *
 * CRITICAL: phase must be advanced continuously — never reset between calls.
 * Resetting phase causes a discontinuity (audible click) in the waveform.
 */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buf) {
  GameAudioState *audio = &state->audio;
  float master = audio->master_volume;
  float spsi   = 1.0f / (float)buf->samples_per_second; /* seconds per sample */

  for (int i = 0; i < buf->sample_count; ++i) {
    float mix_l = 0.0f, mix_r = 0.0f;

    /* ── Sound Effects (square wave) ──────────────────────────────────────── */
    float sfx_scale = audio->sfx_volume * master;
    for (int si = 0; si < MAX_SIMULTANEOUS_SOUNDS; ++si) {
      SoundInstance *s = &audio->active_sounds[si];
      if (s->samples_remaining <= 0) continue;

      /* Trapezoidal envelope: fade-in attack, sustain, fade-out release */
      int elapsed = s->total_samples - s->samples_remaining;
      float env   = 1.0f;
      if (elapsed < s->fade_in_samples && s->fade_in_samples > 0)
        env = (float)elapsed / (float)s->fade_in_samples;
      else if (s->samples_remaining < s->fade_out_samples && s->fade_out_samples > 0)
        env = (float)s->samples_remaining / (float)s->fade_out_samples;

      /* Square wave: phase < 0.5 → +1, else → -1 */
      float wave = (s->phase < 0.5f) ? 1.0f : -1.0f;
      float amp  = wave * env * s->volume * sfx_scale * 32767.0f;

      float pan_l, pan_r;
      calculate_pan_volumes(s->pan_position, &pan_l, &pan_r);
      mix_l += amp * pan_l;
      mix_r += amp * pan_r;

      /* Advance oscillator — NEVER reset phase between samples or calls */
      s->phase += s->frequency * spsi;
      if (s->phase >= 1.0f) s->phase -= 1.0f;

      /* Apply frequency slide (linear pitch glide) */
      s->frequency += s->frequency_slide;

      s->samples_remaining--;
    }

    /* ── Music voices (triangle wave) ────────────────────────────────────── */
    float music_scale = audio->music_volume * master;

    /* Helper lambda-style macro to mix one sequencer's tone */
    #define MIX_MUSIC_VOICE(seq) do { \
      ToneGenerator *t = &(seq).tone; \
      float target_vol = (t->is_playing) ? t->volume * (seq).volume_scale : 0.0f; \
      /* Smooth volume ramp — prevents click on note-on/note-off */ \
      float vol_step = spsi * 80.0f; /* ~12 ms to full volume */ \
      if (t->current_volume < target_vol) { \
        t->current_volume += vol_step; \
        if (t->current_volume > target_vol) t->current_volume = target_vol; \
      } else if (t->current_volume > target_vol) { \
        t->current_volume -= vol_step; \
        if (t->current_volume < target_vol) t->current_volume = target_vol; \
      } \
      if (t->current_volume > 0.0001f && t->frequency > 0.0f) { \
        float wave = wave_triangle(t->phase); \
        float amp  = wave * t->current_volume * music_scale * 32767.0f; \
        float pan_l, pan_r; \
        calculate_pan_volumes(t->pan_position, &pan_l, &pan_r); \
        mix_l += amp * pan_l; \
        mix_r += amp * pan_r; \
        t->phase += t->frequency * spsi; \
        if (t->phase >= 1.0f) t->phase -= 1.0f; \
      } \
    } while(0)

    MIX_MUSIC_VOICE(audio->title_seq);
    MIX_MUSIC_VOICE(audio->gameplay_seq);
    #undef MIX_MUSIC_VOICE

    /* Write stereo pair */
    buf->samples[i * 2 + 0] = clamp_sample(mix_l);
    buf->samples[i * 2 + 1] = clamp_sample(mix_r);
  }
}
