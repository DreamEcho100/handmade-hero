# Lesson 18: Shared Audio Contract, Sample Frames, and Buffer Boundaries

## Frontmatter

- Module: `05-audio-foundations-and-runtime`
- Lesson: `18`
- Status: Required
- Target files:
  - `src/utils/audio.h`
  - `src/platform.h`
  - `src/game/main.h`
- Target symbols:
  - `AUDIO_SAMPLE_RATE`
  - `AUDIO_CHANNELS`
  - `AUDIO_CHUNK_SIZE`
  - `AudioSampleFormat`
  - `AudioOutputBuffer`
  - `audio_format_bytes_per_sample`
  - `audio_write_sample`
  - `audio_read_sample`
  - `GameAudioState`
  - `PlatformAudioConfig`
  - `game_app_get_audio_samples`

## Observable Outcome

By the end of this lesson, you should be able to explain one audio chunk request with the same confidence you can explain one backbuffer:

- how many stereo frames are being requested now
- how those frames are laid out in memory
- what format the destination storage currently uses
- which state belongs only to this output chunk and which state persists across calls

## Prerequisite Bridge

Module 04 ended by making one visual frame trustworthy.

Module 05 starts by defining the audio equivalent of a trustworthy frame boundary:

```text
one shared buffer contract
that both the game layer and the backend understand
```

Before voices, waveform math, or ALSA details can make sense, that boundary has to be explicit.

## Why This Lesson Exists

The rendering side already taught a crucial runtime rule:

```text
the game should target a shared contract,
not talk directly to each backend API
```

Audio needs the same rule.

That is why this lesson starts in `src/utils/audio.h` and not in Raylib or ALSA code.

## Step 1: Start With the Live Source of Truth

At the top of `src/utils/audio.h`, the active branch defines:

```c
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_CHANNELS 2
#define AUDIO_CHUNK_SIZE 4096
```

Those are the live runtime facts.

Use them as the ground truth even if older historical comments elsewhere still mention `44100`.

This branch currently teaches:

- `48000 Hz`
- stereo
- chunk requests up to `4096` stereo frames

## Step 2: Keep Three Units Separate

Most beginner confusion disappears if you keep these units from collapsing into one another:

```text
channel sample -> one left or one right value
sample frame   -> one left + one right together
chunk          -> many sample frames requested in one call
```

That distinction matters in every later lesson.

## Visual: Three Stereo Frames in Memory

```text
frame 0 -> L0 R0
frame 1 -> L1 R1
frame 2 -> L2 R2

flat interleaved memory:
[L0, R0, L1, R1, L2, R2]
```

This is why the runtime often counts in frames instead of raw scalar sample values.

## Worked Example: What One Chunk Means

With `AUDIO_CHUNK_SIZE 4096`, one maximum chunk request means:

$$
4096 \text{ stereo frames}
$$

which also means:

$$
8192 \text{ individual channel samples}
$$

If the storage format is `AUDIO_FORMAT_F32`, each stereo frame uses:

$$
4 \text{ bytes for left} + 4 \text{ bytes for right} = 8 \text{ bytes}
$$

So a full chunk payload is:

$$
4096 \times 8 = 32768 \text{ bytes}
$$

That arithmetic should feel routine by the end of this lesson.

## Step 3: Read `AudioSampleFormat` as a Storage Decision

The enum is:

```c
typedef enum {
  AUDIO_FORMAT_I16 = 0,
  AUDIO_FORMAT_I32,
  AUDIO_FORMAT_F32,
  AUDIO_FORMAT_F64,
} AudioSampleFormat;
```

This enum does not change the mixer's conceptual math.

The mixer still thinks in normalized float values.

The enum answers a narrower boundary question:

```text
how is the destination buffer physically storing stereo frames right now?
```

That is exactly the kind of information a shared audio contract should expose.

## Step 4: `audio_format_bytes_per_sample(...)` Returns Bytes Per Stereo Frame

This helper is easy to misread.

It returns bytes per stereo frame, not bytes per single channel sample.

Examples:

- `AUDIO_FORMAT_I16` -> `sizeof(int16_t) * 2` -> `4` bytes
- `AUDIO_FORMAT_F32` -> `sizeof(float) * 2` -> `8` bytes
- `AUDIO_FORMAT_F64` -> `sizeof(double) * 2` -> `16` bytes

That is why `platform_game_props_init(...)` in `src/platform.h` can allocate one buffer large enough for the biggest supported format.

## Step 5: Read `AudioOutputBuffer` as the Audio Equivalent of `Backbuffer`

The central struct is:

