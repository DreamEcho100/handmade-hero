# Lesson 11 — Audio

## What We're Building

A self-contained procedural audio engine that synthesizes square-wave sound effects with fade-in/fade-out envelopes, frequency slides, and spatial stereo panning — plus a looping background music sequencer — with zero audio file dependencies.

> **Course Note:** The reference javidx9 source has no audio. This entire system is a course upgrade. The architecture mirrors what you would find in a Handmade Hero-style engine: PCM samples computed manually in a tight loop, no OS audio library abstraction beyond "give us a buffer to fill."

---

## Sound Design Overview

| Sound | When | Description |
|---|---|---|
| `SOUND_FOOD_EATEN` | On eating food | Short rising blip, panned by food column |
| `SOUND_GROW` | Each growth step (×5 per food) | Low subtle click as a segment extends |
| `SOUND_RESTART` | On game restart | Rising sweep jingle |
| `SOUND_GAME_OVER` | On collision | Descending sad tone |
| Music sequencer | During play | 16-note looping C-major melody |

---

## The Concept

### JS Analogy: Web Audio API, Implemented Manually

If you have used the Web Audio API, you can build a direct mental model:

| Web Audio concept | This engine's equivalent |
|---|---|
| `AudioContext` | `GameAudioState` embedded in `GameState` |
| `OscillatorNode` (square wave) | `phase` accumulator + `(phase < 0.5) ? 1 : -1` |
| `OscillatorNode.frequency.linearRampToValueAtTime` | `frequency_slide` — Hz added per sample |
| `GainNode` (fade-in/out) | `fade_in` / `fade_out` envelope in the sample loop |
| `StereoPannerNode` | `calculate_pan_volumes(pan, &lv, &rv)` |
| `AudioContext.createScriptProcessor` callback | `game_get_audio_samples` called per-frame by the platform (ALSA: `fill_audio`; Raylib: `IsAudioStreamProcessed` poll) |

The difference: instead of the browser wiring these nodes together for you, you compute every sample yourself in a `for` loop. This is slower to write but lets you see exactly what the audio is doing.

---

### The Phase Accumulator Pattern

A digital oscillator needs to know "where in the current cycle am I?" The answer is `phase` — a float in `[0.0, 1.0)`.

```
phase 0.0 = start of cycle
phase 0.5 = halfway through cycle
phase 1.0 = end of cycle (wraps back to 0)
```

Each sample, phase advances by `frequency / sample_rate`:

```
At 800 Hz, 44100 Hz sample rate:
  phase advance = 800 / 44100 ≈ 0.01814 per sample
  samples per cycle = 44100 / 800 ≈ 55.1
```

For a **square wave**: if `phase < 0.5` the waveform is `+1.0`; otherwise `-1.0`. That's it.

⚠ **Critical:** `phase` must **never** be reset between buffer fills. A discontinuity between two fills creates a jump in the waveform → audible click. `phase` is only reset when a *new* sound starts (in `game_play_sound_at`).

---

### Frequency Slide

`frequency_slide` is pre-computed when the sound starts:

```c
frequency_slide = (target_frequency - base_frequency) / total_samples;
```

Each sample, `frequency += frequency_slide`. This is linear interpolation from start to end frequency.

- `SOUND_FOOD_EATEN`: 800 Hz → 1200 Hz (rising pitch — positive reinforcement)
- `SOUND_GAME_OVER`:  400 Hz → 100 Hz  (falling pitch — "bad thing happened")

---

### Spatial Pan Formula

Food can appear anywhere in the grid. The pan formula maps its column to stereo position:

```
pan = (food_x / (GRID_WIDTH - 1)) * 2.0 - 1.0

food_x = 0:              pan = -1.0  (full left — never used; food spawns at x≥1)
food_x = GRID_WIDTH/2:   pan ≈  0.0  (centre)
food_x = GRID_WIDTH-1:   pan =  1.0  (full right — never used; food spawns at x≤GRID_WIDTH-2)
```

This gives the player spatial audio feedback about where the food is before they can see it clearly.

---

## The Code

### The Sample Loop — File: `src/audio.c`, `game_get_audio_samples`

