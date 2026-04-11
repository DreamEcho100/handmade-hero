# Lesson 14 — Audio System

## What you'll learn
- Procedural PCM synthesis: no WAV files
- Four waveforms and their character
- `SoundDef` table and `SOUND_ID` enum
- `ENVELOPE_TRAPEZOID` for seamless looping
- `game_sustain_sound()` for continuous sounds
- Spatial panning in game units

---

## No audio files

All sounds are generated mathematically from a table of parameters.
The synthesis pipeline runs each frame for all active sounds:

```
For each active SoundInstance:
  raw = generate_wave(waveform, phase)
  raw *= linear_envelope(progress, envelope_type)
  raw *= volume * sfx_volume * master_volume
  mix_left  += raw * pan_left
  mix_right += raw * pan_right
  advance phase by freq/samplerate
  slide freq by freq_slide/samplerate
Output: clamp(mix * 32767) → int16_t
```

---

## Four waveforms

```c
// WAVE_SQUARE:   phase < 0.5 → +1.0, else → -1.0.  Retro, rich harmonics.
// WAVE_SAWTOOTH: 2*phase - 1.  Bright, buzzy.
// WAVE_NOISE:    rand() → [-1, +1].  Explosions.
// WAVE_TRIANGLE: 1 - |4*phase - 2|.  Smooth, no abrupt edges.  Best for loops.
```

---

## SoundDef table

```c
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    /* SOUND_NONE */
    {   0.0f,    0.0f, 0.00f, 0.00f, WAVE_SQUARE,   ENVELOPE_FADEOUT   },
    /* SOUND_THRUST — smooth bass hum, seamlessly looped */
    { 120.0f,    0.0f, 1.00f, 0.75f, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID },
    /* SOUND_FIRE — short high-pitched pew */
    {1760.0f, -800.0f, 0.06f, 0.55f, WAVE_SQUARE,   ENVELOPE_FADEOUT   },
    /* SOUND_EXPLODE_LARGE */
    { 140.0f,  -80.0f, 0.55f, 0.80f, WAVE_NOISE,    ENVELOPE_FADEOUT   },
    /* SOUND_EXPLODE_MEDIUM */
    { 220.0f, -120.0f, 0.35f, 0.70f, WAVE_NOISE,    ENVELOPE_FADEOUT   },
    /* SOUND_EXPLODE_SMALL */
    { 400.0f, -250.0f, 0.18f, 0.60f, WAVE_NOISE,    ENVELOPE_FADEOUT   },
    /* SOUND_SHIP_DEATH — descending whomp */
    {  55.0f,  -30.0f, 0.90f, 0.85f, WAVE_SQUARE,   ENVELOPE_FADEOUT   },
    /* SOUND_ROTATE — 200 Hz triangle, seamlessly looped */
    { 200.0f,    0.0f, 0.60f, 0.50f, WAVE_TRIANGLE, ENVELOPE_TRAPEZOID },
    /* SOUND_MUSIC_GAMEPLAY — 110 Hz drone (A2), looped every 3s */
    { 110.0f,    0.0f, 3.00f, 0.20f, WAVE_SQUARE,   ENVELOPE_FADEOUT   },
    /* SOUND_MUSIC_RESTART — ascending jingle on restart */
    { 330.0f,  200.0f, 0.60f, 0.50f, WAVE_SQUARE,   ENVELOPE_FADEOUT   },
};
```

`freq_slide`: Hz per second of pitch change.
- `SOUND_FIRE`: starts at 1760 Hz (A6), slides down 800 Hz/s → descends 48 Hz in 60 ms.
- `SOUND_EXPLODE_LARGE`: starts at 140 Hz, slides down 80 Hz/s → rumbles.

---

## ENVELOPE_TRAPEZOID — seamless looping

For `SOUND_THRUST` and `SOUND_ROTATE` (keys held down):

```
amplitude
  1.0 |      ___________
      |     /           \
      |    /             \
  0.0 |___/               \___
        0  10%           90% 100%   progress
        attack  sustain   decay
```

When the game retriggers a looping sound:
- Old instance enters its 10% decay zone (amplitude fading from 1→0).
- New instance starts its 10% attack zone (amplitude rising from 0→1).
- `old_amp + new_amp = (1-p) + p = 1.0` throughout → seamless crossfade.

Without TRAPEZOID, each retrigger produces a loud→quiet→loud pulse.

---

## ENVELOPE_FADEOUT — one-shot sounds

```
amplitude
  1.0 |#
      |  \
      |    \
  0.0 |______\___
        0     100%   progress
```

`amplitude = 1 - progress` — simple linear decay.  Used for explosions,
fire shots, death sounds, and music (which loops via re-trigger at expiry).

---

## game_sustain_sound()

```c
void game_sustain_sound(GameAudioState *audio, SOUND_ID id) {
    // If already playing, clamp samples_remaining ≥ 25% of total
    for (int i = 0; i < audio->active_sound_count; i++) {
        if (audio->active_sounds[i].sound_id == id &&
            audio->active_sounds[i].samples_remaining > 0) {
            int floor = audio->active_sounds[i].total_samples / 4;
            if (audio->active_sounds[i].samples_remaining < floor)
                audio->active_sounds[i].samples_remaining = floor;
            return;
        }
    }
    // Not playing → start it
    game_play_sound(audio, id);
}
```

Call once per frame while a key is held:
```c
if (input->up.ended_down)
    game_sustain_sound(&state->audio, SOUND_THRUST);
if (input->left.ended_down || input->right.ended_down)
    game_sustain_sound(&state->audio, SOUND_ROTATE);
```

When the key is released, stop calling `game_sustain_sound`.
`samples_remaining` drains naturally; the TRAPEZOID decay kicks in
automatically during the final 10%.

---

## Spatial panning

```c
// Pan based on asteroid's world-x position
float pan = (a->x / GAME_UNITS_W) * 2.0f - 1.0f;
// pan: -1 = hard left, 0 = centre, +1 = hard right
game_play_sound_panned(&state->audio, snd, pan);
```

Maps the world-x range `[0, GAME_UNITS_W]` to `[-1, +1]`.
An asteroid at the left edge of the screen explodes in the left speaker.

---

## Raylib audio: push-when-ready

```c
// Only generate + push when miniaudio has consumed one sub-buffer:
if (IsAudioStreamProcessed(g_audio_stream)) {
    game_get_audio_samples(&state, &audio_buf);
    UpdateAudioStream(g_audio_stream, audio_samples, SAMPLES_PER_FRAME);
}
```

`SAMPLES_PER_FRAME = 2048` (~46 ms).  Don't call `game_get_audio_samples`
unconditionally: that would cause ring-buffer drift → "wavy" artifact.

---

## Key takeaways

1. No WAV files — all sounds synthesised from `SoundDef` parameters.
2. TRAPEZOID envelope enables seamless looping (sum of old+new = 1.0).
3. `game_sustain_sound`: clamp samples_remaining ≥ 25% while key is held.
4. `freq_slide` creates pitch bends: negative = descending.
5. Spatial pan: `(x / GAME_UNITS_W) * 2 - 1` maps position to [-1, +1].
6. Raylib: only push audio when `IsAudioStreamProcessed()` returns true.
