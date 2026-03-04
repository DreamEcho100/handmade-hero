# Lesson 15 — Procedural Audio System and Final Polish

## What you build

You wire up a full procedural audio system modelled after Casey Muratori's
Handmade Hero approach:

- **`src/utils/audio.h`** — shared audio types: `GameAudioState`, `SoundInstance`,
  `ToneGenerator`, `MusicSequencer`, `AudioOutputBuffer`
- **`src/audio.c`** — synthesis engine: `game_get_audio_samples()`, `game_play_sound()`,
  `game_audio_update()`, Sugar Sugar sound definitions, pentatonic music patterns
- **Raylib backend** — `AudioStream` + `IsAudioStreamProcessed` per-frame fill loop
- **X11 backend** — ALSA `snd_pcm_writei` (enable with `-DALSA_AVAILABLE`)
- **`change_phase()` audio wiring** — every phase transition triggers the right sound/music
- **Cup fill trigger** — `SOUND_CUP_FILL` fires exactly when a cup reaches 100%
- **Mouse letterbox correction** — Raylib maps window mouse coords to canvas coords
- **`DEBUG_TRAP` / `ASSERT` macros** — zero-cost in release, crash-on-fail in debug

## Concepts introduced

- `GameAudioState` embedded in `GameState` (no global audio object)
- `SoundInstance` pool — `MAX_SIMULTANEOUS_SOUNDS = 4` active at once
- Square-wave oscillator with linear pitch glide (`frequency_slide`)
- Per-sample fade-in + fade-out envelope (prevents clicks)
- Constant-power stereo pan (`calculate_pan_volumes`)
- `MusicSequencer` — 4 patterns × 16 steps; MIDI note → Hz via `midi_to_freq()`
- Volume ramping (`current_volume` interpolates toward `target`) for click-free note changes
- `AudioStream` model: platform asks game for samples, not the reverse
- `SAMPLES_PER_FRAME = 2048` — why 735 causes wavy/echoing audio
- Mouse letterbox transform: `(mouse_window - offset) / scale`

## JS → C analogy

| JavaScript                                              | C                                                    |
|---------------------------------------------------------|------------------------------------------------------|
| `AudioWorkletProcessor.process(inputs, outputs)`        | `game_get_audio_samples(state, buffer)`              |
| `oscillator.frequency.value = freq`                     | `inst->frequency = freq`                            |
| `gainNode.gain.setTargetAtTime(0, now, 0.01)`           | `tone->current_volume` ramped by `VOLUME_RAMP_SPEED`|
| `Sequencer.step()` advancing each 16th note             | `game_audio_update()` called once per game frame     |
| `audioCtx.createOscillator()` → `start()` → `stop()`   | `game_play_sound(&state->audio, SOUND_X)`           |

---

## Step 1: `src/utils/audio.h` — the audio type layer

```c
/* utils/audio.h — shared audio types */

typedef struct {
  int16_t *samples;       /* interleaved stereo: L0,R0,L1,R1,... */
  int samples_per_second;
  int sample_count;       /* stereo pairs per call                */
} AudioOutputBuffer;

typedef enum {
  SOUND_NONE = 0,
  SOUND_CUP_FILL,       /* rising chime — cup reaches 100%   */
  SOUND_LEVEL_COMPLETE, /* three-note fanfare — all cups done */
  SOUND_TITLE_SELECT,   /* click — selecting a level          */
  SOUND_GRAVITY_FLIP,   /* whoosh — gravity reversal          */
  SOUND_RESET,          /* beep — level restart               */
  SOUND_COUNT
} SOUND_ID;

#define MAX_SIMULTANEOUS_SOUNDS 4

typedef struct {
  SOUND_ID sound_id;
  float phase;             /* oscillator phase [0.0, 1.0)     */
  int   samples_remaining;
  int   total_samples;
  int   fade_in_samples;   /* short ramp to prevent clicks    */
  float volume;
  float frequency;
  float frequency_slide;   /* Hz added per sample (glide)     */
  float pan_position;      /* -1=left, 0=center, 1=right      */
} SoundInstance;

typedef struct {
  float phase;
  float frequency;
  float volume;
  float current_volume;   /* smoothed toward volume each sample */
  float pan_position;
  int   is_playing;
} ToneGenerator;

#define MUSIC_PATTERN_LENGTH 16
#define MUSIC_NUM_PATTERNS   4

typedef struct {
  uint8_t       patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
  int           current_pattern;
  int           current_step;
  float         step_timer;
  float         step_duration; /* seconds per step (tempo)       */
  ToneGenerator tone;
  int           is_playing;
} MusicSequencer;

typedef struct {
  SoundInstance  active_sounds[MAX_SIMULTANEOUS_SOUNDS];
  MusicSequencer music;
  float          master_volume;
  float          sfx_volume;
  float          music_volume;
  int            samples_per_second;
} GameAudioState;

/* MIDI note → Hz.  A4 (69) = 440 Hz. */
static inline float midi_to_freq(int note) {
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}
```

