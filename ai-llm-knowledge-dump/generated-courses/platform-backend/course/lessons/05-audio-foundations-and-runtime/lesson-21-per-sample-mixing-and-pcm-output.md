# Lesson 21: Per-Sample Mixing, Sequencer Timing, and PCM Output

## Frontmatter

- Module: `05-audio-foundations-and-runtime`
- Lesson: `21`
- Status: Required
- Target files:
  - `src/game/audio_demo.c`
  - `src/utils/audio.h`
  - `src/game/main.c`
- Target symbols:
  - `game_get_audio_samples`
  - `game_app_get_audio_samples`
  - `audio_write_sample`
  - `audio_clamp_sample`
  - `master_volume`
  - `sfx_volume`
  - `music_volume`

## Observable Outcome

By the end of this lesson, you should be able to explain how one requested audio chunk is built frame by frame from active voices, sequencer state, mix layers, clamp logic, and final buffer writes.

## Prerequisite Bridge

Lesson 20 ended with live voices sitting in the pool.

The next unavoidable question is:

```text
how do those active voices become actual stereo frames in the output buffer?
```

Lesson 21 answers that by walking the audio equivalent of a software raster loop.

## Why This Lesson Exists

The runtime now has:

- a shared destination buffer
- pure waveform helpers
- active voice state

But none of that matters until the system can actually synthesize one finished chunk on demand.

That is what `game_get_audio_samples(...)` does.

## The Right Analogy

Treat the mixer as the audio version of the pixel loop.

Rendering says:

```text
for each destination pixel, compute the final color
```

Audio mixing says:

```text
for each destination stereo frame, compute the final left/right values
```

That parallel makes the mixer much easier to trust.

## Step 1: Read the Function Contract First

The entry point is:

```c
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buf,
                            int num_frames)
```

That signature already tells you the layer split:

- `GameAudioState` is the long-lived audio world
- `AudioOutputBuffer` is the transient destination chunk for this call
- `num_frames` is the current request size

This is not a once-per-visual-frame helper.

It is a demand-driven chunk-fill function.

## Step 2: Read the Top-Level Loop Shape

The function clamps the request against capacity, computes `inv_sr`, then does:

```c
for (int i = 0; i < frames; i++) {
  float mix_l = 0.0f;
  float mix_r = 0.0f;
  ...
}
```

That is the central structure of the module.

Each iteration builds one stereo output frame from scratch.

## Visual: One Output Frame Build

```text
start mix_l/mix_r at zero
  + add active SFX voices
  + add music/sequencer contribution
  + apply master volume
  + clamp
  + write into the destination buffer
```

That is the whole mixer in one picture.

## Step 3: Read the SFX Loop as Accumulation, Not Replacement

Inside each output frame, the mixer loops over `MAX_SOUNDS` voices.

For each active voice, it roughly does this:

1. skip inactive slots
2. deactivate slots whose lifetime reached zero
3. advance phase by `frequency * inv_sr`
4. compute the waveform value
5. compute envelope terms
6. compute pan multipliers
7. add the contribution into `mix_l` and `mix_r`

The key word is add.

This is a mixer, not a single-source renderer.

## Step 4: Understand Phase Advance in Concrete Terms

The code does:

```c
gen->phase_acc += gen->frequency * inv_sr;
if (gen->phase_acc >= 1.0f)
  gen->phase_acc -= 1.0f;
```

Meaning:

```text
for one sample step,
advance through the waveform cycle by frequency / sample_rate
```

Higher pitch means larger phase motion per sample.

That is the bridge between musical pitch and raw synthesis timing.

## Worked Example: Simple Phase Motion

At `48000 Hz`, a `480 Hz` tone advances phase by:

$$
\frac{480}{48000} = 0.01
$$

per sample frame.

So after about `100` sample frames, the oscillator has moved through one complete cycle.

## Step 5: Read the Envelope Logic as Click Prevention and Natural Decay

The SFX path computes:

- a full decay factor from `samples_remaining / total_samples`
- an early fade-in based on `fade_in_samples`

That means voices do not jump instantly from silence to full amplitude or from full amplitude to zero.

This is exactly how the runtime avoids obvious note-start and note-end clicks.

## Visual: Why Envelope Math Exists

```text
without envelope:
  abrupt start / abrupt stop

with fade-in + decay:
  smoother attack / smoother release
```

This is a strong design choice even for a small square-wave demo system.

## Step 6: Read Pan and Category Volume as Contribution Shaping

The mixer computes something like:

```c
float sample = wave * gen->volume * env * audio->sfx_volume;
audio_calculate_pan(gen->pan, &pan_l, &pan_r);
mix_l += sample * pan_l;
mix_r += sample * pan_r;
```

So each voice contribution is shaped by:

- waveform value
- voice volume
- envelope
- SFX category volume
- pan multipliers

and only then accumulated into the current frame mix.

## Step 7: The Sequencer Advances Inside the Sample Loop

This is one of the most important live-branch facts.

The music sequencer now advances note timing inside `game_get_audio_samples(...)`, not primarily in `game_audio_update(...)`.

That means note boundaries occur on exact sample boundaries.

## Why That Matters

```text
game loop timing   -> coarse, around once per frame
sample loop timing -> exact at audio-sample granularity
```

The branch deliberately gives the sequencer the more precise clock.

This is a good runtime design decision, not an incidental implementation detail.

## Step 8: Read the Music Tone as a Second Accumulation Layer

After SFX voices, the mixer evaluates the sequencer's persistent `MusicTone`:

- maybe advance to the next note step
- update smoothstep fade state
- synthesize the current waveform sample
- apply music volume and pan
- add that contribution into `mix_l` and `mix_r`

