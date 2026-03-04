# Lesson 11: Audio — Full Three-Layer Audio System

## What we're building

A complete software audio system with three independent layers:

1. **One-shot SFX** — triggered by game events: hop click, squash (car hit),
   splash (water death), home-reached tone, win jingle, game-over chord, and a
   level-start arpeggio.
2. **Background music** — a looping chiptune melody driven by a note sequencer.
   `game_music_update(state, dt)` advances the note index each frame.
3. **Ambient loops** — two continuously looping environmental layers (traffic
   drone, river noise) whose volumes fade in/out based on the frog's distance
   from road and river lanes.

No WAV files, no external libraries beyond ALSA (X11) or Raylib's audio stream.
All sounds are synthesised mathematically from a `SoundDef` table.

## What you'll learn

- Software audio synthesis pipeline: wave → envelope → mix → PCM output
- `SoundDef` table pattern: immutable design-time parameters
- `SoundInstance` pool: mutable per-play state (`phase`, `freq`, `vol_scale`)
- New wave type: **`WAVE_SAWTOOTH`** — engine/drone synthesis
- New envelope: **`ENVELOPE_FLAT`** — constant amplitude for looping ambients
- **`looping`** flag — auto-restart instead of expiry (ambient + music)
- **`vol_scale`** per-instance volume — replaces global `sfx_volume` in the mix
- Music note sequencer: `MUSIC_NOTES[]` array + `game_music_update()`
- Ambient distance model: inverse-distance fade by lane proximity
- `game_set_sound_vol()` — live volume control for looping sounds
- ALSA and Raylib audio backends (platform wiring unchanged)

## Prerequisites

Lessons 01–10 complete and building.

---

## Step 1 — Volume hierarchy and the `vol_scale` field

The original mixer formula was:

```
amp = raw × envelope × def.volume × sfx_volume × master_volume
```

This applied one `sfx_volume` to every sound, making it impossible to give
music and ambient sounds their own independent volume budgets.

The updated formula is:

```
amp = raw × envelope × def.volume × inst.vol_scale × master_volume
```

`vol_scale` is a **per-instance** field in `SoundInstance`.  When a sound is
triggered, the caller decides which "layer" it belongs to and sets `vol_scale`:

| Layer   | vol_scale value at trigger          |
|---------|-------------------------------------|
| SFX     | `audio->sfx_volume` (0.75 default)  |
| Music   | `audio->music_volume` (0.40 default)|
| Ambient | 0.0 — faded in by distance model    |

```c
typedef struct {
    SOUND_ID sound_id;
    int      samples_remaining;
    int      total_samples;
    float    phase;
    float    freq;
    float    pan_left;
    float    pan_right;
    int      looping;    /* ← NEW: 1 = auto-restart; 0 = expire             */
    float    vol_scale;  /* ← NEW: per-layer volume multiplier              */
} SoundInstance;
```

**JS analogy** — each `SoundInstance` now carries its own `GainNode` value
instead of sharing a single bus gain:

```js
// Old: all SFX routed through one sfxGain
const sfxGain = audioCtx.createGain();
sfxGain.gain.value = 0.75;

// New: each instance carries its own gain value
const inst = { ... gainValue: sfxVolume };          // SFX
const inst = { ... gainValue: musicVolume };         // music note
const inst = { ... gainValue: 0, looping: true };   // ambient (faded in later)
```

---

## Step 2 — Wave types: SAWTOOTH

Previous wave types:

- **`WAVE_SQUARE`**: alternates ±1 every half-period — bright, buzzy.
- **`WAVE_NOISE`**: uniform random ±1 per sample — white noise.
- **`WAVE_TRIANGLE`**: linear ramp −1→+1→−1 — soft, flute-like.

New wave type:

- **`WAVE_SAWTOOTH`**: ramps `+1→−1` linearly per period — rich harmonic
  content, ideal for engine/tyre drones.  Formula: `raw = 1.0f - 2.0f * phase`.

```
phase 0.0 → raw = +1.0
phase 0.5 → raw =  0.0
phase 1.0 → raw = -1.0  (wraps to 0.0 → +1.0 on next cycle)
```

```c
case WAVE_SAWTOOTH:
    raw = 1.0f - 2.0f * inst->phase;
    break;
```

The sawtooth is used for `SOUND_AMBIENT_TRAFFIC` at 65 Hz — below human
perception of pitch (< 80 Hz it sounds like a mechanical rumble rather than a
musical note).

---

## Step 3 — Envelope types: FLAT

Previous envelopes:

- **`ENVELOPE_FADEOUT`**: linear decay `1→0` over full duration.
- **`ENVELOPE_TRAPEZOID`**: 10% attack + 80% sustain + 10% decay.

New envelope:

- **`ENVELOPE_FLAT`**: constant amplitude `= 1.0`.  Used for looping ambient
  drones where the volume is controlled externally by `vol_scale` rather than
  by the envelope shape.  Without `ENVELOPE_FLAT`, a looping ambient would fade
  out and reset abruptly on every loop cycle.

```c
case ENVELOPE_FLAT:
    envelope = 1.0f;
    break;
```

---