```c
typedef struct {
  int   samples_per_second;
  int   sample_count;
  int   max_sample_count;
  void *samples_buffer;
  AudioSampleFormat format;
  int   is_initialized;
} AudioOutputBuffer;
```

Treat this as the sound-side twin of the rendering backbuffer.

## Rendering Parallel

Keep this comparison active:

```text
Backbuffer
  -> one CPU destination image for this render pass

AudioOutputBuffer
  -> one CPU destination chunk for this audio fill pass
```

Different media, same architectural role.

## Step 6: Separate `sample_count` and `max_sample_count`

These two fields are related, but they are not duplicates.

`sample_count`

- how many stereo frames this call is expected to fill

`max_sample_count`

- total capacity of the buffer allocation

That means the runtime can ask for fewer frames than the buffer can hold.

This matters because backend demand changes over time, especially in the X11/ALSA path.

## Visual: Capacity vs Current Request

```text
buffer capacity: 4096 frames
current request: 735 frames

meaning:
  use the same allocation
  fill only the requested prefix this call
```

That is a normal runtime situation, not an error.

## Step 7: `audio_write_sample(...)` and `audio_read_sample(...)` Are the Buffer Boundary

The game-side mixer does not cast `samples_buffer` directly.

Instead it goes through shared helpers:

- `audio_write_sample(...)`
- `audio_read_sample(...)`

This is a clean design choice.

It means the mixer always reasons in normalized float values while the helper layer handles physical storage conversion.

## Why This Matters

Without these helpers, synthesis code would have to care about whether the current backend wanted:

- 16-bit integers
- 32-bit floats
- some other storage format

That would leak backend concerns into game-side mixing code.

The helpers prevent that leak.

## Step 8: Read `GameAudioState` as Persistent Audio World State

Later in `src/utils/audio.h`, `GameAudioState` owns:

- the fixed SFX voice pool
- the music sequencer
- sample-rate knowledge
- master, music, and SFX mix levels

This is not one output chunk.

It is the long-lived audio world that survives across many buffer fills.

## Step 9: Read `PlatformAudioConfig` as Backend-Owned Device Context

`PlatformAudioConfig` stores facts like:

- sample rate
- channel count
- chunk size
- opaque Raylib stream pointer
- opaque ALSA PCM handle

This struct is shared at the contract boundary, but many of its fields belong to backend-owned device state rather than synthesis state.

That distinction becomes important in Lesson 22.

## Step 10: Notice the App-Level Facade Boundary

`src/game/main.h` exposes:

```c
void game_app_get_audio_samples(GameAppState *game, AudioOutputBuffer *buf,
                                int num_frames);
```

and `src/game/main.c` implements it as:

```c
game_get_audio_samples(&game->audio, buf, num_frames);
```

That is the audio-side facade boundary.

Backends do not reach directly into the voice pool or sequencer.

They ask the app to fill the shared contract.

## Common Beginner Mistakes

### Mistake 1: Treating one stereo frame as one scalar sample

In stereo, one frame contains two channel values.

### Mistake 2: Treating `sample_count` like allocation size

It is the current request size, not the total capacity.

### Mistake 3: Thinking the game should write raw device format directly

The shared helper layer exists so the mixer can stay in normalized float space.

## Practice

Answer these before moving on:

1. What is the difference between one channel sample and one stereo frame?
2. Why is `audio_format_bytes_per_sample(...)` really bytes per stereo frame?
3. Why are `sample_count` and `max_sample_count` separate fields?
4. Why does `game_app_get_audio_samples(...)` make the architecture cleaner?

## Mini Exercises

### Exercise 1

How many individual channel values are contained in a request for `256` stereo frames?

### Exercise 2

At `AUDIO_FORMAT_F32`, how many bytes are needed for `1024` stereo frames?

## Troubleshooting Your Understanding

### "I keep mixing up frames and samples"

Use this rule:

```text
stereo frame = left + right together
```

### "I still do not see the parallel with rendering"

Both systems are built around the same idea:

```text
shared CPU-side destination
filled by game-side code
later drained/presented by backend code
```

## Recap

This lesson established the whole audio module's contract boundary:

1. live audio constants define the active runtime, including `48000 Hz` and `4096`-frame chunk capacity
2. `AudioSampleFormat` describes storage format, not synthesis meaning
3. `AudioOutputBuffer` is the audio equivalent of the backbuffer
4. `GameAudioState` is persistent audio world state, while one buffer fill is transient work
5. `game_app_get_audio_samples(...)` gives backends one clean facade boundary

That gives the rest of Module 05 a stable place to stand.

## Next Lesson

Lesson 19 leaves storage and ownership behind and focuses on value generation.