```c
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
    GameAudioState *audio = &state->audio;
    int16_t *out     = buffer->samples;
    int      i, s;
    float    fade_out_samples = (float)(audio->samples_per_second / 100); /* ~10ms */

    /* Outer loop: one iteration per stereo output frame */
    for (s = 0; s < buffer->sample_count; s++) {
        float mix_left  = 0.0f;
        float mix_right = 0.0f;

        /* Inner loop: mix all active sound instances */
        for (i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
            SoundInstance *inst = &audio->active_sounds[i];
            if (inst->samples_remaining <= 0) continue;  /* slot is empty */

            /* ── Square wave oscillator ───────────────────────────────────
             * phase < 0.5 → positive half-cycle; phase >= 0.5 → negative.
             * JS: const wave = phase < 0.5 ? 1.0 : -1.0; */
            float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;

            /* ── Fade-in envelope ─────────────────────────────────────────
             * Ramp amplitude from 0→1 over the first fade_in_samples.
             * Prevents the click/pop that occurs when audio starts at
             * non-zero amplitude.
             *
             * elapsed = how many samples have already been output.
             * fade_in = elapsed / fade_in_samples  (clamped to 1.0 after) */
            int   elapsed  = inst->total_samples - inst->samples_remaining;
            float fade_in  = (inst->fade_in_samples > 0 &&
                              elapsed < inst->fade_in_samples)
                             ? (float)elapsed / (float)inst->fade_in_samples
                             : 1.0f;

            /* ── Fade-out envelope ────────────────────────────────────────
             * Ramp amplitude from 1→0 over the last ~10ms.
             * Prevents the click/pop at the end of the sound.
             *
             * samples_remaining counts down from total to 0.
             * When samples_remaining < fade_out_samples, we're in the
             * fade-out window.
             * fade_out = samples_remaining / fade_out_samples  (1→0) */
            float fade_out = ((float)inst->samples_remaining < fade_out_samples)
                             ? (float)inst->samples_remaining / fade_out_samples
                             : 1.0f;

            /* Combined envelope: all gains multiplied together */
            float env = fade_in * fade_out * inst->volume * audio->sfx_volume;

            /* ── Stereo pan ───────────────────────────────────────────────
             * calculate_pan_volumes maps pan [-1,+1] to per-channel gains.
             * Pan = -1.0: lv=1.0, rv=0.0  (full left)
             * Pan =  0.0: lv=1.0, rv=1.0  (centre — equal power)
             * Pan = +1.0: lv=0.0, rv=1.0  (full right)
             *
             * JS equivalent: StereoPannerNode.pan.value = inst->pan_position */
            {
                float lv, rv;
                calculate_pan_volumes(inst->pan_position, &lv, &rv);
                mix_left  += wave * env * lv;
                mix_right += wave * env * rv;
            }

            /* ── Advance phase ────────────────────────────────────────────
             * Each sample advances phase by frequency/sample_rate.
             * Wrap at 1.0 to keep in [0, 1).
             * ⚠ Never reset phase between buffer fills — that causes a click. */
            inst->phase += inst->frequency / (float)audio->samples_per_second;
            if (inst->phase >= 1.0f) inst->phase -= 1.0f;

            /* ── Frequency slide (linear interpolation) ──────────────────
             * frequency_slide = (target - base) / total_samples.
             * Pre-computed in game_play_sound_at; applied each sample here. */
            inst->frequency += inst->frequency_slide;

            inst->samples_remaining--;
        }

        /* ── Scale and write output ───────────────────────────────────────
         * Scale factor 16000: int16 max is 32767; we use ~49% of that range
         * to leave headroom for multiple simultaneous sounds without clipping.
         * clamp_sample prevents integer overflow if the sum exceeds ±32767. */
        {
            float scale = audio->master_volume * 16000.0f;
            *out++ = clamp_sample(mix_left  * scale);
            *out++ = clamp_sample(mix_right * scale);
        }
    }
}
```

---

### Triggering a Sound — File: `src/audio.c`, `game_play_sound_at`

