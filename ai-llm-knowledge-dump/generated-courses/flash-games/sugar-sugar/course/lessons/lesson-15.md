# Lesson 15: Audio System — Procedural Sound Effects, Music Sequencer, and Platform Backends

## What we're building

Add the entire audio system in two parts:

**Part A — Sound Effects & Music Core (`utils/audio.h` + `audio.c`)**
- Procedural synthesis engine: square-wave SFX, triangle-wave music
- Trapezoidal amplitude envelope (click-free attack and release)
- Linear pitch glide (frequency slide)
- Pattern-based music sequencer with two tracks (title / gameplay)
- Cross-fade between tracks via ramped `volume_scale`

**Part B — Platform Backends (`main_raylib.c` and `main_x11.c`)**
- Raylib `AudioStream` approach: `AUDIO_CHUNK_SIZE` must be identical in two places
- X11/ALSA: `snd_pcm_hw_params` (not `snd_pcm_set_params`), Casey latency model, `start_threshold=1`
- **Critical init-order rule**: why `game_music_play_title()` must be called AFTER `platform_audio_init()`

## What you'll learn

- The phase-accumulator oscillator — why never resetting phase is essential
- Trapezoidal envelope — fade-in + sustain + fade-out
- MIDI note-number → Hz formula
- Constant-power stereo panning
- The `MusicSequencer` and `ToneGenerator` types
- Why `game_get_audio_samples()` is called ~21× per frame but `game_audio_update()` once per frame
- The `AUDIO_CHUNK_SIZE` mismatch trap in Raylib
- ALSA hardware parameter setup vs. the deprecated one-liner
- Why `memset` in `game_audio_init` destroys music state set in `change_phase`

## Prerequisites

- Lesson 14 complete (full game rendering and phase flow working)

---

## Part A — Sound Effects and Music Core

### Step A-1: `utils/audio.h` — complete type walkthrough

`utils/audio.h` defines all audio types and is included by both `game.h` and `audio.c`.
No platform code touches it directly.

