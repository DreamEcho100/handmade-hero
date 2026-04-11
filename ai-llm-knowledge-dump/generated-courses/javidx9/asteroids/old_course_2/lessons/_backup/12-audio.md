# Lesson 12 — Audio System

## What you'll build
A software PCM mixer: `SoundInstance` pool, additive synthesis (square + triangle
+ noise waves), spatial panning based on asteroid position, and per-event sound
triggers (thrust, fire, explosions, death).  Audio works on both X11/ALSA
and Raylib backends.

---

## Core Concepts

### 1. Why Procedural Audio (no WAV files)?

| Approach | Pros | Cons |
|----------|------|------|
| WAV files | Easy to design; arbitrary sounds | File I/O, asset pipeline, binary to track |
| Procedural | No files, tiny binary, portable, educational | Limited to synthesised sounds |

For a retro arcade game, procedural synthesis is appropriate and teaches
more C concepts.

### 2. `SoundInstance` — One Playing Sound

```c
typedef struct {
    SOUND_ID sound_id;          // which sound definition to use
    int      samples_remaining; // frames of audio left
    int      total_samples;     // total at start (for envelope)
    float    phase;             // oscillator phase [0, 1)
    float    freq;              // current frequency (can slide)
    float    pan_left;          // left-channel gain [0, 1]
    float    pan_right;         // right-channel gain [0, 1]
} SoundInstance;
```

### 3. `SoundDef` — Static Sound Parameters

```c
typedef struct {
    float base_freq;    // Hz
    float freq_slide;   // Hz per second (negative = pitch descent)
    float duration;     // seconds
    float volume;       // relative scale [0, 1]
    int   wave_type;    // WAVE_SQUARE, WAVE_SAWTOOTH, WAVE_NOISE, WAVE_TRIANGLE
    int   envelope;     // ENVELOPE_FADEOUT or ENVELOPE_TRAPEZOID
} SoundDef;

// Example: large explosion
// { 140.0f, -80.0f, 0.55f, 0.75f, WAVE_NOISE, ENVELOPE_FADEOUT }
// 140 Hz noise, slides down 80 Hz/s, lasts 0.55s, 75% volume, linear decay
```

### 4. Wave Synthesis

All waveforms share the same **phase accumulator** pattern:

```c
// Advance phase each sample
inst->phase += inst->freq / (float)samples_per_second;
while (inst->phase >= 1.0f) inst->phase -= 1.0f;
```

At 880 Hz / 44100 Hz: each sample advances phase by `880 / 44100 ≈ 0.02`.
One complete cycle ≈ 50 samples.  The waveform shape then depends on wave type:

#### `WAVE_SQUARE` — instant polarity flip, rich harmonics

```c
s = (inst->phase < 0.5f) ? 1.0f : -1.0f;
```

Hard-edged.  Good for short SFX (fire pew, death tone) where a retro "digital"
character is intentional.  Sounds buzzy/harsh in a continuous loop.

#### `WAVE_TRIANGLE` — linear ramps, no discontinuities

```c
// -1 at phase=0, +1 at phase=0.5, -1 at phase=1.0
s = 1.0f - fabsf(4.0f * inst->phase - 2.0f);
```

Ramps linearly between -1 and +1.  No abrupt jumps anywhere in the waveform.
**Use for looping background sounds** (thrust, rotate) where smoothness matters —
a square wave loop is perceived as "buzzy" or "spammy" even at low volume.

Comparison at 120 Hz, one period (≈ 367 samples):
```
Square:   ████████████████████████████████__________ (jumps ±1 at midpoint)
Triangle: /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\  (continuous ramp)
```

#### `WAVE_NOISE` — random per sample

```c
s = ((float)(rand() & 0x7FFF) / 16383.5f) - 1.0f;
```

Flat white noise.  Used for explosion booms and cracks.

### 5. Amplitude Envelopes

Two envelope shapes are supported via the `SoundDef.envelope` field:

#### `ENVELOPE_FADEOUT` (default, for one-shot SFX)

```c
float progress = 1.0f - ((float)inst->samples_remaining / (float)inst->total_samples);
float envelope = 1.0f - progress;  // 1.0 at start, 0.0 at end
```

Amplitude decays linearly from full to zero.  Correct for sounds with a
natural decay: fire, explosions, death.

#### `ENVELOPE_TRAPEZOID` (for looping sounds: thrust, rotate)