```c
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound, float pan) {
    int i;
    SoundInstance *inst = NULL;

    if (sound <= SOUND_NONE || sound >= SOUND_COUNT) return;

    /* Find the first free slot (samples_remaining == 0 means the slot is idle) */
    for (i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
        if (audio->active_sounds[i].samples_remaining <= 0) {
            inst = &audio->active_sounds[i];
            break;
        }
    }
    if (!inst) return;  /* all slots busy — drop this sound gracefully */

    {
        const SoundDef *def = &SOUND_DEFS[sound];
        int total = (audio->samples_per_second * def->duration_ms) / 1000;
        if (total <= 0) return;

        inst->phase             = 0.0f;     /* new sound: reset phase */
        inst->frequency         = def->base_frequency;
        /* Pre-compute per-sample Hz change so the sample loop is just +=  */
        inst->frequency_slide   = (def->target_frequency - def->base_frequency)
                                  / (float)total;
        inst->volume            = def->volume;
        inst->total_samples     = total;
        inst->samples_remaining = total;
        inst->fade_in_samples   = total / 10;  /* first 10% = fade-in */
        inst->pan_position      = pan;
        inst->sound_id          = sound;
    }
}
```

---

### Spatial Pan at Food-Eat Site — File: `src/game.c`, food-eaten branch

```c
/* Map food column to stereo pan position.
 * food_x ∈ [1, GRID_WIDTH-2].
 * pan ∈ [-1.0, +1.0].
 *
 * Formula: pan = (food_x / (GRID_WIDTH - 1)) * 2.0 - 1.0
 * The difference between using GRID_WIDTH-1 and GRID_WIDTH-3 is imperceptible
 * — the spatial cue is clearly audible across the full range. */
float pan = ((float)s->food_x / (float)(GRID_WIDTH - 1)) * 2.0f - 1.0f;
game_play_sound_at(&s->audio, SOUND_FOOD_EATEN, pan);
```

---

### The Music Sequencer — File: `src/audio.c`

The music system is a separate channel from the SFX pool — it never occupies a `MAX_SIMULTANEOUS_SOUNDS` slot, so music and SFX can always play simultaneously.

#### Note Table

```c
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
    /* … 8 more notes … */
};
#define MELODY_LEN  ((int)(sizeof(MELODY) / sizeof(MELODY[0])))
```

A frequency of `0.0f` is a **rest** — the oscillator outputs silence for that note's duration.

#### State Fields (inside `GameAudioState`)

| Field | Purpose |
|---|---|
| `music_enabled` | 1 = playing; 0 = stopped |
| `music_note_idx` | Current position in `MELODY[]` |
| `music_note_samples_left` | Countdown to next note |
| `music_phase` | Oscillator phase, reset at each note boundary |
| `music_frequency` | Current note Hz (0 = rest) |
| `music_volume` | Per-channel amplitude (default 0.20) |
| `music_fade_in` | Counts from `MUSIC_FADE_IN_SAMPLES` → 0 at each note start |

#### Per-Sample Music Mix (inside `game_get_audio_samples`)

```c
if (audio->music_enabled) {
    /* Note transition: advance when current note expires */
    if (audio->music_note_samples_left <= 0) {
        audio->music_note_idx = (audio->music_note_idx + 1) % MELODY_LEN;
        audio->music_frequency         = MELODY[audio->music_note_idx].freq;
        audio->music_note_samples_left =
            (audio->samples_per_second * MELODY[audio->music_note_idx].dur_ms) / 1000;
        audio->music_phase   = 0.0f;              /* reset phase at note boundary */
        audio->music_fade_in = MUSIC_FADE_IN_SAMPLES;  /* prevent click */
    }

    if (audio->music_frequency > 0.0f) {
        float note_fade = (audio->music_fade_in > 0)
                          ? (1.0f - (float)audio->music_fade_in / MUSIC_FADE_IN_SAMPLES)
                          : 1.0f;
        float wave = (audio->music_phase < 0.5f) ? 1.0f : -1.0f;
        mix_left  += wave * audio->music_volume * note_fade;
        mix_right += wave * audio->music_volume * note_fade;
    }

    audio->music_phase += audio->music_frequency / (float)audio->samples_per_second;
    if (audio->music_phase >= 1.0f) audio->music_phase -= 1.0f;
    if (audio->music_fade_in > 0) audio->music_fade_in--;
    audio->music_note_samples_left--;
}
```