```c
/* src/utils/audio.h  —  full file (first introduction) */
#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H
#include <math.h>
#include <stdint.h>

/* ---- AudioOutputBuffer ----
 * Platform allocates this struct on the stack, fills .samples and .sample_count,
 * then passes it to game_get_audio_samples().  The game writes stereo int16 pairs. */
typedef struct {
  int16_t *samples;       /* interleaved stereo: L0,R0,L1,R1,...  */
  int samples_per_second; /* typically 44100                      */
  int sample_count;       /* stereo pairs to generate this call   */
} AudioOutputBuffer;

/* ---- SOUND_ID enum ----
 * Named by intent, not by arbitrary numbers.  Adding a new sound means
 * adding one entry here and one SoundDef row in audio.c. */
typedef enum {
  SOUND_NONE = 0,
  SOUND_CUP_FILL,        /* rising chime — cup reaches 100%       */
  SOUND_LEVEL_COMPLETE,  /* bright fanfare — all cups filled       */
  SOUND_TITLE_SELECT,    /* soft ping — level selected on menu     */
  SOUND_GRAVITY_FLIP,    /* descending whoosh — gravity reversed   */
  SOUND_RESET,           /* gentle blip — level restarted          */
  SOUND_COUNT
} SOUND_ID;

/* ---- SoundInstance ---- (one slot in the active-sounds pool)
 * CRITICAL: phase must NEVER be reset between audio buffer fills.
 * A phase discontinuity creates a polarity jump → audible click. */
#define MAX_SIMULTANEOUS_SOUNDS 8
typedef struct {
  SOUND_ID sound_id;
  float    phase;              /* oscillator phase [0.0, 1.0) — never reset */
  int      samples_remaining;  /* counts down to 0 when sound ends          */
  int      total_samples;      /* samples at birth (for envelope ratio)     */
  int      fade_in_samples;    /* attack ramp length (~10 ms)               */
  int      fade_out_samples;   /* release ramp length (~10 ms)              */
  float    volume;             /* peak amplitude [0.0, 1.0]                 */
  float    frequency;          /* current pitch in Hz                       */
  float    frequency_slide;    /* Hz added per sample (linear pitch glide)  */
  float    pan_position;       /* -1 = full left, 0 = center, +1 = full right */
} SoundInstance;

/* ---- ToneGenerator ---- (one continuous music voice)
 * current_volume is a smoothed copy of volume — it ramps each sample to
 * prevent clicks on note-on / note-off transitions. */
typedef struct {
  float phase;          /* oscillator phase [0.0, 1.0)              */
  float frequency;      /* target pitch in Hz                       */
  float volume;         /* target amplitude [0.0, 1.0]              */
  float current_volume; /* smoothed (click-free) amplitude          */
  float pan_position;   /* stereo position                          */
  int   is_playing;     /* 0 = note off (ramps current_volume to 0) */
} ToneGenerator;

/* ---- MusicSequencer ---- (pattern-based note trigger)
 * Loops through MUSIC_NUM_PATTERNS × MUSIC_PATTERN_LENGTH steps.
 * Each step is a MIDI note (0 = rest).
 * volume_scale smoothly fades in/out when switching between tracks. */
#define MUSIC_PATTERN_LENGTH 16
#define MUSIC_NUM_PATTERNS    4
typedef struct {
  uint8_t       patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
  int           current_pattern;
  int           current_step;
  float         step_timer;       /* accumulated delta_time            */
  float         step_duration;    /* seconds per step                  */
  ToneGenerator tone;
  int           is_playing;
  float         volume_scale;     /* 0.0→1.0 for cross-fade            */
} MusicSequencer;

/* ---- GameAudioState ---- (embedded in GameState) */
typedef struct {
  SoundInstance  active_sounds[MAX_SIMULTANEOUS_SOUNDS];
  MusicSequencer title_seq;    /* dreamy ambient    — PHASE_TITLE    */
  MusicSequencer gameplay_seq; /* upbeat pentatonic — PHASE_PLAYING  */
  int            active_music; /* 0=title, 1=gameplay, -1=silence    */
  float master_volume;
  float sfx_volume;
  float music_volume;
  int   samples_per_second;
} GameAudioState;

/* ---- Inline math helpers ---- */

/* MIDI note 60 = C4 = 261.6 Hz, note 69 = A4 = 440 Hz */
static inline float midi_to_freq(int note) {
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

static inline int16_t clamp_sample(float s) {
  if (s >  32767.0f) return  32767;
  if (s < -32768.0f) return -32768;
  return (int16_t)s;
}

/* Linear pan: -1 = full left, 0 = center, +1 = full right.
 * Simple (not constant-power) — adequate for retro-style game audio. */
static inline void calculate_pan_volumes(float pan, float *left, float *right) {
  if (pan <= 0.0f) { *left = 1.0f; *right = 1.0f + pan; }
  else             { *left = 1.0f - pan; *right = 1.0f;  }
}

/* Triangle wave: smooth, no polarity flip — use for sustained/music voices.
 * Square wave (phase < 0.5 ? +1 : -1) is used for short event SFX instead. */
static inline float wave_triangle(float phase) {
  return 1.0f - fabsf(4.0f * phase - 2.0f);
}

#endif /* UTILS_AUDIO_H */
```

**JS analogy:**

| C type | JS equivalent |
|---|---|
| `GameAudioState` | The internal state object of an `AudioWorkletProcessor` |
| `SoundInstance` | One entry in a `Map` of currently playing `OscillatorNode`s |
| `ToneGenerator` | A `ConstantSourceNode` controlling an `OscillatorNode`'s gain |
| `game_get_audio_samples()` | `AudioWorkletProcessor.process(inputs, outputs)` |
| `game_audio_update()` | `requestAnimationFrame` callback advancing sequencer state |

---

### Step A-2: Sound definitions (`SOUND_DEFS` table)

```c
/* src/audio.c  —  SoundDef table */

typedef struct {
  float frequency;     /* Hz at sound start                      */
  float frequency_end; /* Hz at sound end (linear glide target)  */
  float duration;      /* total length in seconds                */
  float fade_in_time;  /* attack in seconds                      */
  float fade_out_time; /* release in seconds                     */
  float volume;        /* peak amplitude [0.0, 1.0]              */
  float pan;           /* -1 left, 0 center, +1 right            */
} SoundDef;

static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
  /* SOUND_NONE             */ { 0.f,   0.f,   0.f,   0.f,   0.f,   0.f,  0.f },

  /* Cup fill: rising chime, 660→1320 Hz over 0.25 s */
  [SOUND_CUP_FILL]          = { 660.f, 1320.f, 0.25f, 0.012f, 0.05f, 0.90f, 0.f },

  /* Level complete: bright high sweep 880→1760 Hz, 0.55 s */
  [SOUND_LEVEL_COMPLETE]    = { 880.f, 1760.f, 0.55f, 0.010f, 0.12f, 1.00f, 0.f },

  /* Title select: short ping at C5 (523 Hz) */
  [SOUND_TITLE_SELECT]      = { 523.f, 523.f,  0.14f, 0.008f, 0.06f, 0.80f, 0.f },

  /* Gravity flip: descending whoosh 880→220 Hz */
  [SOUND_GRAVITY_FLIP]      = { 880.f, 220.f,  0.30f, 0.010f, 0.08f, 0.85f, 0.f },

  /* Reset: gentle blip at E2 (330 Hz) */
  [SOUND_RESET]             = { 330.f, 330.f,  0.12f, 0.010f, 0.05f, 0.75f, 0.f },
};
```