## Step 4 — Expanded SoundDef table

```c
static const SoundDef SOUND_DEFS[] = {
    /* SOUND_NONE            */ { 0.0f,    0.0f,   0.00f, 0.00f, WAVE_SQUARE,   ENVELOPE_FADEOUT   },
    /* SOUND_HOP             */ { 900.0f, -300.0f, 0.06f, 0.50f, WAVE_SQUARE,   ENVELOPE_FADEOUT   },
    /* SOUND_SQUASH          */ { 150.0f,  0.0f,   0.20f, 0.80f, WAVE_NOISE,    ENVELOPE_FADEOUT   },
    /* SOUND_SPLASH          */ { 280.0f, -90.0f,  0.35f, 0.75f, WAVE_NOISE,    ENVELOPE_FADEOUT   },
    /* SOUND_HOME_REACHED    */ { 523.0f,  200.0f, 0.30f, 0.70f, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID },
    /* SOUND_WIN_JINGLE      */ { 523.0f,  580.0f, 0.55f, 0.75f, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID },
    /* SOUND_GAME_OVER       */ { 220.0f, -55.0f,  0.90f, 0.80f, WAVE_SQUARE,   ENVELOPE_FADEOUT   },
    /* SOUND_LEVEL_START     */ { 440.0f,  440.0f, 0.35f, 0.60f, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID },
    /* SOUND_MUSIC           */ { 659.0f,  0.0f,   0.20f, 0.40f, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID },
    /* SOUND_AMBIENT_TRAFFIC */ { 65.0f,   0.0f,   1.00f, 0.55f, WAVE_SAWTOOTH, ENVELOPE_FLAT      },
    /* SOUND_AMBIENT_WATER   */ { 0.0f,    0.0f,   1.00f, 0.45f, WAVE_NOISE,    ENVELOPE_FLAT      },
};
```

Key design decisions:

- **SQUASH vs SPLASH** — car hits (`SOUND_SQUASH`) use 150 Hz noise for a
  percussive thud; water deaths (`SOUND_SPLASH`) start at 280 Hz and slide
  down for a bubbly descend.  `game_update()` picks the right sound:
  ```c
  int lane    = (int)state->frog_y;
  int on_road = (lane >= 5 && lane <= 8);
  trigger_sound(state, on_road ? SOUND_SQUASH : SOUND_SPLASH, pan);
  ```

- **WIN_JINGLE** — `freq_slide = 580 Hz/s` over 0.55 s sweeps C5→C6 for a
  satisfying octave rise.

- **LEVEL_START** — shorter sweep (0.35 s) on game start and restart.

- **SOUND_MUSIC** — `base_freq` is irrelevant; the sequencer overrides `freq`
  and `total_samples` for each note.

---

## Step 5 — Background music sequencer

```c
typedef struct { float freq; float dur; } MusicNote;

static const MusicNote MUSIC_NOTES[20] = {
    { 880.0f,  0.14f },   /* A5 */
    { 783.99f, 0.14f },   /* G5 */
    { 659.25f, 0.14f },   /* E5 */
    { 783.99f, 0.14f },   /* G5 */
    { 880.0f,  0.14f },   /* A5 */
    { 783.99f, 0.14f },   /* G5 */
    { 659.25f, 0.28f },   /* E5 (held) */
    { 0.0f,    0.14f },   /* rest */
    /* … 12 more notes … */
};
```

`game_music_update(state, dt)` is called every game frame.  It decrements a
timer; when the timer expires it advances `music_note_idx` and fires the next
note:

```c
void game_music_update(GameState *state, float dt) {
    if (!audio->music_playing) return;
    audio->music_note_timer -= dt;
    if (audio->music_note_timer > 0.0f) return;

    audio->music_note_idx = (audio->music_note_idx + 1) % MUSIC_NOTE_COUNT;
    const MusicNote *note = &MUSIC_NOTES[audio->music_note_idx];
    audio->music_note_timer = note->dur;

    if (note->freq <= 0.0f) return;  /* rest */

    play_sound_internal(state, SOUND_MUSIC, 0.0f, audio->music_volume, 0);

    /* Override the newly allocated slot's frequency and duration */
    int slot = find_sound_slot_by_id(audio, SOUND_MUSIC);
    if (slot >= 0) {
        audio->active_sounds[slot].freq          = note->freq;
        audio->active_sounds[slot].total_samples = note_samples;
        audio->active_sounds[slot].samples_remaining = note_samples;
    }
}
```

The sequencer stores three fields in `GameAudioState`:

```c
int   music_note_idx;    /* current position in MUSIC_NOTES[]             */
float music_note_timer;  /* seconds until next note                        */
int   music_playing;     /* 1 = playing; 0 = paused (set to 0 on win)     */
```

All three survive game restart because `game_init()` saves/restores
`music_volume` and `ambient_volume`, and re-starts the sequencer from index -1
(advances to 0 on the first `game_music_update()` call).

---

## Step 6 — Ambient loops and the distance model

### Looping instances

`game_play_looping(state, id, pan, vol_scale)` starts an ambient and marks it
`looping = 1`.  In the compaction pass, looping instances are **reset** instead
of freed:

```c
/* Compaction pass in game_get_audio_samples() */
} else if (inst->looping) {
    inst->samples_remaining = inst->total_samples;
    inst->freq  = def->base_freq;
    inst->phase = 0.0f;
    /* vol_scale is NOT reset — it's being modulated by game_update_ambients */
    write++;
}
```

### Distance-based volume model

```c
void game_update_ambients(GameState *state) {
    float fy = state->frog_y;

    /* Distance to nearest road lane (rows 5-8) */
    float road_dist = min(|fy-5|, |fy-6|, |fy-7|, |fy-8|);
    float road_vol  = ambient_volume / (1 + road_dist × 0.6);

    /* Distance to nearest river lane (rows 1-3) */
    float river_dist = min(|fy-1|, |fy-2|, |fy-3|);
    float water_vol  = ambient_volume / (1 + river_dist × 0.7);

    /* 20% lerp toward target — smooth fade, no pops */
    current_traffic_vol += (road_vol  - current) × 0.20;
    current_water_vol   += (water_vol - current) × 0.20;
}
```

At `ambient_volume = 0.30`:

| Frog position | Traffic vol | Water vol |
|---------------|-------------|-----------|
| Row 9 (start) | 0.06        | 0.04      |
| Row 6 (road)  | 0.30        | 0.08      |
| Row 2 (river) | 0.07        | 0.30      |

The lerp factor (0.20 per frame) means it takes ~4-5 frames to reach 90% of
the target — about 65 ms at 60 fps.  This prevents instant pops while still
feeling responsive.

---

## Step 7 — Triggering sounds in game_init and game_update

In `game_init()`:

```c
/* Start ambients at vol 0; game_update_ambients() fades them in */
game_play_looping(state, SOUND_AMBIENT_TRAFFIC, 0.0f, 0.0f);
game_play_looping(state, SOUND_AMBIENT_WATER,   0.0f, 0.0f);

/* Short startup arpeggio */
game_play_sound(state, SOUND_LEVEL_START, 0.0f);

/* Start music sequencer */
state->audio.music_playing  = 1;
state->audio.music_note_idx = -1;  /* advances to 0 on first update */
```

In `game_update()`, called every frame before phase-specific logic:

```c
game_music_update(state, dt);
game_update_ambients(state);
```

On win:

```c
state->phase = PHASE_WIN;
trigger_sound(state, SOUND_WIN_JINGLE, 0.0f);
state->audio.music_playing = 0;  /* silence music during win screen */
```

---

## Step 8 — ALSA and Raylib backends (unchanged)

The platform backends call `game_get_audio_samples()` exactly as before.
No platform changes are required for the expanded audio system.

```c
/* X11 backend — unchanged */
game_get_audio_samples(&state, &audio_buf);
snd_pcm_writei(g_alsa_pcm, audio_samples, g_alsa_period_size);

/* Raylib backend — unchanged */
if (IsAudioStreamProcessed(g_audio_stream)) {
    game_get_audio_samples(&state, &audio_buf);
    UpdateAudioStream(g_audio_stream, audio_samples, SAMPLES_PER_FRAME);
}
```

The only platform change is setting the two new volume fields before calling
`game_init()`:

```c
state.audio.master_volume  = 1.0f;
state.audio.sfx_volume     = 1.0f;
state.audio.music_volume   = 0.40f;   /* ← new */
state.audio.ambient_volume = 0.30f;   /* ← new */
game_init(&state, ASSETS_DIR);
```

---

## Build and run

```bash
# X11 backend
./build-dev.sh --backend=x11 -r

# Raylib backend
./build-dev.sh --backend=raylib -r
```

---

## Expected result

| Event                | Sound                                        |
|----------------------|----------------------------------------------|
| Frog hops            | Short downward click, panned to frog position|
| Car hit (road lane)  | Low percussive thud (SQUASH)                 |
| Water death          | Descending noise burst (SPLASH)              |
| Home cell reached    | Ascending triangle tone                      |
| All 5 homes filled   | Octave sweep jingle + music stops            |
| All lives lost       | Slow descending chord                        |
| Game start / restart | Short ascending arpeggio                     |
| Standing on road     | Low engine rumble (louder near lanes 5-8)    |
| Standing on river    | White noise flow (louder near lanes 1-3)     |
| Background           | Looping pentatonic chiptune melody           |

## Exercises

1. Add a `SOUND_BONUS` triggered when `score` reaches a multiple of 200.
   Design a unique `SoundDef` for it (try `WAVE_TRIANGLE`, short attack).
2. Change `ambient_volume` from 0.30 to 0.55 at runtime by pressing a key.
   Observe how the lerp prevents pops.
3. Add a third ambient layer: a faint `WAVE_SQUARE` at 110 Hz when the frog
   is on the home row (row 0) — a distant crowd cheer.
4. Modify `game_music_update()` to double the tempo (halve `note->dur`) when
   `homes_reached >= 3` — builds tension as the frog nears a win.

## What's next

Lesson 12 polishes the game — respawn flicker, overlays, restart flow — and
explains the `utils/` refactor that separates drawing code from game logic.