So the overall mix is layered:

```text
all active one-shot voices
plus
the current persistent music tone
```

## Step 9: Read the Final Mix Boundary Carefully

At the end of each output-frame iteration, the code does:

```c
float scale = audio->master_volume;
audio_write_sample(buf, i, audio_clamp_sample(mix_l * scale),
                   audio_clamp_sample(mix_r * scale));
```

That is the clean final boundary of the mixer.

The pipeline is:

```text
sum contributions
apply master volume
clamp to safe normalized range
store in device-format buffer
```

That is the right point for the shared helper boundary from Lesson 18.

## Step 10: Tail Clearing Is Deterministic Hygiene

At the end of the function, if fewer than `max_sample_count` frames were written, the remaining buffer tail is zero-filled.

That matters for the same reason frame clears matter in rendering:

```text
do not let stale previous-call data masquerade as current output
```

It is easy to overlook, but it is exactly the kind of cleanup that keeps runtime behavior trustworthy.

## Step 11: Notice the App Facade Remains Thin

`src/game/main.c` implements:

```c
game_app_get_audio_samples(game, buf, num_frames)
  -> game_get_audio_samples(&game->audio, buf, num_frames)
```

That thin facade is a good sign.

It means the backend only sees the app-level contract, while the actual mixer remains a pure game-side concern.

## Common Beginner Mistakes

### Mistake 1: Thinking one voice replaces the current mix

Voices are accumulated, not swapped in one at a time.

### Mistake 2: Thinking the sequencer is only a frame-based system

On this branch, note stepping is sample-accurate inside the mixer.

### Mistake 3: Thinking tail zero-fill is optional cleanup fluff

It prevents stale data from leaking across calls.

## Practice

Answer these before moving on:

1. Why is the mixer better thought of as an audio raster loop?
2. Why does phase advance depend on `frequency / sample_rate`?
3. Why does the mixer apply master volume late rather than inside each voice helper?
4. Why is sequencer stepping inside the sample loop more precise than frame-based stepping?

## Mini Exercises

### Exercise 1

Explain the order of operations for one output frame from memory.

### Exercise 2

If the current request asks for `735` frames but the buffer can hold `4096`, why is zero-filling the remainder a sensible policy?

## Troubleshooting Your Understanding

### "The mixer still feels big"

Reduce it to this checklist:

```text
start at zero
add SFX
add music
scale
clamp
write
```

### "I keep forgetting which code is chunk-local and which code is persistent"

Remember:

```text
mix_l/mix_r -> chunk-local per frame
GameAudioState -> persistent across many calls
```

## Recap

This lesson showed how the runtime actually synthesizes one chunk:

1. `game_get_audio_samples(...)` fills the output buffer one stereo frame at a time
2. active SFX voices are accumulated through waveform, envelope, pan, and category-volume math
3. the music sequencer advances at sample time and adds its own contribution
4. master volume, clamping, and buffer writes happen at the final boundary

That gives the backends something concrete to drain in the next lesson.

## Next Lesson

Lesson 22 leaves synthesis and moves back to platform-shell responsibility.

The next question is:

```text
how do Raylib and ALSA decide when they need more audio,
and how do they ask the app to provide it?
```

If `frames < buf->max_sample_count`, the function clears the unused tail of the buffer.

That prevents stale data from previous buffer fills from being misread later as current audio.

This is the audio equivalent of deterministic clear behavior in rendering.

## Step 10: Read the App-Level Wrapper in `game/main.c`

The backend does not dig through `GameAppState` internals itself.

Instead it calls:

```c
void game_app_get_audio_samples(GameAppState *game, AudioOutputBuffer *buf,
                                int num_frames) {
  if (!game || !buf)
    return;
  game_get_audio_samples(&game->audio, buf, num_frames);
}
```

That preserves the same facade pattern used everywhere else in the runtime.

## Common Mistake: Thinking `game_audio_update(...)` Is the Main Mixer

On this branch, it is not.

`game_audio_update(...)` still matters, but its job is narrower, such as frequency slides on active SFX voices.

The actual PCM production and sample-accurate sequencing happen inside `game_get_audio_samples(...)`.

That distinction is important for source parity.

## JS-to-C Bridge

This is like an explicit audio callback that accumulates all active sources into left/right sums one sample frame at a time. The crucial lesson is that the sample loop is the authoritative place where final output is assembled.

## Try Now

Open `src/game/audio_demo.c` and `src/game/main.c`, then do these checks:

1. Find where `frames` is clamped against `buf->max_sample_count`.
2. Find where `mix_l` and `mix_r` are reset for each output frame.
3. Find the phase-advance code for one SFX voice.
4. Find the envelope logic and explain the fade-in plus decay roles.
5. Find the final `audio_write_sample(...)` call and explain what has already happened to the sample by then.

## Exercises

1. Explain why the mixer starts each output frame with `mix_l = 0` and `mix_r = 0`.
2. Explain why sample-accurate sequencer stepping belongs in the sample loop.
3. Explain the difference between category volume and master volume.
4. Describe the full `game_get_audio_samples(...)` pipeline in order.

## Runtime Proof Checkpoint

At this point, you should be able to explain exactly how one requested output chunk is built sample frame by sample frame from active SFX voices, sequencer state, volume layers, clamp logic, and final buffer writes.

## Recap

This lesson established the mixer core:

- one output frame is assembled at a time
- active SFX voices add into the current stereo mix
- the sequencer advances at sample precision inside the same loop
- master volume, clamp, and buffer-write helpers form the final output boundary

Next, we move out of the game-side mixer and into the backend drain loops that decide when the device needs more audio and how focus or underrun events affect playback.