---

### Step A-3: `game_play_sound` — triggering a SFX

```c
/* src/audio.c  —  game_play_sound */

void game_play_sound(GameAudioState *audio, SOUND_ID id) {
  if (id <= SOUND_NONE || id >= SOUND_COUNT) return;
  const SoundDef *def = &SOUND_DEFS[id];
  if (def->duration <= 0.f) return;

  /* Find a free slot; evict the least-remaining if pool is full */
  int slot = -1, oldest = 0;
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; ++i) {
    if (audio->active_sounds[i].samples_remaining <= 0) { slot = i; break; }
    if (audio->active_sounds[i].samples_remaining <
        audio->active_sounds[oldest].samples_remaining) oldest = i;
  }
  if (slot < 0) slot = oldest;

  int total    = (int)(def->duration    * audio->samples_per_second);
  int fade_in  = (int)(def->fade_in_time  * audio->samples_per_second);
  int fade_out = (int)(def->fade_out_time * audio->samples_per_second);

  SoundInstance *s    = &audio->active_sounds[slot];
  s->sound_id         = id;
  s->phase            = 0.0f;   /* only reset at birth — never between frames */
  s->samples_remaining = total;
  s->total_samples    = total;
  s->fade_in_samples  = fade_in;
  s->fade_out_samples = fade_out;
  s->volume           = def->volume;
  s->frequency        = def->frequency;
  s->frequency_slide  = (def->frequency_end - def->frequency) / (float)total;
  s->pan_position     = def->pan;
}
```

---

### Step A-4: `game_get_audio_samples` — the synthesis hot path

This function runs at **audio-buffer rate** — approximately 21× per game frame at
44 100 Hz / 2048 samples.  It must be allocation-free and pure math.

