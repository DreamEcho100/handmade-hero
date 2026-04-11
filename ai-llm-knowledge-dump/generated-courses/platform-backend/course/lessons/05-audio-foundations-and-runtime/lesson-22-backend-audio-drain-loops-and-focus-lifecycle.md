# Lesson 22: Backend Drain Loops, Device Demand, and Focus Lifecycle

## Frontmatter

- Module: `05-audio-foundations-and-runtime`
- Lesson: `22`
- Status: Required
- Target files:
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/audio.h`
  - `src/platforms/x11/audio.c`
  - `src/platforms/x11/main.c`
- Target symbols:
  - `init_audio`
  - `process_audio`
  - `platform_audio_init`
  - `platform_audio_get_samples_to_write`
  - `platform_audio_write`
  - `handle_focus_change`
  - `platform_audio_pause_cfg`
  - `platform_audio_resume_cfg`

## Observable Outcome

By the end of this lesson, you should be able to explain how each backend decides when audio is needed, how many frames to request, and why pause/resume and recovery logic stay on the backend side instead of leaking into the mixer.

## Prerequisite Bridge

Lesson 21 ended with a filled `AudioOutputBuffer`.

That still leaves the real device questions unanswered:

```text
when does the backend ask for more audio?
how much should it ask for right now?
how does it submit the result?
what happens on focus loss or underrun?
```

Those are backend-shell responsibilities, not mixer responsibilities.

## Why This Lesson Exists

If the game-side mixer tried to own device timing, stream health, and focus lifecycle, the whole architecture would collapse.

The system stays understandable because it keeps this split:

```text
game side:
  given N frames, fill the shared output buffer

backend side:
  decide when N is needed and push the result to the device
```

That is the main lesson.

## The Shared Pattern First

Both backends follow the same high-level loop:

```text
backend detects demand
backend asks the app to fill the shared buffer
backend submits the filled data to the real device path
```

The API names differ.

The architecture does not.

## Visual: One Backend-Owned Audio Demand Cycle

```text
video frame or host callback point arrives
  -> backend checks stream/device demand
  -> backend chooses N frames to write now
  -> game fills shared buffer for N frames
  -> backend submits those N frames to host audio path
  -> repeat if more demand is already waiting
```

This is the audio equivalent of the render backend's presentation loop.

## Step 1: Keep the Query -> Fill -> Submit Pattern Visible

When reading either backend, keep asking:

```text
how does this backend know audio is needed?
how does it request the fill?
how does it submit the result?
```

That question cuts through a lot of platform-specific detail.

## Step 2: Read the Raylib Init Path as Managed Stream Setup

In `src/platforms/raylib/main.c`, `init_audio(...)` does all of this:

- `InitAudioDevice()`
- `SetAudioStreamBufferSizeDefault(cfg->chunk_size)`
- `LoadAudioStream(...)`
- two silent pre-fills via `UpdateAudioStream(...)`
- `PlayAudioStream(...)`

That is Raylib's stream-opening and priming shell.

## Why the Silent Pre-Fill Matters

Before playback begins, the backend pushes known silence twice.

That means the stream starts from deterministic quiet data rather than stale or late-produced content.

This is a small but real anti-pop measure.

## Step 3: Read the Raylib Drain Loop Carefully

The core loop is:

```c
while (IsAudioStreamProcessed(g_raylib.audio_stream)) {
  game_app_get_audio_samples(game, audio_buf, frames);
  UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples_buffer, frames);
}
```

The word `while` is the key detail.

## Why `while` Matters More Than `if`

Raylib may report more than one ready chunk during a single video frame.

So the backend must keep draining until the stream is caught up.

```text
if    -> maybe refill only one ready chunk
while -> refill every ready chunk now
```

Using `if` here risks starvation.

## Worked Example: Two Ready Buffers

Suppose Raylib reports that two internal buffers are ready during one video frame.

With `if`, the runtime would refill only one and leave the second hungry.

With `while`, it refills both before continuing the frame loop.

That is exactly why the live code uses repeated draining.

## Step 4: Read the X11/ALSA Init Path as Explicit Device Ownership

`src/platforms/x11/audio.c` is much more explicit than Raylib.

It configures:

- device open with `snd_pcm_open(...)`
- interleaved access
- float PCM format
- channel count
- sample rate
- hardware buffer size
- period size
- software start threshold

This is the same overall job as Raylib init, but with less abstraction and more visible device policy.

## Step 5: `platform_audio_get_samples_to_write(...)` Is the Demand Query

This helper asks ALSA:

```c
snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm);
```

That is the correct question.

It does not guess how much audio to write.

It asks the device how much space is actually available without blocking, then clamps the answer to safe limits.

That is strong backend design.

## Step 6: Recovery Belongs in Backend Code

If `avail` is negative or a write fails, the X11 backend uses `snd_pcm_recover(...)`.

That is device-health logic.

The game-side mixer should not know anything about it.

## Why This Separation Matters

If ALSA underruns or a stream needs re-preparation, that is not a question about:

- oscillator phase
- envelope math
- voice lifetimes

It is a question about the health of the device-facing path.

That is exactly why recovery helpers belong in platform code.

## Step 7: Read the X11 Drain Loop as Query -> Fill -> Write

In `src/platforms/x11/main.c`, the flow is:

```c
int frames = platform_audio_get_samples_to_write(cfg);
...
game_app_get_audio_samples(game, audio_buf, frames);
platform_audio_write(audio_buf, frames, cfg);
```

That is the same architecture you saw in Raylib, just with different API calls.

## Visual: Same Architecture, Different Device APIs

```text
Raylib:
  stream ready?
    -> fill buffer
    -> UpdateAudioStream

