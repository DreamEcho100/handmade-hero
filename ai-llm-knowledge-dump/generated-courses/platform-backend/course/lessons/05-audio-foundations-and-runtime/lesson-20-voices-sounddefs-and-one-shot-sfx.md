# Lesson 20: `SoundDef`, `ToneGenerator`, and the Fixed Voice Pool

## Frontmatter

- Module: `05-audio-foundations-and-runtime`
- Lesson: `20`
- Status: Required
- Target files:
  - `src/utils/audio.h`
  - `src/game/audio_demo.c`
  - `src/game/main.c`
- Target symbols:
  - `ToneGenerator`
  - `SoundDef`
  - `SoundID`
  - `MAX_SOUNDS`
  - `SOUND_DEFS`
  - `game_play_sound_at`
  - `next_age`

## Observable Outcome

By the end of this lesson, you should be able to explain how one gameplay event becomes one active voice slot with its own lifetime, phase, and replacement age.

## Prerequisite Bridge

Lesson 19 gave the runtime pure waveform and pitch math.

That is still not a usable game audio system.

The runtime now needs at least three more things:

- named sounds
- live playback instances
- a policy for what happens when too many sounds want to play at once

Lesson 20 adds those three things.

## Why This Lesson Exists

Games do not trigger anonymous equations.

They trigger named sound events like:

```text
play the low tone now
play the high tone now
play the scene-enter cue now
```

So the runtime needs a clean bridge from static recipe data to live playback state.

## The Recipe-to-Instance Model

Read the whole lesson through this one chain:

```text
SoundID -> SoundDef -> ToneGenerator -> voice pool slot
```

That is the actual architectural story.

## Step 1: Separate Static Recipe From Live Playback State

`SoundDef` is the static recipe:

```c
typedef struct {
  float frequency;
  float freq_slide;
  float volume;
  float pan;
  int   duration_ms;
} SoundDef;
```

This says what a sound should be when triggered.

It does not contain runtime playback progress.

`ToneGenerator` is the live playback instance:

- current phase
- current frequency
- remaining lifetime
- fade-in window
- active flag
- age for replacement policy

That distinction is the center of the lesson.

## Visual: Static vs Live

```text
SoundDef
  -> reusable recipe table

ToneGenerator
  -> one active playback instance with evolving state
```

If those two layers blur together, the voice system becomes much harder to reason about.

## Step 2: Read the Named Sound Surface

The header defines symbolic sound IDs such as:

```c
SOUND_TONE_LOW
SOUND_TONE_MID
SOUND_TONE_HIGH
```

and also defines:

```c
#define MAX_SOUNDS 8
```

That gives the runtime two explicit design decisions:

- gameplay code can trigger named sounds instead of hand-assembling frequencies every time
- the number of simultaneous one-shot SFX voices is capped

That cap is a feature, not a weakness.

## Step 3: A Fixed Voice Pool Is a Deliberate Systems Choice

The runtime could allocate a new object every time a sound starts.

It does not.

Instead it stores a fixed array of `ToneGenerator` voices inside `GameAudioState`.

That gives the runtime:

```text
predictable memory usage
predictable ownership
no per-sound heap churn during playback
```

This is a classic game-runtime tradeoff.

## Step 4: Read the Actual Recipe Table

`src/game/audio_demo.c` contains:

```c
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
  ...
};
```

Each entry encodes one reusable sound recipe.

That means gameplay code can trigger `SOUND_TONE_LOW` without rebuilding frequency, volume, pan, and duration data manually.

## Worked Example: Converting Duration to Sample Lifetime

On the live branch, audio runs at `48000 Hz`.

So a `300 ms` sound duration becomes:

$$
0.3 \times 48000 = 14400 \text{ stereo frames}
$$

That is exactly the kind of conversion `game_play_sound_at(...)` must perform when it fills `samples_remaining` and `total_samples`.

## Step 5: Walk Through `game_play_sound_at(...)`

The function's job is simple but important:

1. validate the requested sound ID
2. find a voice slot
3. convert the static recipe into live runtime state
4. leave the slot ready for the next audio fill

That one trigger path is the whole lesson in miniature.

## Step 6: Read the Slot Selection Policy Carefully

The policy is:

```text
prefer the first inactive slot
if all slots are active, steal the oldest one
```

That gives a deterministic answer when the pool is full.

It is not the only possible replacement rule, but it is simple, visible, and easy to teach.

## Why `age` Exists

The pool needs a way to define "oldest."

That is what these fields do:

- `ToneGenerator.age`
- `GameAudioState.next_age`

Each time a voice is started, it receives the next monotonic age value.

So oldest means the lowest age among currently active voices.

## Step 7: Read the Initialization Block as Recipe Expansion

Once a slot is chosen, `game_play_sound_at(...)` copies recipe data into live state.

Important details:

- `phase_acc = 0.0f` starts the oscillator at the beginning of a cycle
- `frequency`, `freq_slide`, `volume`, and `pan` copy directly from the recipe
- `duration_ms` is converted into a sample-frame countdown
- `total_samples` preserves the original lifetime for later decay-envelope math
- `fade_in_samples = 88` adds a short attack ramp to reduce click artifacts
- `age = audio->next_age++` records relative start order