```c
/* src/audio.c  —  game_get_audio_samples */

void game_get_audio_samples(GameState *state, AudioOutputBuffer *buf) {
  GameAudioState *audio = &state->audio;
  float master = audio->master_volume;
  float spsi   = 1.0f / (float)buf->samples_per_second; /* 1/44100 ≈ 22.7 µs */

  for (int i = 0; i < buf->sample_count; ++i) {
    float mix_l = 0.0f, mix_r = 0.0f;

    /* ── SFX: square wave + trapezoidal envelope ─────────────────────────── */
    float sfx_scale = audio->sfx_volume * master;
    for (int si = 0; si < MAX_SIMULTANEOUS_SOUNDS; ++si) {
      SoundInstance *s = &audio->active_sounds[si];
      if (s->samples_remaining <= 0) continue;

      /* Trapezoidal envelope.
       *   elapsed < fade_in  → ramp 0→1 (attack)
       *   remaining < fade_out → ramp 1→0 (release)
       *   otherwise           → 1.0 (sustain) */
      int elapsed = s->total_samples - s->samples_remaining;
      float env = 1.0f;
      if (elapsed < s->fade_in_samples && s->fade_in_samples > 0)
        env = (float)elapsed / (float)s->fade_in_samples;
      else if (s->samples_remaining < s->fade_out_samples && s->fade_out_samples > 0)
        env = (float)s->samples_remaining / (float)s->fade_out_samples;

      /* Square wave: phase in [0,0.5) → +1; [0.5,1) → -1 */
      float wave = (s->phase < 0.5f) ? 1.0f : -1.0f;
      float amp  = wave * env * s->volume * sfx_scale * 32767.0f;

      float pan_l, pan_r;
      calculate_pan_volumes(s->pan_position, &pan_l, &pan_r);
      mix_l += amp * pan_l;
      mix_r += amp * pan_r;

      /* Phase accumulator — NEVER reset between calls */
      s->phase += s->frequency * spsi;
      if (s->phase >= 1.0f) s->phase -= 1.0f;

      /* Linear frequency slide (pitch glide) */
      s->frequency += s->frequency_slide;

      s->samples_remaining--;
    }

    /* ── Music: triangle wave + smooth volume ramp ───────────────────────── */
    float music_scale = audio->music_volume * master;

    /* Macro to mix one MusicSequencer's single ToneGenerator voice.
     * The macro avoids duplicating the 20-line block for title and gameplay. */
    #define MIX_MUSIC_VOICE(seq) do {                                       \
      ToneGenerator *t = &(seq).tone;                                       \
      float target_vol = t->is_playing ? t->volume * (seq).volume_scale     \
                                       : 0.0f;                              \
      /* Ramp ~12 ms to target — prevents click on note on/off */           \
      float vol_step = spsi * 80.0f;                                        \
      if      (t->current_volume < target_vol)                              \
        { t->current_volume += vol_step; if (t->current_volume > target_vol) t->current_volume = target_vol; } \
      else if (t->current_volume > target_vol)                              \
        { t->current_volume -= vol_step; if (t->current_volume < target_vol) t->current_volume = target_vol; } \
      if (t->current_volume > 0.0001f && t->frequency > 0.0f) {            \
        float wave = wave_triangle(t->phase);                               \
        float amp  = wave * t->current_volume * music_scale * 32767.0f;    \
        float pan_l, pan_r;                                                 \
        calculate_pan_volumes(t->pan_position, &pan_l, &pan_r);            \
        mix_l += amp * pan_l;                                               \
        mix_r += amp * pan_r;                                               \
        t->phase += t->frequency * spsi;                                    \
        if (t->phase >= 1.0f) t->phase -= 1.0f;                            \
      }                                                                     \
    } while(0)

    MIX_MUSIC_VOICE(audio->title_seq);
    MIX_MUSIC_VOICE(audio->gameplay_seq);
    #undef MIX_MUSIC_VOICE

    /* Write interleaved stereo int16 pair */
    buf->samples[i * 2 + 0] = clamp_sample(mix_l);
    buf->samples[i * 2 + 1] = clamp_sample(mix_r);
  }
}
```

**Why `spsi` instead of `1.0f / 44100.0f` hardcoded?**  
The platform initialises `samples_per_second` at runtime; the game never hardcodes
the sample rate.  This means the same `audio.c` works at 44 100 Hz (Raylib) and
48 000 Hz (some ALSA configurations) without recompiling.

---

### Step A-5: `game_audio_update` — sequencer advance (once per game frame)

```c
/* src/audio.c  —  update_sequencer + game_audio_update */

static void update_sequencer(MusicSequencer *seq, float dt) {
  /* Ramp volume_scale toward target (1.0 = playing, 0.0 = stopped).
   * Rate = 3× per second → full fade in ~0.33 s. */
  float target = seq->is_playing ? 1.0f : 0.0f;
  float rate   = dt * 3.0f;
  seq->volume_scale += (target - seq->volume_scale > 0)
    ? fminf(rate,  target - seq->volume_scale)
    : fmaxf(-rate, target - seq->volume_scale);

  if (!seq->is_playing) return;

  /* Advance step counter */
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
```

---

## Part B — Platform Audio Backends

### Step B-1: Raylib backend (`main_raylib.c`)

**The `AUDIO_CHUNK_SIZE` contract — the most common Raylib audio bug:**

```c
/* src/main_raylib.c  —  critical excerpt */

/* AUDIO_CHUNK_SIZE must be used in EXACTLY two places:
 *   1. SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE)  ← registers segment size
 *   2. UpdateAudioStream(stream, buf, AUDIO_CHUNK_SIZE)   ← fills that segment
 *
 * If the two values differ, Raylib writes past the end of its internal ring buffer
 * → heap corruption → crashes or silent garbled audio. */
#define AUDIO_CHUNK_SIZE  2048

static int16_t g_sample_buffer[AUDIO_CHUNK_SIZE * 4]; /* 4× stereo headroom */

void platform_audio_init(GameState *state, int samples_per_second) {
  game_audio_init(&state->audio, samples_per_second);

  InitAudioDevice();

  /* MUST call SetAudioStreamBufferSizeDefault BEFORE LoadAudioStream */
  SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);

  g_stream = LoadAudioStream(samples_per_second, 16, 2); /* 44100, 16-bit, stereo */
  PlayAudioStream(g_stream);

  /* Pre-fill two silent chunks to prevent an underrun on the very first frame */
  memset(g_sample_buffer, 0, sizeof(int16_t) * AUDIO_CHUNK_SIZE * 2);
  UpdateAudioStream(g_stream, g_sample_buffer, AUDIO_CHUNK_SIZE);
  UpdateAudioStream(g_stream, g_sample_buffer, AUDIO_CHUNK_SIZE);
}

/* Per-frame audio push: */
while (IsAudioStreamProcessed(g_stream)) {
  AudioOutputBuffer out_buf = {
    .samples            = g_sample_buffer,
    .samples_per_second = samples_per_second,
    .sample_count       = AUDIO_CHUNK_SIZE,
  };
  game_get_audio_samples(&state, &out_buf);
  /* Always pass AUDIO_CHUNK_SIZE — never a different count */
  UpdateAudioStream(g_stream, g_sample_buffer, AUDIO_CHUNK_SIZE);
}
```