```
amplitude
   1.0 |    ┌──────────────────────┐
       |   /                      \
   0.0 |──/                        \──
       0% 10%                    90% 100%  progress
           attack    sustain     decay
```

```c
if (progress < 0.10f)
    envelope = progress / 0.10f;           // 0 → 1 (attack)
else if (progress > 0.90f)
    envelope = (1.0f - progress) / 0.10f; // 1 → 0 (decay)
else
    envelope = 1.0f;                       // sustain
```

**Why trapezoid for looping sounds?**

A `FADEOUT`-looping sound creates an audible pulse: each 300ms burst starts
loud and fades to quiet, then the retrigger snaps back to full volume —
heard as a "throb" or "spam" at every retrigger.

With `ENVELOPE_TRAPEZOID`, game.c retriggers the sound when `remaining ≤ 10%`
(= at decay zone start).  The old and new instances are simultaneously in their
decay and attack zones.  Because both zones are linear over the same number of
samples, their amplitudes always sum to exactly 1.0:

```
old_amp = 1 - decay_progress   →  1.0 ... 0.0
new_amp =     attack_progress  →  0.0 ... 1.0
sum     = 1.0 throughout the crossfade  ✓
```

This produces a seamless, steady-volume loop — no throbbing, no clicking.

### 6. Spatial Panning

Explosions pan left or right based on the asteroid's X position:

```c
// In asteroids_update() when a bullet hits an asteroid:
float pan = (a->x / (float)SCREEN_W) * 2.0f - 1.0f;
// pan: -1 = hard left (asteroid at x=0), +1 = hard right (x=SCREEN_W)

game_play_sound_panned(&state->audio, SOUND_EXPLODE_LARGE, pan);
```

Inside `game_play_sound_panned`:
```c
// Linear pan law: left_gain = (1 - pan) / 2
inst->pan_left  = (1.0f - pan) * 0.5f;   // 1.0 at pan=-1, 0.0 at pan=+1
inst->pan_right = (1.0f + pan) * 0.5f;   // 0.0 at pan=-1, 1.0 at pan=+1
```

### 7. Thrust — Sustain-While-Held Pattern

Thrust uses a **sustain-while-held** design, not a clip-retrigger loop.

```c
// In asteroids_update() while UP is held:
game_sustain_sound(&state->audio, SOUND_THRUST);
// When key released: stop calling it → sound fades out naturally
```

`game_sustain_sound` does three things:
1. **Not playing** → starts a new instance (attack ramp plays: engine spool-up)
2. **Playing** → ensures `samples_remaining ≥ total/4` (25% floor, amplitude stays 1.0)
3. **Key released** (stop calling) → remaining drains to 0; TRAPEZOID decay zone fades volume 1→0

```c
void game_sustain_sound(GameAudioState *audio, SOUND_ID id) {
    for (int i = 0; i < audio->active_sound_count; i++) {
        if (audio->active_sounds[i].sound_id == id &&
            audio->active_sounds[i].samples_remaining > 0) {
            int floor = audio->active_sounds[i].total_samples / 4; /* 25% */
            if (audio->active_sounds[i].samples_remaining < floor)
                audio->active_sounds[i].samples_remaining = floor;
            return;
        }
    }
    game_play_sound(audio, id);  /* not playing: start it */
}
```

**Why 25% floor?**  At 25% remaining, `progress = 75%` — solidly in TRAPEZOID
sustain (10%–90%).  Each 2048-sample audio push decrements remaining by 2048.
The game refills every frame (~16ms), which is faster than one audio push (~46ms),
so remaining never reaches the 10% decay threshold while the key is held.

**Release lifecycle for 1.0s SOUND_THRUST:**
```
Key held:    [attack 100ms] → [sustain at 100% vol] → [sustain pinned at 25%]
Key released:  remaining drains from 25% ...
  • 25% → 10%: still in sustain zone, full volume (~150ms)
  • 10% → 0%:  decay zone, fades 1.0→0.0 (~100ms)
Total release: ~250ms — perceptible engine wind-down  ✓
```

**Sound design:** 120 Hz triangle, 1.0s, vol 0.75, ENVELOPE_TRAPEZOID.
120 Hz sits in the audible bass range on all speaker types — 60 Hz is below
the usable range of most laptop/small desktop speakers (felt as vibration,
not tone).  Vol 0.75 makes the engine clearly heard.

### 8. Rotate SFX — Same Sustain Pattern

Rotate uses the identical `game_sustain_sound` pattern:

```c
// While any rotation key is held:
if (input->left.ended_down || input->right.ended_down)
    game_sustain_sound(&state->audio, SOUND_ROTATE);
// Key released → natural fade-out via TRAPEZOID decay zone
```

**Sound design:** 200 Hz triangle, 0.6s, vol 0.50, ENVELOPE_TRAPEZOID.
Release: ~60ms fade — quick stop when rotation ends.
200 Hz is clearly distinct from both the 120 Hz thrust and the 110 Hz music
drone, while still sharing the smooth triangle timbre.

### 9. Gameplay Music Loop + Restart Jingle

Background music is a 3-second square drone re-triggered when it expires.
The restart jingle plays once on each restart.  We avoid overlap by not
starting gameplay music while the restart jingle is still playing:

```c
// In asteroids_update(), after the PHASE_DEAD check:
if (!game_is_sound_playing(&state->audio, SOUND_MUSIC_GAMEPLAY) &&
    !game_is_sound_playing(&state->audio, SOUND_MUSIC_RESTART)) {
    game_play_sound(&state->audio, SOUND_MUSIC_GAMEPLAY);
}
```

On restart, play the jingle AFTER `asteroids_init` (which memsets audio state):
```c
asteroids_init(state);                              // wipes active_sounds
game_play_sound(&state->audio, SOUND_MUSIC_RESTART); // then add jingle
```

### 10. The Mixing Loop

```c
for (int fi = 0; fi < out->sample_count; fi++) {
    float mix_left = 0, mix_right = 0;

    for (int si = 0; si < audio->active_sound_count; si++) {
        // generate sample, apply envelope, scale, pan → add to mix
    }

    out->samples[fi * 2 + 0] = clamp_sample(mix_left  * 32767.0f);
    out->samples[fi * 2 + 1] = clamp_sample(mix_right * 32767.0f);
}
// compact: remove sounds with samples_remaining == 0
```

Stereo interleaved: `samples[0]` = left frame 0, `samples[1]` = right frame 0,
`samples[2]` = left frame 1, etc.

### 11. Platform Audio — ALSA (X11) vs Raylib

Both backends feed the same `AudioOutputBuffer` to `game_get_audio_samples`.
The platform layer is the only thing that differs.

#### ALSA (X11 backend)

ALSA is Linux's native audio API.  Setup in `platform_init`:

```c
snd_pcm_t *pcm;
snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);

// Hardware params: S16 little-endian, stereo, 44100 Hz
snd_pcm_hw_params_t *hw;
snd_pcm_hw_params_alloca(&hw);
snd_pcm_hw_params_any(pcm, hw);
snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
unsigned int rate = 44100;
snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, NULL);
snd_pcm_hw_params_set_channels(pcm, hw, 2);
snd_pcm_hw_params(pcm, hw);

// Software params — CRITICAL: set start_threshold to 1 or get silent startup
snd_pcm_sw_params_t *sw;
snd_pcm_sw_params_alloca(&sw);
snd_pcm_sw_params_current(pcm, sw);       // load defaults FIRST
snd_pcm_sw_params_set_start_threshold(pcm, sw, 1);
snd_pcm_sw_params(pcm, sw);
```

Main loop — blocking write each frame:

```c
// Generate samples for this frame
game_get_audio_samples(&state, &audio_buf);

// snd_pcm_writei blocks until there's space in the ALSA ring buffer
// EPIPE = buffer underrun (game loop ran slow); recover and continue
int rc = snd_pcm_writei(pcm, audio_samples,
                        (snd_pcm_uframes_t)period_size);
if (rc == -EPIPE)
    snd_pcm_prepare(pcm);   // flush + restart the PCM stream
```

Teardown in `platform_shutdown` (always drain or the terminal hangs):
```c
snd_pcm_drain(pcm);   // wait until all pending audio plays out
snd_pcm_close(pcm);
```

**ALSA `start_threshold=1` — the most common ALSA bug:**  
Without this, ALSA waits for the ring buffer to fill to its default
threshold (~2048 frames) before playing anything.  That causes ~0.5s of
silence on startup.  Always load current sw_params first with
`snd_pcm_sw_params_current`, then override, then apply.

#### Raylib backend

```c
// platform_init():
InitAudioDevice();
// SAMPLES_PER_FRAME = 2048 (≈ 46 ms).  Must be called BEFORE LoadAudioStream.
// miniaudio double-buffers internally: ring = 2 × 2048 = 4096 frames total.
SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME);
AudioStream stream = LoadAudioStream(44100, 16, 2);
PlayAudioStream(stream);

// Main loop — gate BOTH generate AND push behind IsAudioStreamProcessed:
if (IsAudioStreamProcessed(stream)) {
    game_get_audio_samples(&state, &audio_buf);
    // Third arg = stereo FRAMES (not individual int16_t samples)
    UpdateAudioStream(stream, audio_samples, SAMPLES_PER_FRAME);
}
```

