# Lesson 19: Waveforms, MIDI Notes, Panning, and Clamp Helpers

## Frontmatter

- Module: `05-audio-foundations-and-runtime`
- Lesson: `19`
- Status: Required
- Target files:
  - `src/utils/audio-helpers.h`
- Target symbols:
  - `audio_midi_to_freq`
  - `audio_wave_square`
  - `audio_wave_sine`
  - `audio_wave_triangle`
  - `audio_wave_sawtooth`
  - `audio_clamp_sample`
  - `audio_calculate_pan`

## Observable Outcome

By the end of this lesson, you should be able to explain how one note number and one phase position become one left/right sample contribution before any voice-pool or backend logic is involved.

## Prerequisite Bridge

Lesson 18 explained where audio samples go.

Lesson 19 explains where their values come from.

The runtime now needs a small pure-math vocabulary that can answer questions like:

- what frequency does this MIDI note mean?
- what sample value does this phase position produce?
- how should one mono sample be split between left and right?
- how do we keep the final result inside a safe normalized range?

That vocabulary lives in `src/utils/audio-helpers.h`.

## Why This Lesson Exists

Before the runtime can talk about voices or mixing, it needs stateless building blocks.

That is why this helper file deliberately knows nothing about:

- Raylib
- ALSA
- `GameAudioState`
- scenes
- voice lifetimes

This is pure math and small utility policy.

## The Tiny Pipeline

Read the helper file as one little pipeline:

```text
note -> frequency -> phase step -> waveform value -> pan split -> clamp
```

If you keep that flow in mind, the file becomes much easier to digest.

## Step 1: Keep the Unit Vocabulary Straight

Do not let these concepts collapse into one another:

```text
MIDI note number -> symbolic pitch label
frequency        -> cycles per second
phase            -> position inside one oscillator cycle
sample value     -> waveform output in [-1, +1]
```

Those are related, but they are not interchangeable.

## Step 2: Read `audio_midi_to_freq(...)`

The helper is:

```c
static inline float audio_midi_to_freq(int note) {
  if (note == 0) return 0.0f;
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}
```

This function encodes a lot of music vocabulary in a tiny amount of code.

## What the Formula Means

`69`

- MIDI note for A4

`440.0f`

- reference frequency for A4 in Hz

`(note - 69) / 12`

- semitone distance from A4, measured in octaves

So:

- `69` -> `440 Hz`
- `60` -> middle C
- `72` -> C5

And the helper also gives the course a convenient music-data rule:

```text
note 0 -> rest -> 0 Hz
```

That is why the pattern tables in `game/audio_demo.c` can encode rests without a separate data type.

## Step 3: Understand `phase` Before Reading Any Waveform

Every waveform helper accepts a `phase` in `[0, 1)`.

That is not loudness.

It is not frequency.

It is only position inside one repeating cycle.

## Visual: Phase as Position

```text
phase 0.00 -> start of the cycle
phase 0.25 -> quarter through
phase 0.50 -> halfway through
phase 0.99 -> almost wrapped
```

That mental model is the key to the whole file.

## Worked Example: Phase Step Size

If the oscillator is `480 Hz` and the sample rate is `48000 Hz`, then each emitted sample advances phase by:

$$
\frac{480}{48000} = 0.01
$$

So every sample moves the oscillator forward by one hundredth of a cycle.

That is why the mixer later uses `frequency * inv_sr` to advance phase.

## Step 4: Read the Waveforms by Shape, Not by Fear

### Square wave

```c
return (phase < 0.5f) ? 1.0f : -1.0f;
```

Meaning:

```text
first half of the cycle  -> +1
second half of the cycle -> -1
```

This abrupt jump is why it sounds bright and buzzy.

### Sine wave

```c
return sinf(phase * 2.0f * (float)M_PI);
```

This is the smoothest oscillator in the set.

### Triangle wave

This ramps up and down linearly, producing a sharper sound than sine but a softer shape than square.

### Sawtooth wave

```c
return 2.0f * phase - 1.0f;
```

This ramps steadily and then wraps sharply.

It tends to sound bright and harsher than triangle or sine.

## Rough Shape Intuition

```text
square:    + + + + - - - -
sine:      smooth rounded oscillation
triangle:  /\  /\  /\
sawtooth:  /| /| /|
```

This is not mathematically exact plotting, but it is enough to connect code shape to sound character.

## Step 5: Keep Shape Separate From Loudness

The waveform helpers return values around:

```text
[-1.0, +1.0]
```

That means they define shape, not final output loudness.

Later layers apply loudness by multiplying with things like:

- voice volume
- category volume
- master volume

That separation keeps the synthesis pipeline cleaner.

## Step 6: Read `audio_calculate_pan(...)` as Channel Multiplier Math

The helper turns one `pan` value into left/right multipliers.

Anchor cases:

```text
pan = -1.0 -> full left
pan =  0.0 -> centered
pan = +1.0 -> full right
```

The function is not doing spatial simulation.

It is only deciding how much of one mono contribution goes into each channel.

## Worked Example: Center vs Full Left

For a mono contribution of `0.5`:

- with `pan = 0.0`, both channels receive the centered contribution
- with `pan = -1.0`, the right multiplier falls to zero

That is why this helper still belongs in the pure-math layer.

## Step 7: Read `audio_clamp_sample(...)` as the Final Safety Gate

When several voices are added together, the sum can exceed the normalized range.

Example:

```text
voice A = 0.7
voice B = 0.6
sum     = 1.3
```

That is why the helper clamps to `[-1.0, +1.0]` before storage.

It gives the mixer a simple explicit policy for preventing runaway final values.

## Why the Helper Layer Must Stay Stateless

This file becomes much less useful if it starts learning about:

- whether a voice is active
- what scene is playing
- whether ALSA needs more frames

Those are higher-layer concerns.

The whole power of this file is that its functions remain tiny, pure, and reusable.

## Common Beginner Mistakes

### Mistake 1: Treating phase like output loudness

Phase is only position inside the cycle.

### Mistake 2: Thinking waveform helpers produce finished sound effects

They produce raw oscillator values, not complete voiced sound events.

### Mistake 3: Thinking pan is a backend or speaker API concern

It is still just shared math at this layer.

## Practice

Answer these before moving on:

1. Why does `note 0` map to `0 Hz` in this course?
2. What does phase represent that frequency does not?
3. Why are waveform helpers about shape rather than loudness?
4. Why does pan belong in the helper layer instead of backend code?

## Mini Exercises

### Exercise 1

At `48000 Hz`, what phase step does a `240 Hz` tone use per sample?

### Exercise 2

Describe in one sentence the audible difference you would expect between square and sine.

## Troubleshooting Your Understanding

### "I keep confusing note, frequency, and phase"

Use this chain:

```text
note names the pitch
frequency sets cycles per second
phase says where we are inside one cycle right now
```

### "The helpers still feel disconnected"

Remember that Lesson 20 will start turning these pure values into live voice instances, and Lesson 21 will accumulate them into real chunk output.

## Recap

This lesson built the pure math vocabulary for the rest of Module 05:

1. MIDI notes convert to frequency through `audio_midi_to_freq(...)`
2. waveform helpers map phase to normalized sample values
3. pan splits one mono value into channel contributions
4. clamping keeps the final output inside the safe normalized range

That gives the runtime raw audio values without introducing stateful playback yet.

## Next Lesson

Lesson 20 turns pure audio math into real sound events.

The next question is:

```text
how does one gameplay trigger become one live playback voice
with its own lifetime, phase, and replacement policy?
```

The full audible result also depends on:

- frequency advance
- envelope
- pan
- mix accumulation
- output clamp
- format conversion

That is why these helpers stay small and pure.

## JS-to-C Bridge

This is like a pure helper module that contains note-to-frequency conversion, oscillator shapes, pan multipliers, and output clamping. The benefit is that every higher-level audio system can reuse the same tiny math vocabulary without dragging in runtime state.

## Try Now

Open `src/utils/audio-helpers.h`, then do these checks:

1. Find the formula that maps MIDI note `69` to `440 Hz`.
2. Explain why note `0` maps to silence.
3. Find one waveform helper and explain what its `phase` input means.
4. Find `audio_calculate_pan(...)` and explain what happens at `pan = -1`, `0`, and `+1`.
5. Find `audio_clamp_sample(...)` and explain why it belongs near the final mix boundary.

## Exercises

1. Explain phase without using the word “time.”
2. Explain why waveform helpers return shape values rather than final device-format samples.
3. Explain the difference between panning and changing frequency.
4. Describe the helper layer in one short paragraph.

## Runtime Proof Checkpoint

At this point, you should be able to explain how the runtime turns note numbers, phase positions, and pan values into the raw ingredients that later voice and mixer code will use.

## Recap

This lesson built the pure audio math layer:

- MIDI notes map to frequencies
- phase positions map to waveform samples
- pan maps one sample into left/right multipliers
- clamp keeps the final mix in a safe normalized range

Next, we connect those helpers to live voice state, named sound definitions, and the fixed-size one-shot SFX pool.