---

### Step B-2: X11/ALSA backend (`main_x11.c`)

**Why `snd_pcm_hw_params` instead of `snd_pcm_set_params`:**

`snd_pcm_set_params` is a one-liner shortcut but it sets `buffer_size = period_size * 4`
internally and provides no way to tune latency.  Using `snd_pcm_hw_params` directly
gives full control over buffer size, period size, and the start threshold.

```c
/* src/main_x11.c  —  ALSA includes (order matters) */

/* MUST include <alloca.h> before <alsa/asoundlib.h>.
 * snd_pcm_hw_params_alloca / snd_pcm_sw_params_alloca are macros that expand
 * to alloca() calls — clang does not implicitly declare alloca. */
#include <alloca.h>
#include <alsa/asoundlib.h>

#define ALSA_SAMPLES_PER_SEC  44100
#define ALSA_GAME_HZ           60
#define ALSA_FRAMES_PER_FRAME  (ALSA_SAMPLES_PER_SEC / ALSA_GAME_HZ)  /* 735 */
#define ALSA_LATENCY_FRAMES    (ALSA_FRAMES_PER_FRAME * 4)             /* ~66 ms */

void platform_audio_init(GameState *state, int samples_per_second) {
  game_audio_init(&state->audio, ALSA_SAMPLES_PER_SEC);

  snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);

  /* Hardware parameters */
  snd_pcm_hw_params_t *hw;
  snd_pcm_hw_params_alloca(&hw);              /* stack alloc via macro */
  snd_pcm_hw_params_any(g_pcm, hw);           /* fill with any/all capabilities */

  snd_pcm_hw_params_set_access(g_pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(g_pcm, hw, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(g_pcm, hw, 2);

  unsigned int rate = ALSA_SAMPLES_PER_SEC;
  snd_pcm_hw_params_set_rate_near(g_pcm, hw, &rate, 0);

  snd_pcm_uframes_t buf_frames = ALSA_LATENCY_FRAMES * 4;
  snd_pcm_hw_params_set_buffer_size_near(g_pcm, hw, &buf_frames);

  snd_pcm_uframes_t period_frames = ALSA_FRAMES_PER_FRAME;
  snd_pcm_hw_params_set_period_size_near(g_pcm, hw, &period_frames, 0);

  snd_pcm_hw_params(g_pcm, hw);  /* commit */

  /* Software parameters: start_threshold = 1 means playback starts
   * as soon as 1 frame is written (no waiting for a full-buffer fill).
   * Without this, the stream can hang silently on the first write. */
  snd_pcm_sw_params_t *sw;
  snd_pcm_sw_params_alloca(&sw);
  snd_pcm_sw_params_current(g_pcm, sw);
  snd_pcm_sw_params_set_start_threshold(g_pcm, sw, 1);
  snd_pcm_sw_params(g_pcm, sw);
}
```

**Casey latency model — per-frame audio write:**