**Why gate with `IsAudioStreamProcessed` AND use 2048-frame chunks?**

Raylib's `UpdateAudioStream` does **not** block.  If you call it every game
frame with a 735-frame buffer (≈ 16 ms) and the game runs even slightly faster
than 60 fps, the two internal sub-buffers are sometimes both still full.
Raylib silently skips the write (`TRACELOG WARNING "Buffer not processed"`).
Skipped writes drift the ring-buffer write-pointer ahead of the read-pointer:
old and new data overlap → **wavy/echoing audio with a slight pitch shift**.

Using `SAMPLES_PER_FRAME = 2048` + the `IsAudioStreamProcessed` gate fixes
this: we only push when miniaudio has genuinely consumed a sub-buffer, so
there is always room and no data is ever skipped or overwritten.

**Why NOT call `game_get_audio_samples` every frame?**

`game_get_audio_samples` advances `samples_remaining` in every live
`SoundInstance`.  If we advance state every frame but only push every 2-3
frames, the sound-instance counters run ahead of the hardware playhead:
sounds appear to expire earlier in game time than they do in audio time.
With the gated approach, state advances only when we generate, keeping
game and audio states perfectly aligned.

**What about sounds triggered between gate opens?**

`game_play_sound` (called from `asteroids_update`) just enqueues a new
`SoundInstance` — it does not generate PCM.  Sounds queued during the
1-2 frames between gate opens are picked up on the very next generate
call.  At 46 ms chunks the maximum trigger-to-audio lag is imperceptible.

#### Platform audio comparison

| Feature | ALSA (X11) | Raylib |
|---------|-----------|--------|
| Open device | `snd_pcm_open` | `InitAudioDevice()` |
| Create stream | hw + sw params | `LoadAudioStream(44100, 16, 2)` |
| Push data | `snd_pcm_writei(pcm, buf, frames)` | `UpdateAudioStream(stream, buf, frames)` |
| Blocking | Yes (rate-limits game loop) | No — gate with `IsAudioStreamProcessed` |
| Underrun recovery | `snd_pcm_prepare(pcm)` on `-EPIPE` | Automatic |
| Shutdown | `snd_pcm_drain` + `snd_pcm_close` | `StopAudioStream` + `UnloadAudioStream` + `CloseAudioDevice()` |
| Silent startup bug | `start_threshold=1` fix needed | Not applicable |

---

## Verify

| Action | Expected sound |
|--------|----------------|
| Game starts | 110 Hz square drone begins immediately |
| Hold UP (thrust) | Low 90 Hz rumble, continuous while held |
| Hold LEFT or RIGHT | 400 Hz tick loops while rotating |
| Press SPACE (fire) | Short 1760 Hz ping |
| Bullet hits large asteroid | Deep noise boom, panned to asteroid position |
| Bullet hits medium asteroid | Mid noise crack, panned |
| Bullet hits small asteroid | High noise snap, panned |
| Ship collides with asteroid | Descending 55 Hz thud |
| Press FIRE to restart | Rising jingle, then drone resumes |

Test with X11 and Raylib — sounds should be identical on both backends.

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| Procedural audio | Square/triangle/noise synthesis | `OscillatorNode` (Web Audio API) |
| Spatial pan | `pan_left = (1-pan)/2` | `StereoPannerNode.pan` |
| PCM mixing | Sum floats → clamp int16 | `ScriptProcessorNode` buffer |
| Stereo interleaved | `[L0,R0,L1,R1,…]` | Web Audio `Float32Array` pairs |
| Looping sound quality | `WAVE_TRIANGLE` — no abrupt edges, smooth bass hum | `OscillatorNode type='triangle'` |
| Sustain-while-held | `game_sustain_sound` — pin remaining ≥ 25%, fade on release | `OscillatorNode` + `GainNode` ramp |
| ALSA silent startup | `start_threshold=1` fix | No equivalent |
| Raylib ring-buffer overflow | Gate push with `IsAudioStreamProcessed`; use 2048-frame chunks | — |
| ALSA blocking write | `snd_pcm_writei` rate-limits loop | `AudioContext` auto-scheduled |