**Why reset phase at note boundaries?**

SFX use a continuous phase (never reset between *fills*) to avoid mid-buffer clicks.  Music notes are different: each note is an independent tonal event.  Resetting phase to 0 at the start of each note guarantees a predictable starting position (positive half-cycle).  Without the `MUSIC_FADE_IN_SAMPLES` ramp, this reset would produce a click — but the ramp eliminates it.

#### Lifecycle

```c
/* In snake_init (both startup and restart) — at the very end: */
game_music_start(&s->audio);   /* resets sequencer to note 0, sets volume */

/* On wall or self-collision — before game_play_sound(SOUND_GAME_OVER): */
game_music_stop(&s->audio);    /* sets music_enabled = 0 */

/* On restart (after snake_init) — the restart jingle plays over music: */
game_play_sound(&s->audio, SOUND_RESTART);
```

> **Why play SOUND_RESTART after `snake_init`, not before?**
> `snake_init` calls `memset(s, 0, sizeof(*s))` which zeroes `active_sounds[]`.
> Any sound queued before `snake_init` would be wiped.  Queue it after so it
> lands in the freshly initialised pool.

---

### Platform Integration — ALSA (`src/main_x11.c`)


ALSA manages the hardware audio buffer.  The two key configuration parameters
are `buffer_size` (how many samples the kernel holds) and `period_size` (how
many samples trigger a wake-up).  A critical gotcha is `start_threshold`:

```c
/* Default behaviour without explicit sw_params:
 *   start_threshold = buffer_size  (e.g. 4096 samples)
 * Effect: ALSA waits until the buffer is completely full before starting
 *         playback.  A game writing ~1024 samples/frame never fills a 4096-
 *         sample buffer → silence forever.
 *
 * Fix: set start_threshold = 1 so playback starts after the very first write. */

snd_pcm_sw_params_t *sw_params;
snd_pcm_sw_params_alloca(&sw_params);
snd_pcm_sw_params_current(pcm_handle, sw_params);          /* load defaults  */
snd_pcm_sw_params_set_start_threshold(pcm_handle,
    sw_params, 1);                                          /* start on first write */
snd_pcm_sw_params(pcm_handle, sw_params);                  /* apply          */
```

Without this, `setup_alsa` appears to succeed (no errors), but no audio plays
because ALSA is patiently waiting for a buffer that never gets full.

Each frame, `fill_audio` checks how many samples ALSA can accept and writes one
full period:

```c
static void fill_audio(GameState *state) {
    snd_pcm_sframes_t avail = snd_pcm_avail_update(g_pcm_handle);
    if (avail < (snd_pcm_sframes_t)g_samples_per_frame) return; /* not ready */

    AudioOutputBuffer ab;
    ab.sample_count = g_samples_per_frame;
    ab.samples      = g_audio_buffer;
    game_get_audio_samples(state, &ab);
    snd_pcm_writei(g_pcm_handle, g_audio_buffer,
                   (snd_pcm_uframes_t)g_samples_per_frame);
}
```

### Platform Integration — Raylib (`src/main_raylib.c`)

Raylib's audio stream is managed by polling `IsAudioStreamProcessed` each frame
and calling `UpdateAudioStream` when more samples are needed.  This is simpler
and safer than a callback approach because everything runs on the main thread:

```c
/* In platform_init: create a stereo 44100 Hz PCM stream */
InitAudioDevice();
g_audio_stream = LoadAudioStream(44100, 16, 2);  /* sample_rate, bit_depth, channels */
PlayAudioStream(g_audio_stream);
g_samples_per_frame = 735;  /* 44100 / 60 ≈ 735 stereo frames per game frame */
```

```c
/* In the main loop: fill stream when Raylib has consumed the previous buffer */
if (IsAudioStreamProcessed(g_audio_stream)) {
    AudioOutputBuffer ab;
    ab.sample_count = g_samples_per_frame;
    ab.samples      = g_audio_buffer;
    game_get_audio_samples(state, &ab);
    UpdateAudioStream(g_audio_stream, g_audio_buffer,
                      (int)g_samples_per_frame);
}
```

