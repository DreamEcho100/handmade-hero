# Lesson 23: Music Sequencer, Persistent Tone State, and Scene Audio Profiles

## Frontmatter

- Module: `05-audio-foundations-and-runtime`
- Lesson: `23`
- Status: Required
- Target files:
  - `src/utils/audio.h`
  - `src/game/audio_demo.h`
  - `src/game/audio_demo.c`
  - `src/game/main.c`
- Target symbols:
  - `MusicTone`
  - `MusicSequencer`
  - `game_audio_init_demo`
  - `game_audio_update`
  - `game_audio_apply_scene_profile`
  - `game_audio_trigger_scene_enter`
  - `game_app_get_audio_samples`

## Observable Outcome

By the end of this lesson, you should be able to explain how the runtime keeps long-lived music state across notes, advances sequence timing at audio-sample precision, and swaps scene-specific audio identity without changing the backend-facing contract.

## Prerequisite Bridge

The audio system now has:

- a shared output contract
- helper math
- one-shot voices
- a per-sample mixer
- backend drain loops

What it still needs is higher-level musical behavior that persists over time and changes with scene identity.

That is what the sequencer layer adds.

## Why This Lesson Exists

If the runtime only had one-shot SFX voices, it could produce reactions but not ongoing musical identity.

Lesson 23 adds that identity through three stacked layers:

```text
persistent music oscillator
step-based sequencer state
scene-driven profile selection
```

If those layers stay separate, the code is much calmer than it first looks.

## Step 1: Persistent Music Tone Is Not Just Another SFX Voice

The runtime does not model background music as one more `ToneGenerator` slot.

Instead it defines a dedicated `MusicTone`:

```c
typedef struct {
  float phase;
  float frequency;
  float volume;
  float current_volume;
  float pan;
  int   is_playing;
  int   fade_samples_remaining;
} MusicTone;
```

This is a different category of state from the short-lived SFX pool.

## Why the Persistent Tone Matters

The crucial design choice is phase preservation across note changes.

If every new music note restarted at phase `0`, the waveform could jump discontinuously and click.

So the runtime uses one continuing oscillator whose frequency and envelope change over time.

That is a very important low-level audio decision.

## Worked Example: Why Preserving Phase Prevents Clicks

Imagine the current sine wave is halfway through one cycle when the sequencer changes notes.

If the runtime reset phase to `0`, the sample stream would jump abruptly from:

```text
old sample value -> some non-zero point on the current waveform
new sample value -> the start of the new waveform
```

That discontinuity is exactly the kind of abrupt edge that creates audible clicks.

By preserving phase, the runtime changes pitch and envelope while keeping the waveform continuous.

## Step 2: Keep Two Audio Timelines Separate

This lesson becomes much easier if you separate:

```text
game-loop time   -> coarse scene and SFX state updates
sample-loop time -> exact note boundaries and PCM generation
```

The current branch deliberately gives sequencer stepping to the second timeline.

That means note boundaries are sample-accurate, not frame-quantized.

## Step 3: Read `MusicSequencer` as a Timing State Machine

The struct stores:

- pattern list
- pattern length
- pattern count
- current pattern and step
- samples per step
- samples remaining in the current step
- the persistent `MusicTone`
- whether sequencing is active

This is not only note data.

It is a live timing machine layered on top of the mixer.

## Step 4: Pattern Data Uses MIDI Notes on Purpose

In `src/game/audio_demo.c`, pattern tables are arrays of `uint8_t` MIDI note numbers.

That is a strong design choice because it keeps the musical data:

- compact
- readable
- independent from floating-point frequency literals

And because Lesson 19 already gave the runtime `audio_midi_to_freq(...)`, the conversion boundary stays clean.

The value `0` still means rest.

## Worked Example: Step Duration on the Live Branch

At startup, `game_audio_init_demo(...)` uses:

```c
seq->step_duration_samples = (int)(0.15f * (float)AUDIO_SAMPLE_RATE);
```

With `AUDIO_SAMPLE_RATE 48000`, that means:

$$
0.15 \times 48000 = 7200 \text{ samples per step}
$$

That is the correct live-branch number to keep in mind.

## Step 5: Step Duration Is Stored in Samples for a Reason

The runtime stores step duration in sample units, not frame units.

That is a huge architecture clue.

It means musical timing belongs naturally inside the sample loop.

This is more precise than saying "advance the note every visual frame or two."

## Visual: One Sequencer Step in Sample Time

```text
samples_remaining = 7200
  -> emit sample
  -> samples_remaining--
  -> emit sample
  -> samples_remaining--
  -> ...
  -> samples_remaining reaches 0
  -> advance to next step
  -> choose next note or rest
  -> reset samples_remaining for the new step
```

This is why the live branch keeps note advancement inside the sample loop. The countdown belongs to the same timeline as the samples themselves.

## Step 6: `game_audio_update(...)` Is No Longer the Main Sequencer Clock

This is a critical current-branch parity point.