```c
/* src/main_x11.c  —  per-frame ALSA write (Casey model) */

/* snd_pcm_delay tells us how many frames are already queued in the driver.
 * We write exactly (LATENCY_TARGET - delay) frames to keep the buffer at
 * the target level without over-filling (which causes drift) or under-filling
 * (which causes underruns / clicks). */
snd_pcm_sframes_t delay;
int err = snd_pcm_delay(g_pcm, &delay);
if (err < 0) delay = 0;

snd_pcm_sframes_t target = ALSA_LATENCY_FRAMES;
snd_pcm_sframes_t to_write = target - delay;
if (to_write < ALSA_FRAMES_PER_FRAME / 4) to_write = 0; /* skip tiny writes */
if (to_write > ALSA_SAMPLE_BUF_SIZE)      to_write = ALSA_SAMPLE_BUF_SIZE;

if (to_write > 0) {
  AudioOutputBuffer out_buf = {
    .samples            = g_alsa_buf,
    .samples_per_second = ALSA_SAMPLES_PER_SEC,
    .sample_count       = (int)to_write,
  };
  game_get_audio_samples(&state, &out_buf);

  snd_pcm_sframes_t written = snd_pcm_writei(g_pcm, g_alsa_buf,
                                              (snd_pcm_uframes_t)to_write);
  if (written < 0) {
    /* Underrun recovery: snd_pcm_recover attempts to restart the stream */
    snd_pcm_recover(g_pcm, (int)written, 1);
    /* Re-prefill with silence to prevent click on the resumed stream */
    int prefill = ALSA_LATENCY_FRAMES;
    memset(g_alsa_buf, 0, (size_t)prefill * 2 * sizeof(int16_t));
    snd_pcm_writei(g_pcm, g_alsa_buf, (snd_pcm_uframes_t)prefill);
  }
}
```

---

### Step B-3: Critical init-order rule

This is the most subtle bug in the entire codebase.  Understanding it is key to
avoiding silent audio at startup.

```
Timeline (WRONG — title music is silent until you enter a level and come back):
  main() calls:
    1. game_init()
         └─ change_phase(PHASE_TITLE)
              └─ game_music_play_title()  → sets title_seq.is_playing = 1 ✓
    2. platform_audio_init()
         └─ game_audio_init()
              └─ memset(audio, 0, sizeof(*audio))  ← WIPES is_playing back to 0 ✗
```

```
Timeline (CORRECT — music plays immediately on launch):
  main() calls:
    1. game_init()
         └─ change_phase(PHASE_TITLE)
              └─ game_music_play_title()  → sets is_playing = 1 (temporary)
    2. platform_audio_init()
         └─ game_audio_init()
              └─ memset(audio, 0, ...)   ← wipes is_playing = 0
    3. game_music_play_title()           ← set AGAIN after init — now it sticks ✓
```

Both `main_raylib.c` and `main_x11.c` must call `game_music_play_title` after
`platform_audio_init`:

```c
/* src/main_raylib.c  and  src/main_x11.c  —  identical section */

game_init(&state);
platform_audio_init(&state, SAMPLES_PER_SECOND);
/* Reset title music AFTER audio init — game_audio_init's memset wipes the
 * is_playing flag that game_init set via change_phase(PHASE_TITLE). */
game_music_play_title(&state.audio);
```

---

## Build and run (both backends)

```bash
# Raylib
./build-dev.sh --backend=raylib -r

# X11 (requires ALSA: -DALSA_AVAILABLE -lasound handled by build-dev.sh)
./build-dev.sh --backend=x11 -r
```

**Expected audio behavior:**
- Dreamy pentatonic melody plays immediately on the title screen (no delay).
- Music cross-fades to the upbeat gameplay melody on level start.
- Cup fill plays a rising chime; level complete plays a bright sweep.
- Gravity flip plays a descending whoosh; reset plays a soft blip.
- No clicks, pops, or stuttering on either backend.

---

## Exercises

1. **Add a new SFX**: `SOUND_CUP_HALFWAY` that plays when a cup reaches 50%.  Add the enum entry, the `SoundDef` row, and the call site in `update_grains`.
2. **Tune the title music tempo**: Change `step_duration` from `0.26f` to `0.20f` and observe the BPM change.  What BPM does `0.20f` correspond to?
3. **Verify the init-order rule**: Temporarily move the `game_music_play_title` call to before `platform_audio_init`.  Launch the game, notice the silence, then restore the correct order.
4. **ALSA underrun test**: Add `sleep(1)` inside the ALSA write loop (X11 backend only) to force an underrun.  The recovery code should resume audio after a brief gap — verify it does not crash.

---

## What's next

You have now built the complete Sugar, Sugar game from scratch — a full particle
physics engine, procedural audio system, level progression, and a cross-platform
rendering and input layer.

If you want to go further, consider:
- **Persistence**: save `unlocked_count` to a file so progress survives restarts
- **More levels**: add 10–20 more levels using the existing `LevelDef` system
- **New grain color**: add `GRAIN_BLUE` and a blue filter
- **Particle effects**: spawn a short burst of colored pixels when a cup fills
- **A mobile/WebAssembly port**: the platform contract (`platform.h`) makes this straightforward