That is the precise bridge from recipe to active playback instance.

## Visual: Trigger Path

```text
gameplay event
  -> choose SoundID
  -> lookup SoundDef
  -> find free slot or steal oldest
  -> fill ToneGenerator runtime fields
  -> next chunk fill renders the active voice
```

That is the real architecture.

## Step 8: Why `game_play_sound_at(...)` Does Not Generate Samples Directly

This function does not emit PCM immediately.

That is the correct design.

Triggering a sound and synthesizing a chunk are different jobs:

```text
trigger path -> mutate long-lived audio state
mix path     -> read long-lived audio state and synthesize output
```

That separation keeps the runtime coherent.

## Step 9: Notice How the App Layer Uses It

`src/game/main.c` calls `game_play_sound_at(...)` in response to scene and input events, for example when:

- Space is pressed in particular scenes
- a level reload probe is triggered
- a scene-entry cue is emitted

That means gameplay code only chooses *when* to trigger sound events.

It does not take over PCM synthesis.

## Common Beginner Mistakes

### Mistake 1: Treating `SoundDef` and `ToneGenerator` as the same kind of object

One is static recipe data. The other is live playback state.

### Mistake 2: Thinking the fixed pool is only an optimization trick

It is also an ownership and predictability design choice.

### Mistake 3: Thinking trigger functions should write into the audio buffer directly

That would blur long-lived state mutation with chunk synthesis.

## Practice

Answer these before moving on:

1. Why is `SoundDef` not enough to represent a sound that is actively playing?
2. Why is `age` necessary once the pool has a finite size?
3. Why is it useful to convert duration into sample-frame countdowns early?
4. Why should `game_play_sound_at(...)` mutate voice state instead of generating PCM immediately?

## Mini Exercises

### Exercise 1

If a `200 ms` sound runs at `48000 Hz`, how many stereo frames does it last?

### Exercise 2

Explain the difference between "steal oldest" and "always reuse slot 0" in terms of runtime behavior.

## Troubleshooting Your Understanding

### "I still picture sounds as one-off function calls rather than stateful objects"

Remember that audible output happens later, inside chunk generation. A sound trigger only schedules and configures future playback.

### "The pool feels arbitrary"

It is constrained on purpose. Explicit limits make lifetime and replacement policy visible, which is exactly what this course wants to teach.

## Recap

This lesson turned pure audio math into stateful game audio events:

1. `SoundDef` provides static reusable sound recipes
2. `ToneGenerator` stores live playback state
3. `game_play_sound_at(...)` converts one recipe into one active voice slot
4. the fixed voice pool makes memory and replacement policy explicit

That gives the mixer something real to render in the next lesson.

## Next Lesson

Lesson 21 answers the next unavoidable question:

```text
once active voices exist,
how does the runtime turn them into one finished chunk of PCM output?
```

Its job is to prepare voices for later sample rendering.

It should not mix PCM immediately because the backend asks for audio in chunks on its own schedule.

So the correct split is:

```text
game_play_sound_at -> mutate voice state now
game_get_audio_samples -> synthesize sample frames later when requested
```

That separation makes the runtime robust.

## Common Mistake: Treating the Voice Pool as an Effect Queue

It is not only a list of requests.

Each `ToneGenerator` is already a live playback object with current phase, remaining lifetime, and other evolving synthesis state.

That is why the later mixer can step through voices directly without reconstructing them every sample.

## JS-to-C Bridge

This is like the difference between a constant sound preset table and a pool of active playback instances. Presets describe what should happen. Active instances store what is happening right now.

## Try Now

Open `src/utils/audio.h` and `src/game/audio_demo.c`, then do these checks:

1. Find the difference between `SoundDef` and `ToneGenerator` in the headers.
2. Find `MAX_SOUNDS` and explain what fixed resource limit it represents.
3. Find the `SOUND_DEFS` table and explain why sound IDs are useful.
4. Find the slot-selection loop in `game_play_sound_at(...)` and explain the free-slot then steal-oldest policy.
5. Find the initialization of `samples_remaining`, `total_samples`, and `fade_in_samples` and explain why each exists.

## Exercises

1. Explain why a sound recipe and a live voice need different structs.
2. Explain why a fixed pool is a good educational and runtime design here.
3. Explain why `age` is needed when all voice slots are occupied.
4. Describe the one-shot trigger pipeline from `SoundID` to active voice.

## Runtime Proof Checkpoint

At this point, you should be able to explain how the runtime turns a named sound effect into an active `ToneGenerator` instance ready for the next audio buffer fill.

## Recap

This lesson turned pure audio math into a usable SFX architecture:

- `SoundDef` stores reusable sound recipes
- `ToneGenerator` stores live playback state
- `SoundID` gives gameplay code symbolic sound names
- a fixed pool of `MAX_SOUNDS` voices handles one-shot playback
- `game_play_sound_at(...)` bridges from recipe to active voice

Next, we follow those active voices into the sample loop that turns them into actual PCM output.