`game_audio_update(...)` still exists, but on the live branch it mainly handles coarse frame-level audio state such as SFX frequency slides.

The actual note-step advancement for the sequencer now happens inside `game_get_audio_samples(...)`.

So the correct modern summary is:

```text
game_audio_update(...) handles coarse state
game_get_audio_samples(...) handles sample-accurate sequencer timing
```

That distinction matters a lot.

## Step 7: Read `game_audio_init_demo(...)` as the Boot Audio Profile

At startup, this function initializes:

- sample rate
- master, music, and SFX volumes
- initial pattern set
- initial step duration
- tone pan and volume
- playback enabled state

This is not just initialization boilerplate.

It gives the whole runtime a coherent starting musical identity.

## Step 8: Read `game_audio_apply_scene_profile(...)` as Audio Identity Switching

This function chooses, based on `scene_index`:

- which pattern set to use
- how many patterns are active
- music and SFX volume balance
- tone volume
- sequencer step duration

So it is doing much more than "pick a song."

It is selecting the scene's whole audio personality.

## Visual: Scene Change Means Audio Profile Change

```text
scene changes
  -> choose pattern set
  -> choose mix balance
  -> choose step timing
  -> reset sequencer state
```

That is how the runtime makes scenes sound distinct without inventing a separate music engine per scene.

## Step 9: Notice How Thoroughly the Profile Reset Rebuilds Sequencer State

`game_audio_apply_scene_profile(...)` does not merely tweak one variable.

It:

- clears one-shot voices
- swaps pattern lists
- resets current pattern and step
- resets step timing and countdown
- resets tone state
- updates mix levels

That is effectively an audio-side scene rebuild.

## Step 10: `game_audio_trigger_scene_enter(...)` Is the Immediate Cue Layer

After the profile is chosen, the runtime can also play an entry cue:

- low tone for one scene
- mid tone for another
- high tone for another
- layered cue for the slab/audio lab

This separation is important.

Profile selection and immediate scene-entry feedback are different jobs, and the code keeps them separate.

## Step 11: Connect It Back to `game/main.c`

The app-level hooks are straightforward:

- `game_audio_init_demo(&game->audio);` during app init
- `game_audio_update(&game->audio, dt * 1000.0f);` during frame update
- `game_app_get_audio_samples(...)` as the backend-facing fill facade

`src/game/main.c` also triggers sound events and scene-profile changes as part of scene transitions and runtime probes.

That means the app layer decides _when_ audio identity should change, while the shared audio subsystem decides _how_ to render that identity.

## Common Beginner Mistakes

### Mistake 1: Treating background music as just another disposable one-shot voice

The persistent `MusicTone` exists specifically to preserve continuity across note changes.

### Mistake 2: Thinking the sequencer still mainly advances in `game_audio_update(...)`

On the live branch, note timing advances inside the sample loop.

### Mistake 3: Thinking scene profile changes only swap pattern tables

They also reset timing state and mix identity.

## Practice

Answer these before moving on:

1. Why is phase preservation important for the music tone?
2. Why is storing step duration in samples better than storing it in visual frames?
3. Why does scene profile selection do more than choose a melody?
4. Why is it useful to separate scene-entry cues from scene-profile setup?

## Mini Exercises

### Exercise 1

At `48000 Hz`, how many samples long is a `0.09` second sequencer step?

### Exercise 2

Explain in one short paragraph why sample-accurate note boundaries are more trustworthy than frame-quantized boundaries.

## Troubleshooting Your Understanding

### "The sequencer feels like a different system from the mixer"

It is layered on top of the mixer, not separate from it. The sequencer decides what note state should be true; the mixer turns that state into actual samples.

### "Scene audio identity still feels abstract"

Focus on what the profile function concretely changes: patterns, step timing, tone volume, and music/SFX balance. That is already enough to make scenes audibly distinct.

## Recap

This lesson finished the audio architecture by adding long-lived musical identity:

1. `MusicTone` preserves oscillator continuity across note changes
2. `MusicSequencer` stores step timing and pattern progression state
3. sequencer note advancement happens at sample time inside the mixer
4. scene profile functions rebuild the audio identity without changing backend contracts

That completes Module 05's audio pipeline from shared contract to scene-driven runtime behavior.

## End of Module 05

At this point, the runtime can explain sound at every major layer:

```text
shared buffer contract
pure synthesis math
live voice state
per-sample mixing
backend drain timing
scene-driven music identity
```

That is enough audio infrastructure for the later scene labs to use sound as real runtime evidence rather than decoration.

## Next Module Bridge

Module 06 shifts back from time to space.

The new question is:

```text
once the runtime can explain both image and sound pipelines,
how does it explain the coordinate systems that place objects,
camera motion, and HUD content in space?
```

The scene-profile and scene-enter hooks you studied here will reappear later when the course opens scene-management control flow in more detail. For now, the important carry-forward idea is simpler: audio already follows the same pattern as the rest of the runtime, with shared contracts underneath and scene-aware meaning layered on top.