The next question is:

```text
once the runtime has a shared output buffer,
where do the actual sample values come from?
```

## Step 5: Read `audio_write_sample(...)` as the Final Format Boundary

The helper takes normalized float left/right values, clamps them, then writes them into `samples_buffer` according to `buf->format`.

That is a very important design choice.

It means the mixer can think in one conceptual sample format:

```text
normalized float audio in roughly [-1, +1]
```

and let the buffer helper deal with the last-mile storage conversion.

## Why This Boundary Is Strong

If every mixer path had to manually cast and scale into every PCM format, the audio code would become harder to read immediately.

Instead the course isolates that complexity at the buffer boundary.

## Step 6: Read `audio_read_sample(...)` as the Reverse Conversion Boundary

The matching helper takes one frame from raw storage and converts it back into normalized float left/right values.

That preserves symmetry:

```text
float mix domain -> write helper -> device/storage format
device/storage format -> read helper -> float mix domain
```

Even when one direction is used more heavily, the contract stays balanced.

## Where `audio_demo.h` Fits

The target file `src/game/audio_demo.h` is useful here because it exposes the game-facing audio entry points:

- `game_audio_init_demo(...)`
- `game_get_audio_samples(...)`
- `game_audio_update(...)`

Those functions operate on persistent `GameAudioState`, not on backend device handles.

That is another clue that the shared audio layer lives above the backend-specific drain code.

## Step 7: Separate Long-Lived Audio State From One Output Chunk

The same header also defines `GameAudioState`.

That is not the same thing as `AudioOutputBuffer`.

The split is:

```text
GameAudioState
  -> long-lived audio world: voices, sequencer, volumes

AudioOutputBuffer
  -> one requested chunk of destination memory to fill right now
```

This is a crucial architecture distinction.

The game owns the evolving audio state.

The backend owns the timing and device need for output chunks.

## Chunk State vs Persistent State

This is the distinction to keep repeating until it feels boring:

```text
AudioOutputBuffer
  -> this fill request right now

GameAudioState
  -> evolving music, voices, ages, volumes, sequencer state across frames

PlatformAudioConfig
  -> device-facing configuration and opaque backend handles
```

That three-way split is the real architecture lesson here.

## Step 8: Read `PlatformAudioConfig` as the Platform Companion

`PlatformAudioConfig` stores platform-facing details such as:

- sample rate
- channels
- chunk size
- Raylib stream handle pointer
- ALSA PCM handle pointer
- ALSA buffer size

That means the audio architecture already has the same kind of split you saw in rendering:

```text
shared contract + game-side state + backend-side device details
```

That is why the audio module can stay portable across both backends.

## Common Mistake: Thinking the Output Buffer Is the Audio System

It is only the handoff surface.

The real audio system includes:

- long-lived game-side audio state
- helper math
- mixing logic
- backend device interaction

The buffer is the bridge between those worlds, not the whole story.

## JS-to-C Bridge

This is like separating an internal audio state object from the specific output buffer that an audio callback asks you to fill right now. The state persists. The buffer request is transient.

## Try Now

Open `src/utils/audio.h` and `src/game/audio_demo.h`, then do these checks:

1. Find the live audio constants and explain what each one means.
2. Explain what one stereo sample frame contains.
3. Find `audio_format_bytes_per_sample(...)` and explain why it returns bytes per frame rather than bytes per channel.
4. Find `AudioOutputBuffer` and explain the difference between `sample_count` and `max_sample_count`.
5. Explain why `GameAudioState` and `PlatformAudioConfig` are different kinds of state.

## Exercises

1. Explain why audio needs a shared contract just like rendering needed a backbuffer contract.
2. Explain why normalized float mixing and device-format storage should stay separate.
3. Explain why one audio chunk buffer is not the same thing as the persistent audio system.
4. Describe the audio boundary architecture in one paragraph.

## Runtime Proof Checkpoint

At this point, you should be able to explain what one audio frame is, what one output chunk is, and why the game-side mixer should talk to a shared buffer contract instead of directly to Raylib or ALSA.

## Recap

This lesson established the shared audio boundary:

- the live runtime uses `48000` Hz stereo with `4096`-frame chunks
- `AudioSampleFormat` describes physical sample storage
- `AudioOutputBuffer` is the audio equivalent of the backbuffer contract
- `audio_write_sample(...)` and `audio_read_sample(...)` isolate format conversion
- `GameAudioState` and `PlatformAudioConfig` separate persistent game audio from backend device details

Next, we look at the pure math layer that turns notes, phase positions, and pan values into raw audio ingredients.