Key insight: `GameAudioState` is just a plain struct embedded directly in
`GameState` — no heap allocation, no global singleton. The platform layer
never reads it; the game writes it via `game_play_sound` and the platform
reads audio samples via `game_get_audio_samples`.

---

## Step 2: `src/audio.c` — sound definitions

Each `SoundDef` is a row in a lookup table: start frequency, end frequency
(0 = no glide), duration in milliseconds, and peak volume.

```c
typedef struct {
  float frequency;      /* start Hz  */
  float frequency_end;  /* end Hz (0 = fixed pitch) */
  float duration_ms;
  float volume;
} SoundDef;

static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
  [SOUND_NONE]          = {  0.0f,    0.0f,    0.0f, 0.00f },
  [SOUND_CUP_FILL]      = { 440.0f,  880.0f,  150.0f, 0.65f }, /* rising   */
  [SOUND_LEVEL_COMPLETE]= { 523.0f, 1047.0f,  400.0f, 0.75f }, /* sweep up */
  [SOUND_TITLE_SELECT]  = { 350.0f,  450.0f,   80.0f, 0.45f }, /* click    */
  [SOUND_GRAVITY_FLIP]  = { 300.0f,  120.0f,  220.0f, 0.55f }, /* whoosh ↓ */
  [SOUND_RESET]         = { 400.0f,  300.0f,  100.0f, 0.45f }, /* ack      */
};
```

`game_play_sound_at` finds a free `SoundInstance` slot (or steals slot 0):

```c
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound, float pan) {
  int slot = 0;
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    if (audio->active_sounds[i].samples_remaining <= 0) { slot = i; break; }
  }
  const SoundDef *def = &SOUND_DEFS[sound];
  SoundInstance  *inst = &audio->active_sounds[slot];
  inst->phase            = 0.0f;  /* always reset phase at birth */
  inst->frequency        = def->frequency;
  inst->samples_remaining = (int)(def->duration_ms
                            * audio->samples_per_second / 1000.0f);
  inst->total_samples    = inst->samples_remaining;
  inst->fade_in_samples  = 96;    /* ~2 ms ramp — no click at attack */
  inst->volume           = def->volume;
  inst->pan_position     = pan;
  if (def->frequency_end > 0.0f && inst->samples_remaining > 0)
    inst->frequency_slide = (def->frequency_end - def->frequency)
                          / (float)inst->samples_remaining;
  else
    inst->frequency_slide = 0.0f;
}
```

---

## Step 3: `game_get_audio_samples` — the inner sample loop

```c
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
  GameAudioState *audio = &state->audio;
  int16_t *out  = buffer->samples;
  int      n    = buffer->sample_count;
  float    inv_sr = 1.0f / (float)buffer->samples_per_second;
  const float RAMP = 0.002f; /* volume ramp per sample (~22 ms at 44100) */

  memset(out, 0, n * 2 * sizeof(int16_t));

  for (int s = 0; s < n; s++) {
    float L = 0.0f, R = 0.0f;

    /* ── Sound effects ── */
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
      SoundInstance *inst = &audio->active_sounds[i];
      if (inst->samples_remaining <= 0) continue;

      float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f; /* square wave */
      float env  = (float)inst->samples_remaining / (float)inst->total_samples;

      int played = inst->total_samples - inst->samples_remaining;
      if (played < inst->fade_in_samples)
        env *= (float)played / (float)inst->fade_in_samples;  /* fade-in */

      float samp = wave * inst->volume * env * audio->sfx_volume;
      float lv, rv;
      calculate_pan_volumes(inst->pan_position, &lv, &rv);
      L += samp * lv;  R += samp * rv;

      inst->phase += inst->frequency * inv_sr;
      if (inst->phase >= 1.0f) inst->phase -= 1.0f; /* NEVER reset outright */
      inst->frequency += inst->frequency_slide;
      inst->samples_remaining--;
    }

    /* ── Music ── */
    ToneGenerator *tone = &audio->music.tone;
    float target = (audio->music.is_playing && tone->is_playing)
                 ? tone->volume : 0.0f;
    /* Smooth ramp prevents clicks on note changes */
    if      (tone->current_volume < target) tone->current_volume += RAMP;
    else if (tone->current_volume > target) tone->current_volume -= RAMP;

    if (tone->current_volume > 0.0001f) {
      float wave = (tone->phase < 0.5f) ? 1.0f : -1.0f;
      float samp = wave * tone->current_volume * audio->music_volume;
      L += samp;  R += samp; /* music centered */
      tone->phase += tone->frequency * inv_sr;
      if (tone->phase >= 1.0f) tone->phase -= 1.0f;
    }

    /* ── Mix & clip ── */
    L *= audio->master_volume * 16000.0f;
    R *= audio->master_volume * 16000.0f;
    *out++ = clamp_sample(L);
    *out++ = clamp_sample(R);
  }
}
```