X11/ALSA:
  frames available?
    -> fill buffer
    -> snd_pcm_writei via platform_audio_write
```

That is the comparison that matters most.

## Step 8: Focus Changes Are Backend Lifecycle, Not Mixer Logic

Raylib handles focus by pausing or resuming the `AudioStream` and toggling `audio_paused`.

X11 handles focus by:

- `platform_audio_pause_cfg(...)` using `snd_pcm_drop(...)`
- `platform_audio_resume_cfg(...)` using `snd_pcm_prepare(...)` and silence prefill

These are window/device lifecycle concerns.

The mixer should not need to know whether the window is focused.

## Visual: Focus Lifecycle Split

```text
window focus lost
  -> backend pauses stream/device
  -> mixer state remains valid but does not own the pause action

window focus regained
  -> backend resumes stream/device
  -> backend may prefill silence
  -> mixer continues producing normal samples on the next demand query
```

That is why focus handling belongs next to the device path rather than inside sound-definition or mixing code.

## Step 9: Resume Also Needs Deterministic Silence

After an ALSA resume, the backend writes silence into the device buffer again.

That applies the same principle as startup priming:

```text
resume from known quiet state,
not from partially undefined stream contents
```

That is a real correctness detail, not polish fluff.

## Step 10: Keep Backend Timing and Game Timing Separate

Module 04 already taught frame timing.

Lesson 22 adds a second demand loop that is backend-owned:

```text
game loop timing decides when update/render happen
audio device timing decides when more sample frames are needed
```

Those are related systems, but not the same clock.

## Common Beginner Mistakes

### Mistake 1: Thinking the mixer decides when more audio is needed

Only the backend can query or observe real device demand.

### Mistake 2: Thinking pause/resume should live in `game_get_audio_samples(...)`

Focus lifecycle is a backend/window concern, not synthesis logic.

### Mistake 3: Thinking Raylib and ALSA use different architectures

They expose different APIs, but the same query -> fill -> submit pattern is still present.

## Practice

Answer these before moving on:

1. Why does Raylib use `while (IsAudioStreamProcessed(...))` instead of a single `if`?
2. Why is `snd_pcm_avail_update(...)` the right question for ALSA to answer?
3. Why does underrun recovery belong in backend code instead of mixer code?
4. Why is resume prefill of silence a sensible policy?

## Mini Exercises

### Exercise 1

Describe the shared three-stage backend pattern from memory without naming any specific API.

### Exercise 2

Explain in one paragraph why pause/resume is a device-lifecycle concern rather than a sound-definition concern.

## Troubleshooting Your Understanding

### "The backend code looks more complex than the mixer"

That is expected. The mixer is shared logic; the backend has to deal with real host APIs and device state.

### "I keep focusing on API names instead of the structure"

Return to this:

```text
detect demand
fill shared buffer
submit to device
```

That is the reusable architecture.

## Recap

This lesson connected the shared mixer to real output devices:

1. Raylib and ALSA both follow the same query -> fill -> submit architecture
2. Raylib manages a stream and drains it with a `while` loop
3. X11 asks ALSA how many frames are available and writes exactly that much
4. focus lifecycle and underrun recovery stay in backend code where they belong

That gives the runtime a real audio drain path without contaminating synthesis code.

## Next Lesson

Lesson 23 adds the last piece of the module: persistent musical identity over time.

The next question is:

```text
how does the runtime keep a long-lived music oscillator and sequencer alive
while still letting scenes change the sound of the world?
```
