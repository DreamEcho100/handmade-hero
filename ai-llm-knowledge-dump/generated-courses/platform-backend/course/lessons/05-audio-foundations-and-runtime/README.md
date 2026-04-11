# Module 05: Audio Foundations and Runtime

Module 04 made the frame loop trustworthy.

Module 05 does the same for sound.

That means the goal here is not merely "play a tone."

The goal is to understand how the runtime can:

- describe audio through one shared contract
- keep long-lived audio state inside the game layer
- let each backend feed a real device without duplicating synthesis logic

This module is the audio-side mirror of the architecture pattern you already saw in rendering.

---

## Why This Module Exists

Rendering is easy to trust because you can see every frame.

Audio feels harder because the time axis is invisible.

That invisibility causes common beginner failure modes:

- mixing up frames, samples, and bytes
- confusing long-lived synth state with one transient output chunk
- assuming the mixer should know about ALSA or Raylib device details
- treating backend drain timing as if it were game logic

Module 05 fixes those problems by making the audio pipeline explicit from shared contract to device drain loop.

---

## Observable Outcome

By the end of Module 05, you should be able to:

1. Explain what one stereo sample frame is and how it is stored in memory.
2. Explain why `AudioOutputBuffer` plays the same architectural role for sound that `Backbuffer` plays for rendering.
3. Trace how one sound trigger becomes a live voice in the fixed voice pool.
4. Explain how `game_get_audio_samples(...)` builds one chunk frame by frame.
5. Explain how Raylib and X11/ALSA ask for more audio without changing the shared game-side audio code.
6. Explain why the sequencer advances inside the sample loop while `game_audio_update(...)` handles coarser frame-level state.

If those six ideas are stable, audio has stopped being a mysterious side system and has become another understandable runtime pipeline.

---

## Module Map

```text
Lesson 18
  shared audio contract and buffer shape

Lesson 19
  pure waveform, MIDI, pan, and clamp math

Lesson 20
  static sound recipes and live voice instances

Lesson 21
  per-sample mixing and chunk synthesis

Lesson 22
  backend drain loops, demand queries, and pause/resume lifecycle

Lesson 23
  persistent music sequencer and scene audio identity
```

---

## Lesson Order

1. [lesson-18-shared-audio-contract-and-sample-buffers.md](lesson-18-shared-audio-contract-and-sample-buffers.md)
   - define the shared contract before any synthesis details
2. [lesson-19-waveforms-midi-and-pan-helpers.md](lesson-19-waveforms-midi-and-pan-helpers.md)
   - build the pure math vocabulary the mixer reuses
3. [lesson-20-voices-sounddefs-and-one-shot-sfx.md](lesson-20-voices-sounddefs-and-one-shot-sfx.md)
   - turn named sounds into live playback state
4. [lesson-21-per-sample-mixing-and-pcm-output.md](lesson-21-per-sample-mixing-and-pcm-output.md)
   - walk the actual chunk-fill loop in `game/audio_demo.c`
5. [lesson-22-backend-audio-drain-loops-and-focus-lifecycle.md](lesson-22-backend-audio-drain-loops-and-focus-lifecycle.md)
   - connect the shared mixer to Raylib and ALSA demand loops
6. [lesson-23-music-sequencer-and-scene-driven-audio-state.md](lesson-23-music-sequencer-and-scene-driven-audio-state.md)
   - explain persistent music state, sample-accurate sequencing, and scene profiles

---

## The Big Continuity Bridge From Module 04

Module 04 ended with this idea:

```text
one frame is a trustworthy snapshot in time
```

Module 05 adds the audio analogue:

```text
one audio request is a trustworthy chunk in time
```

That means the runtime now has two parallel pipelines:

```text
rendering:
  shared image contract -> fill pixels -> present image

audio:
  shared sample contract -> fill sample frames -> drain to device
```

That symmetry is the main architecture lesson of the module.

---

## The Core Mental Model

Keep this separation active for the whole module:

```text
persistent state
  -> GameAudioState, voices, sequencer, mix levels

transient request
  -> AudioOutputBuffer for this one chunk fill
```

If those two layers blur together, the audio code will feel much more confusing than it really is.

---

## How To Study This Module

For every lesson, keep translating data into one of these three unit systems:

```text
frames
channels
bytes
```

And keep asking these two questions:

```text
what state survives across calls?
what state belongs only to this one chunk request?
```

Those two habits resolve most beginner confusion before it starts.

---

## Beginner Danger Zones

Watch for these mistakes while reading Module 05:

1. Treating one stereo frame as if it were one single sample value.
2. Thinking the mixer should know about ALSA recovery or Raylib stream readiness.
3. Thinking `game_audio_update(...)` and `game_get_audio_samples(...)` are interchangeable.
4. Forgetting that the live source of truth is the current branch constants and call sites, not older historical lesson comments.

That fourth point matters on this branch because some legacy comments still mention older rates or lesson numbers, while the live runtime now uses `AUDIO_SAMPLE_RATE 48000` and the `game/main.c` facade.

---

## Verification Before Module 06

Before moving on, you should be able to explain all of these from memory:

1. What one stereo sample frame contains.
2. Why `sample_count` and `max_sample_count` are different fields.
3. Why the game mixes in normalized float even when the backend may store another format.
4. Why `game_play_sound_at(...)` converts a `SoundDef` recipe into a `ToneGenerator` instance.
5. Why Raylib uses a `while (IsAudioStreamProcessed(...))` drain loop.
6. Why X11 asks ALSA how many frames are available instead of guessing.
7. Why the sequencer advances at sample time inside `game_get_audio_samples(...)`.

If those answers are stable, you are ready for coordinate systems and camera math.

---

## Bridge To Module 06

Module 05 finishes the second big media pipeline of the course.

After this point, the runtime can explain both:

```text
where images come from
where sound comes from
```

Module 06 shifts back to space instead of time.

The next question is:

```text
once the runtime has trustworthy image and audio pipelines,
how does it explain where objects live in world space,
screen space, and camera space?
```