**Why `SAMPLES_PER_FRAME = 2048`, not 735?**

At 60 fps, one frame = 44100 / 60 ≈ 735 samples. Tiny buffers mean the
audio thread must refill every ~16 ms. If the OS scheduler runs late by even
one frame, the stream underruns and you hear wave/echo artifacts.

2048 samples ≈ 46 ms gives you ~3 frames of headroom. The latency is still
imperceptible (< 50 ms) but underruns become rare in practice.

---

## Step 4: music sequencer

```c
/* Four A-minor pentatonic patterns (A4=69 C5=72 D5=74 E5=76 G5=79) */
static const uint8_t SS_PATTERN_A[16] = {
  69, 72,  0, 74, 76,  0, 74, 72,  69,  0, 72, 74,  76, 74, 72,  0,
};
/* ... patterns B, C, D ... */

void game_audio_update(GameAudioState *audio, float dt) {
  if (!audio->music.is_playing) return;
  MusicSequencer *seq = &audio->music;
  seq->step_timer += dt;
  if (seq->step_timer >= seq->step_duration) {
    seq->step_timer -= seq->step_duration;
    uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];
    seq->tone.is_playing = (note > 0);
    if (note > 0) seq->tone.frequency = midi_to_freq(note);
    if (++seq->current_step >= MUSIC_PATTERN_LENGTH) {
      seq->current_step = 0;
      seq->current_pattern = (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
    }
  }
}
```

`game_audio_update` runs once per game frame from `game_update`.
`game_get_audio_samples` runs on the audio thread (or inline in the Raylib
callback). They share `GameAudioState` without a lock because:
- `game_audio_update` writes `tone.is_playing` and `tone.frequency`
- `game_get_audio_samples` reads them
- Both run serially in the same thread in the Raylib AudioStream callback

---

## Step 5: Raylib AudioStream backend

```c
/* main_raylib.c */
#define SAMPLES_PER_SECOND 44100
#define SAMPLES_PER_FRAME  2048

static AudioStream g_stream;
static int16_t     g_sample_buffer[SAMPLES_PER_FRAME * 2]; /* stereo */

void platform_audio_init(GameState *state, int sps) {
  game_audio_init(&state->audio, sps);   /* wire sps into GameAudioState */
  InitAudioDevice();
  SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME);  /* MUST be before Load */
  g_stream = LoadAudioStream(sps, 16, 2);
  /* Pre-fill 2 buffers with silence — no underrun at startup */
  memset(g_sample_buffer, 0, sizeof(g_sample_buffer));
  UpdateAudioStream(g_stream, g_sample_buffer, SAMPLES_PER_FRAME);
  UpdateAudioStream(g_stream, g_sample_buffer, SAMPLES_PER_FRAME);
  PlayAudioStream(g_stream);
}

void platform_audio_update(GameState *state) {
  /* While the hardware has consumed a buffer, fill another */
  while (IsAudioStreamProcessed(g_stream)) {
    AudioOutputBuffer buf = {
      g_sample_buffer, SAMPLES_PER_SECOND, SAMPLES_PER_FRAME
    };
    game_get_audio_samples(state, &buf);
    UpdateAudioStream(g_stream, g_sample_buffer, SAMPLES_PER_FRAME);
  }
}
```

**Why `while`, not `if`?**
`IsAudioStreamProcessed` returns `true` once per consumed buffer. On a slow
frame (vsync jank) two buffers might be consumed. A `while` loop fills both,
preventing a two-frame glitch.

---

## Step 6: Mouse letterbox correction

When the window is resized Raylib letterboxes the canvas (scales to fit,
centers, adds black borders). `GetMousePosition()` returns window-space
coords. We must transform them to canvas-space:

```c
/* In platform_display_backbuffer — set globals: */
float scaleX = (float)win_w / (float)CANVAS_W;
float scaleY = (float)win_h / (float)CANVAS_H;
g_scale    = (scaleX < scaleY) ? scaleX : scaleY;
g_offset_x = (win_w - (int)(CANVAS_W * g_scale)) / 2;
g_offset_y = (win_h - (int)(CANVAS_H * g_scale)) / 2;

/* In platform_get_input — use globals to transform: */
Vector2 pos = GetMousePosition();
int cx = (int)((pos.x - g_offset_x) / g_scale);
int cy = (int)((pos.y - g_offset_y) / g_scale);
input->mouse.x = (cx < 0) ? 0 : (cx >= CANVAS_W) ? CANVAS_W-1 : cx;
input->mouse.y = (cy < 0) ? 0 : (cy >= CANVAS_H) ? CANVAS_H-1 : cy;
```

Without this, clicking the title-screen buttons with a resized window misses
by the letterbox border amount.

---

## Step 7: `change_phase()` audio wiring

```c
static void change_phase(GameState *state, GAME_PHASE next) {
  state->phase       = next;
  state->phase_timer = 0.0f;

  switch (next) {
    case PHASE_TITLE:         game_music_stop(&state->audio);  break;
    case PHASE_PLAYING:
      game_music_play(&state->audio);
      game_play_sound(&state->audio, SOUND_TITLE_SELECT);
      break;
    case PHASE_LEVEL_COMPLETE:
      game_music_stop(&state->audio);
      game_play_sound(&state->audio, SOUND_LEVEL_COMPLETE);
      break;
    case PHASE_FREEPLAY:      game_music_play(&state->audio);  break;
    case PHASE_COUNT: break;
  }
}
```

---

## Step 8: Cup fill sound

Inside `update_grains`, at the moment a cup transitions from not-full to full:

```c
if (right_color && cup->collected < cup->required_count) {
  int just_filled = (cup->collected + 1 >= cup->required_count);
  cup->collected++;
  if (just_filled)
    game_play_sound(&state->audio, SOUND_CUP_FILL);
  /* ... remove grain ... */
}
```

Testing `just_filled` BEFORE incrementing means the sound fires exactly once,
on the grain that causes the cup to overflow from `required_count - 1` to
`required_count`.

---

## Step 9: DEBUG_TRAP and ASSERT

```c
/* game.h */
#ifdef DEBUG
  #define DEBUG_TRAP() __builtin_trap()
  #define ASSERT(expr) do { if (!(expr)) { DEBUG_TRAP(); } } while (0)
#else
  #define DEBUG_TRAP() ((void)0)
  #define ASSERT(expr) ((void)(expr))
#endif
```

`build-dev.sh` passes `-DDEBUG` so these are active during development.
In a hypothetical release build (no `-DDEBUG`) they compile to nothing.

Usage example:

```c
void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
  ASSERT(sound >= 0 && sound < SOUND_COUNT); /* catch bad IDs immediately */
  /* ... */
}
```

---

## Common mistakes to prevent

1. **Resetting `inst->phase = 0` every frame** — creates a click at every
   buffer boundary. Phase must persist across calls; only reset at birth.

2. **`SAMPLES_PER_FRAME = 735`** — too small; OS jitter causes underruns.
   Use 2048 (≈ 46 ms) for stable audio.

3. **`SetAudioStreamBufferSizeDefault` after `LoadAudioStream`** — has no
   effect. Must be called BEFORE `LoadAudioStream`.

4. **Not transforming mouse coords for letterbox** — clicking works at 1:1
   but breaks when the window is resized.

5. **Calling `game_play_sound` from multiple update functions** — scatter-gun
   approach creates hard-to-debug double-triggers. Keep all phase-entry
   sounds in `change_phase()`.

---

## What to run

```bash
./build-dev.sh -r
```

Expected audio:
- Title screen: silence
- Click a level → brief click tone + music starts
- Grains fall → no sound
- Cup fills 100% → rising chime
- All cups fill → fanfare + music stops
- Next level loads → music resumes

---

## Summary

This lesson replaced the old `platform_play_sound()` fire-and-forget API with
a proper procedural synthesis engine: `GameAudioState` in the game layer,
`game_get_audio_samples()` called by the platform each frame, and an
`AudioStream` push-model in the Raylib backend. The game layer now fully owns
all audio state — platforms just move int16_t samples to hardware. This
architecture prevents the wavy/echoing artifacts caused by Raylib's internal
resampling and eliminates the need for any audio asset files.