> **Why not a callback?**  `SetAudioStreamCallback` fires from a Raylib audio
> thread.  The frame count it passes is in *stereo frames* (pairs of L/R
> samples), not individual samples — and it can be called with varying counts.
> The per-frame polling model avoids thread-safety concerns and keeps sample
> generation firmly on the main thread where the rest of game logic lives.

---

## What To Notice

- **Phase never resets between fills (SFX).** `phase` is only set to `0.0f` in `game_play_sound_at`. Between fills, it carries the exact fractional position in the waveform cycle. Resetting it even once causes an audible glitch.
- **Phase IS reset at music note boundaries.** The music channel resets `music_phase = 0.0f` at each note start. This is safe because we're at a note boundary (not mid-fill), and the `MUSIC_FADE_IN_SAMPLES` ramp eliminates the resulting click. Two different rules for two different contexts.
- **`fade_in_samples = total / 10`.** For `SOUND_FOOD_EATEN` at 80 ms, 44100 Hz: `total = (44100 * 80) / 1000 = 3528`. `fade_in_samples = 352`. That's about 8 ms — below the threshold of audibility for a click.
- **`frequency_slide` is pre-computed per play, not per-sample arithmetic.** The sample loop just does `+= frequency_slide`. The division happens once at `game_play_sound_at` time.
- **`MAX_SIMULTANEOUS_SOUNDS = 4` is for SFX only.** The music sequencer is a separate channel and does not consume a slot. Max SFX active at once: `SOUND_FOOD_EATEN` + `SOUND_GROW` simultaneously = 2 slots. 4 slots gives comfortable headroom.
- **ALSA `start_threshold` must be set to 1.** The default inherits `buffer_size` from `snd_pcm_sw_params_current`. A game writing one period per frame never fills the whole buffer → no sound. Always call `snd_pcm_sw_params_set_start_threshold(handle, sw_params, 1)` after `snd_pcm_sw_params_current`.
- **Raylib audio is per-frame, not callback-based.** Using `IsAudioStreamProcessed` + `UpdateAudioStream` on the main thread avoids multi-threading complexity and works reliably with the game's fixed-step loop.
- **`master_volume` and `sfx_volume` must be preserved across `snake_init`.** `snake_init` does `memset(s, 0, sizeof(*s))` to reset all game state. This zeros both volume fields. In `game_get_audio_samples` the scale factor is `master_volume * 16000.0f` — if `master_volume == 0.0f` the output is silence on every frame, both at startup and after every restart. Always save and restore all audio config fields alongside `samples_per_second`.
- **Queue SOUND_RESTART *after* `snake_init`, never before.** `snake_init` calls `memset` which wipes `active_sounds[]`. Any sound queued before the memset is lost. This also applies to any game that has a "reset state" function — audio events must come after the reset.

---

## Exercises

1. **Experiment with waveforms.** Replace the square wave `(phase < 0.5) ? 1 : -1` with a sawtooth: `wave = phase * 2.0f - 1.0f`. How does the sound change? Try a triangle wave: `wave = (phase < 0.5f) ? (phase * 4.0f - 1.0f) : (3.0f - phase * 4.0f)`.

2. **Change the melody.** In `audio.c`, edit the `MELODY[]` table. Change a frequency to `0.0f` to add a rest; change it to `880.0f` for an A5. How does the melody change? Try doubling all `dur_ms` values to make it play at half speed.

3. **Add a pause jingle.** When the player presses `P` (pause), call `game_music_stop`. When they resume, call `game_music_start`. Notice that `game_music_start` resets `music_note_idx` — the melody restarts. Try preserving `music_note_idx` across pause so the melody resumes mid-phrase.

4. **Visualise the envelope.** For one second after eating food, draw a small bar in the header showing the current `fade_in * fade_out` envelope value of the last triggered `SOUND_FOOD_EATEN` instance. Store the value in `GameState.debug_envelope` from within `game_get_audio_samples`.
