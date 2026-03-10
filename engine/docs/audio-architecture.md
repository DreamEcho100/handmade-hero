# Engine Audio Architecture

## Overview

The engine's audio system is split into two layers:

| Layer                   | Owner                                       | Purpose                           |
| ----------------------- | ------------------------------------------- | --------------------------------- |
| `GameAudioOutputBuffer` | Shared (`engine/game/audio.h`)              | Contract between game and backend |
| `LinuxAudioConfig`      | X11 backend (`platforms/x11/audio.h`)       | ALSA ring-buffer state            |
| `g_raylib_audio_output` | Raylib backend (`platforms/raylib/audio.c`) | Raylib push-buffer state          |

The game code only ever touches `GameAudioOutputBuffer`. Backend internals are completely hidden.

---

## How the Architecture Works in Practice

### The Boot Sequence

`game-bootstrap.c` runs first. `game_startup` writes the audio parameters the game wants into `GameConfig`: `initial_audio_sample_rate`, `audio_buffer_size_frames`, `audio_game_update_hz`. These stay in `EngineGameState.config`.

`engine_init` then allocates `GameAudioOutputBuffer.samples` from the engine's own memory block, sets `max_sample_count` from the buffer size, and leaves `is_initialized = false`. The buffer exists in memory but nothing is connected to hardware yet.

Then the backend init runs:

- Raylib: `raylib_init_audio(&engine->game.audio, ...)`
- X11/ALSA: `linux_init_audio(&x11->audio_config, &engine->game.audio, ...)`

Both set `audio_output->samples_per_second`, `audio_output->format = AUDIO_FORMAT_I16`, and `audio_output->is_initialized = true`. From this moment the game can safely write into `samples_buffer` using `audio_write_sample()`.

Finally `game_init` runs. It reads persistent `GameMemory` and initialises the game-specific audio state (`HHGameAudioState`, oscillator phases, volumes). This state lives inside the permanent memory block — not a global, not a DLL static.

### Every Frame

The main loop calls `audio_generate_and_send`:

- **Raylib**: polls `IsAudioStreamProcessed` up to 4 times; each time it fires, sets `game->audio.sample_count` and calls `get_audio_samples`, then `raylib_send_samples`.
- **X11/ALSA**: computes how far the write cursor is behind the latency target, sets `sample_count` accordingly, calls `get_audio_samples`, writes to the ALSA ring buffer with `snd_pcm_writei`.

`get_audio_samples` lands in the game DLL (`game_get_audio_samples` in `main.c`). It reads `HHGameAudioState` from `GameMemory`, mixes all voices in `f32`, and writes each stereo frame via `audio_write_sample(buf, frame_index, left_f32, right_f32)`. That helper reads `buf->format` (set by the backend at init) and converts to the correct PCM format at the output boundary. The backend then moves those bytes to hardware.

### What `audio-helpers.h` and `audio.h` Provide

- `SoundSource` — a phase-accumulator oscillator: `phase`, `frequency`, `target_frequency`, `volume`, `current_volume`, `pan_position`, `is_playing`. Handles frequency glides and smooth volume transitions.
- `GameAudioState` — a minimal engine-level audio state (`SoundSource tone`, `f32 master_volume`). Games with complex needs define their own (e.g., `HHGameAudioState`).
- `De100SoundPlayer` — a playback-cursor player for pre-loaded PCM: stores a pointer to the sample data, cursor, loop flag, gain. The game advances it per-sample in the fill callback.
- `de100_audio_midi_to_freq(midi_note)` — standard MIDI note → Hz (`A4 = 440 Hz`).
- `audio_write_sample(buf, frame_index, L, R)` — write one normalised stereo frame (`-1.0..1.0`) into `buf->samples_buffer`, converting to whatever `buf->format` the backend set (`AUDIO_FORMAT_I16`, `I32`, `F32`, `F64`). Dispatches via `_Generic` on the type of `L`: pass `f32` values for the `f32` path, `f64` for higher precision.
- `audio_format_bytes_per_sample(fmt)` — returns bytes per stereo frame for an `AudioSampleFormat`.
- `de100_audio_calculate_pan(pan, &left_vol, &right_vol)` — linear panning from `[-1, 1]`.
- MIDI note constants: `DE100_MIDI_C4`, `DE100_MIDI_A4`, `DE100_MIDI_REST`, etc.

`HHGameAudioState` (in `main.h`) adds a `SoundSource tone`, a `De100SoundPlayer sfx`, and separate `master_volume`, `sfx_volume`, `music_volume` floats. Per-category volume is applied as a multiplier in the fill callback — there is no bus mixer yet.

### What Is Currently Missing

| Gap                                               | Impact                                                                                    |
| ------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| No audio file loader (WAV, OGG, MP3)              | Every sound must be synthesised — no pre-recorded SFX or music                            |
| No streaming ring buffer                          | Music files too large to load fully must wait; nothing decodes ahead                      |
| No `sample_clock` in `GameAudioOutputBuffer`      | A/V sync requires backend-specific code (`running_sample_index`, `total_samples_written`) |
| No bus mixer                                      | All voices mix to one flat stream; per-bus volume control requires a mixer layer          |
| No `backend_pause_audio` / `backend_resume_audio` | Focus-loss handling is ad-hoc per backend                                                 |

---

## Integrating a Game: The Full Story

### Step 1 — Define your audio state in the game header

Put everything in a struct that will live inside `GameMemory`. Never use DLL-local statics for audio state:

```c
typedef struct {
  SoundSource bgm;          // oscillator for music tone
  De100SoundPlayer sfx[8];  // pool of one-shot effects
  f32 master_vol;
  f32 sfx_vol;
  f32 music_vol;
  i64 sample_clock;         // maintain yourself until engine exposes it
} MyGameAudioState;
```

### Step 2 — Init in `game_init`

```c
MyGameAudioState *a = &game_state->audio;
a->master_vol = 1.0f;
a->sfx_vol    = 0.8f;
a->music_vol  = 0.6f;
a->bgm.frequency = de100_audio_midi_to_freq(DE100_MIDI_A4);
a->bgm.volume    = 0.5f;
a->bgm.is_playing = true;
```

### Step 3 — Implement `game_get_audio_samples`

Always write to the full `sample_count`. Never return early with a partial buffer:

```c
GAME_GET_AUDIO_SAMPLES(game_get_audio_samples) {
  MyGameAudioState *a = &((MyGameState *)memory->permanent.base)->audio;

  if (!audio->is_initialized) {
    memset(audio->samples_buffer, 0,
           (size_t)audio->sample_count * (size_t)audio_format_bytes_per_sample(audio->format));
    return;
  }

  for (i32 i = 0; i < audio->sample_count; i++) {
    float bgm_s = sinf(a->bgm.phase) * a->bgm.volume * a->music_vol;
    a->bgm.phase += 2.0f * PI * a->bgm.frequency / audio->samples_per_second;
    if (a->bgm.phase > 2.0f * PI) a->bgm.phase -= 2.0f * PI;

    float sfx_s = 0.0f;
    // ... accumulate sfx voices ...

    float L = (bgm_s + sfx_s) * a->master_vol;
    float R = L;
    audio_write_sample(audio, i, L, R);  /* format-agnostic; backend decides PCM width */
    a->sample_clock++;
  }
}
```

### Step 4 — Trigger sounds from gameplay

In `game_update_and_render`, set flags or activate players. The fill callback picks them up without any cross-thread concern because both run on the main loop:

```c
if (player_jumped) {
  a->sfx[0].data    = assets->jump_pcm;
  a->sfx[0].frames  = assets->jump_frame_count;
  a->sfx[0].cursor  = 0;
  a->sfx[0].playing = true;
}
```

---

## Adding Sound to a Game: Implementation Paths

### Path 1 — Generated Tones (Works Now)

`SoundSource` + `sinf` + MIDI helpers already cover this. Frequency glides work by interpolating `frequency` toward `target_frequency` each sample. The `audio-helpers.h` utilities handle all the math. You just need to:

- Keep phase in `GameMemory` (not a DLL-local `static float`)
- Use `de100_audio_midi_to_freq` for musical notes
- Attenuate before mixing (`volume <= 0.5f` per voice prevents clipping when voices combine)

### Path 2 — Pre-loaded PCM SFX (Needs a WAV Loader)

`De100SoundPlayer` already handles cursor + loop + gain. What's missing is getting sample data into it.

**Writing a minimal WAV loader (≈80 lines of C, no dependencies):**

WAV layout: `RIFF` → `fmt ` chunk (sample rate, channels, bit depth) → `data` chunk (raw PCM). On little-endian hardware (x86, ARM) no byte-swapping is needed.

```c
typedef struct { u16 channels; u32 sample_rate; u16 bits_per_sample; } WavFmt;
typedef struct { i16 *samples; u32 frame_count; u32 sample_rate; } PcmAsset;

bool load_wav(const char *path, PcmAsset *out, void *memory_arena, u32 *arena_used) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  // read 12-byte RIFF header, validate "RIFF" + "WAVE"
  // scan chunks: read 8-byte chunk header (FourCC + size), skip unknown
  // on "fmt ": read WavFmt, assert channels==2, bits_per_sample==16
  // on "data": fread size bytes into memory_arena, set out->samples + frame_count
  fclose(f);
  return true;
}
```

What to research:

- RIFF chunk scanning: each chunk is `[4-byte FourCC][4-byte size][size bytes of data]`. Chunks are padded to even bytes.
- `fmt ` chunk: `wFormatTag` must be 1 (PCM). `nChannels`, `nSamplesPerSec`, `wBitsPerSample` tell you the format.
- `data` chunk: `ckSize` bytes of raw interleaved PCM follows immediately.
- If the file's `nSamplesPerSec` ≠ `audio->samples_per_second`, resample at load time (slow, once) or per-sample in the fill callback.

Store loaded assets in `GameMemory` transient storage — it's 1 GB in the current config, more than enough for uncompressed SFX clips.

### Path 3 — Music Tracks

**Option A — Full pre-load (simplest, works for tracks < ~100 MB):**

Load the entire WAV into transient `GameMemory`. A 3-minute stereo 16-bit 48 kHz track = ~34 MB. Loop with cursor wrap at `frame_count`. No threading, no ring buffer. Use `De100SoundPlayer` as-is. This is the right first step.

**Option B — Main-thread decode-ahead streaming (no threading, handles large files):**

Allocate a ring buffer in `GameMemory` (e.g., 4× `max_sample_count` ≈ 16 KB for 1024-frame buffer). Each frame in the main loop, before calling `audio_generate_and_send`, decode N frames ahead into the ring head. The fill callback reads from the ring tail.

```
Main loop (per frame):
  decode_ahead(&stream, ring, FRAMES_PER_DECODE);  // fills ring head
  // ... game update + render ...
  audio_generate_and_send();                        // drains ring tail
```

For WAV this "decode" is just `fread`. For OGG it's `stb_vorbis_decode_frame`. No threading needed. The risk: a slow disk read inside `decode_ahead` adds to your frame time. Acceptable for most cases; mitigate with a large enough ring buffer.

**Option C — Background thread streaming (for disk-latency-tolerant systems):**

A dedicated thread runs `decode_ahead` continuously, writing to the ring head with atomic cursor updates. The fill callback reads from the ring tail. The fill callback never touches file I/O or decoder state.

This is the correct approach when you need truly hitless streaming. What to research:

- Lock-free ring buffer with atomic `read_cursor` and `write_cursor` (power-of-two size, modulo indexing, `_Atomic u32` on C11 or `__atomic_*` builtins)
- Ensuring the write cursor never laps the read cursor: `available_to_write = ring_size - (write - read)`
- `pthread_create` / `pthread_join` lifecycle tied to backend init/shutdown
- The fill callback never calls `pthread_mutex_lock` — it only reads/advances the read cursor atomically

**For OGG files specifically:**

`stb_vorbis.h` is a single-header public-domain library. Add it to `engine/_vendor/` and `#define STB_VORBIS_IMPLEMENTATION` in one `.c` file.

Key API:

```c
stb_vorbis *v = stb_vorbis_open_filename("music.ogg", NULL, NULL);
int channels, sample_rate;
short *pcm;
int frames = stb_vorbis_decode_frame_pushdata(v, ...);
// or for full decode:
int total = stb_vorbis_decode_filename("music.ogg", &channels, &sample_rate, &pcm);
```

What to research for stb_vorbis:

- `stb_vorbis_open_memory` for loading from pre-read file data (avoids stb_vorbis doing its own file I/O, better for your memory model)
- `stb_vorbis_seek_frame` for loop points
- stb_vorbis decodes to `float` by default — you must convert to `i16` before writing to `audio->samples`
- Its internal allocation: use `stb_vorbis_alloc` to give it a fixed arena from `GameMemory` so it doesn't call `malloc` — critical for hot-reload correctness

---

## Methodology: Facing a New or Difficult Audio Feature

### Step 1 — Identify the layer before writing code

Ask:

- Does it shape the signal? → Game code or shared mixer utility only. Never the backend.
- Does it need hardware timing? → Backend private state. Never the shared contract.
- Is it truly needed by all games on all backends? → Only then does it belong in `engine/game/`.
- Is it specific to one audio API? → Backend private only.

When unsure, put it in game code first and promote it later. It is much easier to move code up than to pull backend-specific logic back down.

### Step 2 — Find where the state lives

Write it out before coding:

- "This feature needs a delay line of 1024 float samples."
- "That must persist frame-to-frame." → `GameMemory`.
- "It must survive hot-reload." → `GameMemory` (DLL-local statics are wiped).
- "It must survive device change." → `GameMemory` (backend connection drops; buffer stays).

If you can't answer all three, the feature is not ready to implement.

### Step 3 — Build it as a pure function first

Before wiring it to the engine, write it as a standalone function:

```c
void apply_lowpass(float *buf, i32 frames, float cutoff, float *prev_L, float *prev_R);
```

Test it by generating a small WAV file with a `main()` and listening to the output. This separates "does the DSP logic work" from "is it wired up correctly." Fix the DSP in isolation; then wire.

### Step 4 — Measure the budget

The fill callback runs every ~10ms at 100 FPS. At 48 kHz with a 1024-frame buffer that's ~21ms per call. Run your feature stand-alone and time it with `clock_gettime`. If it takes 1ms, that is ~5% of the audio budget — already significant if you have other voices.

Optimise only if you measure a problem. Don't optimise a feature you haven't verified works correctly.

### Step 5 — Identify minimum buffer size

Does the feature require a look-back (reverb tail, HRTF convolution, chorus delay)? Write the minimum in a comment:

```c
// MINIMUM BUFFER: 512 samples (HRTF convolution kernel length)
// Backends that provide < 512 samples/call must accumulate before calling this.
```

Verify that both ALSA and Raylib can provide that minimum with the current `audio_buffer_size_frames = 1024`. The constant is in `game-bootstrap.c`.

### Step 6 — Test both backends before calling it done

ALSA and Raylib call the fill callback with different `sample_count` values per call:

- ALSA: `sample_count` varies frame-to-frame based on how far behind the write cursor is
- Raylib: `sample_count` is fixed at `buffer_size_frames` every call

Any feature that assumes a fixed block size (e.g., an FFT that only works on power-of-two blocks) will behave incorrectly on ALSA. Test both.

---

## DSP and Audio Fundamentals Worth Knowing

These come up repeatedly when implementing real audio features. You do not need a DSP course — just these practical foundations.

### Sampling and Frequencies

- You can only represent frequencies up to **half the sample rate** (Nyquist: 24 kHz max at 48 kHz). Above that, the signal aliases (wraps back, creating wrong frequencies).
- `sinf` for a pure tone never aliases because it's a single frequency. A square wave contains harmonics up to infinity — when you compute it by hard-clipping, those high harmonics alias and create digital distortion.
- If you synthesise a square wave for 8-bit aesthetic sound, this aliasing is intentional. If you want a clean square wave, use a **bandlimited** square (sum of sines up to Nyquist) — `audio-helpers.h` may already do this.

### Volume and Clipping

- `i16` range: `0 = silence`, `±32767 = 0 dBFS` (maximum digital level).
- Two voices at full volume sum to `±65534` — that's twice the `i16` max: **instant clipping**.
- **Rule**: attenuate each voice before mixing. If you have N voices, each should be at most `1/N` volume to guarantee no clipping. In practice, most voices are never all at max simultaneously, so `1/sqrt(N)` is a reasonable heuristic.
- Clipping sounds worse than simply being quiet. Mix to `float32`, accumulate freely, then clamp once at the final `i16` write.

### Why `float32` for Mixing

`i16` has 65536 discrete levels. When you multiply two `i16` values (e.g., a sample × a volume), the result can need 32 bits. If you stay in `i16` arithmetic throughout a multi-stage mix, each operation loses low bits — this is called **quantisation noise** and it accumulates. `float32` has 7-digit decimal precision, far more than enough for game audio mixing. Convert to `i16` only at the output.

### Linear vs. Constant-Power Panning

- **Linear pan** (`left = 1 - pan*0.5, right = 0.5 + pan*0.5`): summing L+R gives constant power at centre, but panning through centre sounds quieter. Simple, fine for game SFX.
- **Constant-power pan** (`left = cos(pan * PI/2), right = sin(pan * PI/2)`): power stays constant as you pan. Sounds natural. `de100_audio_calculate_pan` uses linear — good enough for now, switch if panning sounds "dipped" in the middle.

### Distance Attenuation

- Energy falls off with distance as `1/r²` (inverse square law). But amplitude is proportional to `sqrt(energy)`, so amplitude falls off as **`1/r`**.
- Practical formula: `gain = 1.0f / max(distance, 1.0f)`. The `max(distance, 1)` clamp prevents gain exploding to infinity at zero distance.
- For 2D games: `distance = sqrtf(dx*dx + dy*dy)`. In real-time audio, `sqrtf` matters less than you might think (it's one call per voice per frame, not per sample).

### One-Pole IIR Low-pass Filter

The simplest useful filter. One multiplication and one addition per sample. Coefficient `a` (0 to 1) controls cutoff: higher `a` = lower cutoff = more muffled:

```
y[n] = (1 - a) * x[n] + a * y[n-1]
```

`a = 0.9` makes everything sound like it's behind a door. `a = 0` is no filtering. History value `y[n-1]` must be stored in `GameMemory`.

### What a Stereo i16 Frame Looks Like in Memory

One frame = 4 bytes: `[L_low, L_high, R_low, R_high]` (little-endian). Most API docs are ambiguous — "sample" sometimes means one `i16` value (one channel), sometimes means one stereo pair (one frame). In this engine, `sample_count` means **frames** (stereo pairs). Byte count = `sample_count * 2 channels * 2 bytes = sample_count * 4`.

---

## What's Missing From the Engine (Priority Order)

These five additions unlock everything else without requiring library dependencies or threading:

### 1 — Minimal WAV Loader (`engine/game/asset-loader.h`)

~80 lines, no dependencies. Unlocks all pre-recorded SFX immediately. Store loaded assets in `GameMemory` transient storage. Implement first; everything else builds on this.

### 2 — `u64 sample_clock` in `GameAudioOutputBuffer`

The backend increments it by `sample_count` after each fill call. Games use it for A/V sync, subtitle timing, animation events, and accurate music position. Without it, every game reimplements this in backend-specific ways.

### 3 — ~~Connect pause state to Audio~~ ✅ DONE (game-callback level)

`game_get_audio_samples` checks `game->is_paused` (a `bool32` field on `HandMadeHeroGameState` inside `GameMemory`) immediately after the `is_initialized` guard. When true it `memset`s `samples_buffer` to zero and returns — the backend receives clean silence and keeps its stream fed without underruns. The backends never touch game pause state; they are fully agnostic. `g_game_is_paused` (the old engine global) is gone entirely. Note: `backend_pause_audio` / `backend_resume_audio` (clean ALSA drain via `snd_pcm_drop/prepare`, Raylib `PauseAudioStream/ResumeAudioStream`) are still missing, so the ALSA ring buffer keeps running during pause (filled with silence). A proper backend-level pause interface is a future improvement.

### 4 — Streaming Ring Buffer Utility (`engine/game/audio-stream.h`)

A reusable ring buffer + cursor utility that any game can use for streamed audio. Not a file format decoder — just:

```c
typedef struct { i16 *buf; u32 capacity; _Atomic u32 read; _Atomic u32 write; } AudioRingBuffer;
u32  audio_ring_available_to_read(AudioRingBuffer *r);
u32  audio_ring_available_to_write(AudioRingBuffer *r);
void audio_ring_write(AudioRingBuffer *r, const i16 *src, u32 frames);
void audio_ring_read(AudioRingBuffer *r, i16 *dst, u32 frames);
```

Games allocate the backing buffer from `GameMemory`. The fill callback drains; main-loop decode fills. Threading optional and additive on top.

### 5 — ~~`sample_format` Enum in `GameAudioOutputBuffer`~~ ✅ DONE

`AudioSampleFormat` (`AUDIO_FORMAT_I16`, `I32`, `F32`, `F64`) is now in `engine/game/audio.h`. Both backends set `format = AUDIO_FORMAT_I16` at init. Game code uses `audio_write_sample(buf, frame_index, L, R)` from `audio-helpers.h` — a `_Generic` macro that dispatches on the type of `L` and converts normalised `f32`/`f64` to whatever PCM format the backend requested. No game file ever casts `samples_buffer` to `i16*` directly.

Everything beyond this — OGG decoding, reverb, spatial audio, buses — is game-layer code that assembles from these five primitives plus `stb_vorbis.h` or a hand-rolled decoder.

```c
typedef struct {
  i32             samples_per_second;  // Set once at init; game reads this
  i32             sample_count;        // Samples to generate THIS call (set by backend, max ≤ max_sample_count)
  i32             max_sample_count;    // Buffer capacity — never write more than this (set at engine init)
  void           *samples_buffer;      // Opaque PCM buffer; write via audio_write_sample()
  AudioSampleFormat format;            // Set by backend at init (currently always AUDIO_FORMAT_I16)
  bool            is_initialized;      // Backend sets true after successful init; game can check
} GameAudioOutputBuffer;
```

The game's `get_audio_samples` callback receives this. It should:

1. Read `sample_count` to know how many frames to write.
2. Write samples via `audio_write_sample(buf, frame_index, left_f32, right_f32)` — never cast `samples_buffer` directly.
3. Never write more than `max_sample_count` frames.
4. Check `is_initialized` if it needs to handle the uninitialized case gracefully.

---

## Backend Models

### ALSA (X11 backend) — Ring-buffer / Write-ahead

ALSA uses a hardware ring buffer with:

- **Latency samples** (`latency_samples`) — how far ahead to write so the hardware never starves.
- **Safety margin** (`safety_samples`) — extra cushion to avoid underruns.
- **Running sample index** (`running_sample_index`) — tracks absolute position for synchronisation with the frame timer.

Every frame, the backend:

1. Queries how many samples the hardware has consumed.
2. Computes `samples_to_generate = latency_target - samples_in_flight`.
3. Calls `get_audio_samples`, advancing `running_sample_index`.
4. Writes the result into the ALSA ring buffer with `snd_pcm_writei`.

The game fill callback is called from the main game loop on the same thread. There is **no background audio thread**.

### Raylib — Push / Double-buffer

Raylib's `AudioStream` API internally double-buffers. The backend:

1. Checks `IsAudioStreamProcessed` — returns true when a buffer slot is free.
2. Calls `get_audio_samples` with `sample_count = buffer_size_frames`.
3. Pushes with `UpdateAudioStream`.

The loop calls this up to 4 times per frame to drain any backlog. Buffer size is fixed at init time; changing FPS does not automatically resize it.

---

## Adding a New Backend

1. **Do not add audio fields to `PlatformConfig`.** `PlatformAudioConfig` was removed; backends own their private state.
2. Create a private config struct (e.g., `MyBackendAudioConfig`) in your backend's `audio.h`.
3. Embed it in your platform state struct (not in `EnginePlatformState`).
4. Implement these four functions:

```c
bool  backend_init_audio   (GameAudioOutputBuffer *audio_output,
                            i32 samples_per_second, i32 game_update_hz);
void  backend_shutdown_audio(GameAudioOutputBuffer *audio_output);
u32   backend_get_samples_to_write(GameAudioOutputBuffer *audio_output);
void  backend_send_samples  (GameAudioOutputBuffer *audio_output);
```

5. In `init`: set `audio_output->samples_per_second`, `audio_output->format` (e.g. `AUDIO_FORMAT_I16`), and `audio_output->is_initialized = true` on success. Never omit `format` — game code calls `audio_write_sample()` which dispatches on it.
6. In the main loop, cap `samples_to_generate` to `audio_output->max_sample_count` before calling `get_audio_samples`.

---

## Adding Other Platforms/Backends (SDL3, SFML, Windows, macOS)

### The Root Lesson: Abstraction Inversion

The `PlatformAudioConfig` mistake was an **abstraction inversion**: ALSA-specific fields (`latency_samples`, `running_sample_index`, `safety_samples`) leaked upward into a shared layer, and every other backend had to carry dead weight. The fix — push private state _down_ into each backend, expose only `GameAudioOutputBuffer` upward — is correct, but it has a hidden assumption that breaks on several platforms.

---

### The Hidden Assumption: Single-Threaded Fill

Right now, `get_audio_samples` is called from the **main game loop thread**. That works for:

- ALSA — you call `snd_pcm_writei` yourself
- Raylib push mode — `UpdateAudioStream` is called from the main thread

It will **not** work directly for:

- **macOS CoreAudio / AudioUnit** — the OS calls your render callback on a **real-time audio thread**. `malloc`, `printf`, mutex locks, and most system calls inside it cause glitches or crashes.
- **SFML `SoundStream`** — `onGetData` runs on SFML's internal audio thread.
- **SDL3 audio callback model** — same issue, though SDL3 also offers a push API (`SDL_PutAudioStreamData`) that sidesteps it.
- **XAudio2 (Windows)** — voice callbacks run on a separate WASAPI thread.

The architecture must answer: **is `get_audio_samples` allowed to be called from a non-main thread?**

- If **yes**: game code must use only lock-free operations inside it — no globals without atomics, no allocator, no `printf`.
- If **no**: callback-based backends need an intermediate lock-free ring buffer. The game fills it on the main thread; the audio thread just memcopies from it.

The safe default for this engine is **no** — keep `get_audio_samples` main-thread-only, and expose a ring-buffer shim for any callback-based backend.

---

### What Actually Differs Per Backend

#### Sample format

`AudioSampleFormat` is now in `engine/game/audio.h` with four cases: `AUDIO_FORMAT_I16`, `I32`, `F32`, `F64`. Both current backends set `AUDIO_FORMAT_I16` at init. When adding SDL3, WASAPI, or CoreAudio (which prefer or require `float32`), the backend simply sets `AUDIO_FORMAT_F32` instead — game code is unchanged because it always calls `audio_write_sample()`.

The game's contract is: **mix in normalised `f32`, pass to `audio_write_sample`, never touch `samples_buffer` directly.** Format conversion is the backend's private responsibility.

#### Buffer size ownership

| Backend            | Who controls buffer size?                                                              |
| ------------------ | -------------------------------------------------------------------------------------- |
| ALSA               | You                                                                                    |
| Raylib             | You                                                                                    |
| SDL3 push          | You (driver picks optimal, you request)                                                |
| WASAPI shared mode | **Windows** — typically 480 frames @ 48 kHz (10 ms periods)                            |
| CoreAudio          | **Hardware** — you can request but not guarantee `kAudioDevicePropertyBufferFrameSize` |

`max_sample_count` as a hard cap handles this correctly already — the backend says "I need N samples" and N is bounded. However, on WASAPI shared mode N might be 480 samples, which is far smaller than a typical ALSA latency buffer. `get_audio_samples` must be efficient for very small `sample_count` values.

#### Device change / hot-plug

Windows fires `IMMNotificationClient::OnDefaultDeviceChanged` when headphones are plugged in. macOS fires `kAudioHardwarePropertyDefaultOutputDevice`. Neither ALSA nor Raylib surface this event, so it was easy to ignore — but on those platforms it's a real case.

When a device change happens:

- The backend tears down and re-inits (`is_initialized` flips `false` then `true`)
- The game must not crash when `is_initialized` is false mid-session
- The engine-allocated `samples` buffer remains valid — only the backend's connection to the hardware drops

This makes `is_initialized` **load-bearing correctness** on Windows/macOS, not just a startup convenience. It should be checked every frame, not once.

#### Exclusive vs. shared mode (Windows WASAPI)

- **Exclusive mode**: your process owns the hardware, lowest latency (~1–3 ms), you specify the exact format. Incompatible with other apps playing audio simultaneously.
- **Shared mode**: the Windows Audio Session API mixes you with other processes, higher latency (~10 ms+), format is negotiated.

Games typically want exclusive mode, but it requires the user to not have other audio playing. The backend needs to pick a stance or expose a config flag for it.

#### macOS real-time thread constraints

On macOS with CoreAudio, the audio render callback runs with **real-time priority** and hard deadline constraints. Inside it you **must not**:

- Call `malloc` / `free`
- Lock a `pthread_mutex` (may block)
- Call `printf` / `fprintf` (allocates internally)
- Send Objective-C messages that hit the runtime
- Do any file I/O

Violating any of these causes priority inversion and audio dropouts. The safe design: the game fills a lock-free ring buffer on the main thread; the CoreAudio callback only memcopies from it and never calls back into game code.

---

### What Should Stay in `GameAudioOutputBuffer` vs. Move

**Keep as-is:**

- `samples_per_second` — the game needs this to compute tone frequencies
- `sample_count` — frames to fill this call
- `max_sample_count` — safety cap
- `samples_buffer` — the opaque buffer pointer
- `format` — ✅ added (`AudioSampleFormat`); use `audio_write_sample()` to write, never cast directly
- `is_initialized` — lets the game guard writes and handle device changes gracefully

**Consider adding before the next backend:**

- `channel_count` — stereo is fine now, but adding it later touches every fill callback
- `u64 sample_clock` — an absolute sample counter the backend increments; equivalent to the old `running_sample_index` but now exposed in the shared contract so any game can sync animations/events to audio precisely without backend-specific code

**Keep out of the shared contract forever:**

- Latency targets, ring buffer positions, hardware periods, device handles — 100% backend-private

---

### Architectural Rules for Future Backends

1. **No backend-specific types in `_common/` or `game/`.** If a field only makes sense for one backend, it belongs in that backend's private struct — as `LinuxAudioConfig` is now in `x11/audio.h`.

2. **`get_audio_samples` is a real-time boundary.** Document whether it can be called from a non-main thread. If the answer is no, enforce it with a ring-buffer shim for callback-based backends. If yes, prohibit allocations and locks inside it.

3. **The backend owns format negotiation.** The game fills whatever format `sample_format` says. Conversion is the backend's responsibility.

4. **`is_initialized` is an edge, not a level.** Don't only check it once at startup. Check it every frame so device change and hot-reload are handled correctly automatically.

5. **Debug calls belong to the backend, not the game adapter.** The `linux_debug_audio_latency` call in `games/handmade-hero-game/src/adapters/x11/inputs/keyboard.c` was a layering violation — a game-layer file importing a platform-specific function. The correct pattern is either reading from `GameAudioOutputBuffer` fields directly, or routing through a function pointer in a platform debug interface. Hard-coding backend-specific includes in game adapters creates a coupling that multiplies with every new platform.

6. **Each backend's build target isolates its link flags.** CoreAudio needs `-framework AudioToolbox -framework CoreAudio`. SDL3 needs `-lSDL3`. WASAPI needs `-lole32 -loleaut32`. Nothing in the game layer should know about these.

---

### Questions to Answer Before Starting Each New Backend

- Does this backend's audio fill run on the main thread or a private audio thread?
- What sample format does it natively use? Am I converting in the backend, or negotiating format with the game?
- Can I control the buffer size, or does the OS/hardware dictate it?
- How does this backend handle device disconnection or default device change?
- What is the minimum viable latency and is it acceptable for game audio?
- Does the backend have its own internal mixing/resampling I can leverage?
- What happens if `get_audio_samples` runs too long and misses the deadline?

---

## Designing Audio Features

The same root lesson from the `PlatformAudioConfig` mistake applies to every new feature: **if it shapes the signal, it belongs in game code or a shared mixer layer, not in the backend**. The backend's only job is to move bytes from `samples` to hardware. It must not know about voices, gains, effects, or buses.

### The Core Question for Every Feature

Before adding anything, answer:

- Does this feature affect the backend, the shared contract, or only game code?
- If it needs per-frame state (effect history, playback cursor, phase accumulator), where does that state live? It **must** be `GameMemory` — not platform state, not DLL-local statics. DLL-local statics are wiped on hot-reload.
- Does this feature impose a minimum buffer size? Reverb tails and HRTF convolution require hundreds of samples of context — if yes, document it so backends know.
- Does this feature produce a different signal depending on which backend runs it? If yes, it is in the wrong place.
- Does this feature involve timing? If yes, is it derived from `sample_clock` (hardware-accurate) or the frame timer (drifts under load)?

---

### Volume / Gain Control

**Where it belongs: game code / mixer, never the backend.**

If the game scales `i16` values by a `0.0–1.0` float before writing to `samples`, it works on every backend identically. If you instead use a backend-specific call (Raylib's `SetAudioStreamVolume`, ALSA software volume, WASAPI session volume), behaviour diverges — the same game code sounds different on each backend.

This is the signal-shaping rule in its simplest form.

---

### Multiple Sound Sources / Mixer

**Where it belongs: a utility in `engine/game/`, called from `get_audio_samples`.**

The current contract hands one flat `GameAudioOutputBuffer` to `get_audio_samples`. The game is responsible for mixing everything down to stereo before returning. This is correct — the backend must never know about individual voices.

Inside game code you will quickly want:

- N voices, each with its own gain, pan, and pitch
- A master bus + sub-buses (music, sfx, UI)

The right model: a mixer utility in `engine/game/` (not in any platform layer) that takes `GameAudioOutputBuffer *out` and a list of voices, writes to `out->samples`, and is shareable across games. Games don't rewrite a mixer from scratch; they configure voices and buses.

**What to avoid:** putting a voice list or sound manager in `EnginePlatformState` or any `PlatformConfig`. That is the `PlatformAudioConfig` mistake at a higher level. Voices are game data.

---

### Pitch Shifting / Playback Rate

**Where it belongs: the mixer's resampling step, in game code.**

Playing a sound at 1.5× speed means consuming source samples faster than real time — it is the same resampling problem as a sample-rate mismatch, just triggered by gameplay. If you delegate it to the backend (Raylib's `SetAudioStreamPitch`, ALSA pitch via resampling), there is no portable equivalent and behaviour diverges.

All pitch and rate effects must happen during mixing in game code, using the same per-sample ratio technique as Case 7.

---

### Looping

**Where it belongs: mixer cursor logic, in game code.**

A looping sound wraps its playback cursor when it reaches the end. The non-obvious edge case: **what happens at the wrap boundary within a single `get_audio_samples` call?**

If `sample_count = 500` and the sound has 300 samples left before the loop point, you must copy 300 frames from position X, wrap, then copy 200 frames from position 0. A naive `memcpy` of the whole source either overflows the source or reads garbage past the end.

The mixer's inner loop must handle partial copies with explicit wrap logic. It cannot assume the source is always longer than the request.

---

### Sound Categories / Buses

**Where they belong: game code, mixing into intermediate buffers before the final output.**

Players expect to control music, SFX, and UI sounds independently. If every voice writes directly to `out->samples`, applying per-category volume requires touching every voice.

The clean model: **mix into intermediate `float32` bus buffers** allocated from `GameMemory`, each sized to `max_sample_count`. Apply bus gain, then sum buses into `out->samples` with a final `float` → `i16` clamp pass. Intermediate `float32` prevents clipping during accumulation before the output stage.

Benefits:

- Per-category volume and mute: change one bus gain
- Effects apply to a whole bus, not per-voice
- The `i16` conversion happens exactly once, at the very end

---

### Audio Effects (Reverb, Low-pass, Echo, Distortion)

**Where they belong: game code, operating on bus buffers. State lives in `GameMemory`.**

Effects are signal processors: buffer in → buffer out. Their state (delay line for reverb, filter coefficients for a low-pass, echo history) must persist frame-to-frame. It must live in `GameMemory`, not in DLL-local statics — hot-reload wipes those, producing a click at the boundary.

The critical consideration: **effects that look ahead or have tails** require a history buffer. If `sample_count` varies between calls — which it can on WASAPI shared mode (~480 frames @ 48 kHz) or any OS-dictated backend — the effect state must handle arbitrary-length chunks correctly, not just fixed-size blocks.

This creates a hidden dependency between backend buffer size and effect complexity:

- HRTF convolution typically needs ≥ 512 samples of context
- If WASAPI gives 480-sample periods, you must accumulate samples before processing
- **Document the minimum buffer size requirement of any effect, and verify that all target backends can meet it**

---

### Pause / Focus Loss

**Game layer: write silence into the buffer. Backend layer (future): drain with `backend_pause_audio` or pause the stream.**

Currently implemented: `game_get_audio_samples` checks `game->is_paused` (a `bool32` field on `HandMadeHeroGameState` inside `GameMemory`) right after the `is_initialized` guard. If true, it `memset`s `samples_buffer` to zero and returns. The backend sees a normal call that happened to produce silence — no special pause path needed, no underruns, no backend coupling. `g_game_is_paused` (the old engine global) no longer exists.

The missing piece is a clean backend drain on pause for ALSA. ALSA's ring buffer keeps running while filled with silence (fine), but if a future `backend_pause_audio` were added it could do `snd_pcm_drop` + `snd_pcm_prepare` to stop the hardware clock during pause:

| Backend   | Future pause mechanism                          |
| --------- | ----------------------------------------------- |
| ALSA      | `snd_pcm_drop` then `snd_pcm_prepare` to resume |
| Raylib    | `PauseAudioStream` / `ResumeAudioStream`        |
| WASAPI    | `IAudioClient::Stop` / `Start`                  |
| CoreAudio | `AudioOutputUnitStop` / `Start`                 |

The current approach (silence from game callback) is correct for this stage. Focus-loss pause is handled the same way. The backend interface has no pause concept yet.

---

### Audio-Video Sync / Sample Clock

**The timing source for A/V sync must be sample-accurate, not frame-timer-accurate.**

Handmade Hero synchronised flip timing with `running_sample_index` in `LinuxAudioConfig`. That was correct for ALSA but the field was private to the backend. Making it backend-agnostic is the reason `sample_clock` belongs in `GameAudioOutputBuffer`.

With `sample_clock` in the shared contract, any backend increments it by `sample_count` each call, and the game derives hardware-accurate time:

```c
double audio_time = (double)audio->sample_clock / audio->samples_per_second;
```

This drives subtitle timing, cutscene sync, animation events, and screen-flip synchronisation on any backend, without backend-specific code.

Without it, games either re-derive the clock per-backend (fragile, diverges between platforms) or use the frame timer, which drifts relative to audio under CPU load.

---

### Thread Safety of `is_initialized`

For the current single-threaded model this is not yet a problem. But before adding any callback-based backend (CoreAudio, SFML, XAudio2): if a device change on the audio thread flips `is_initialized = false` while the main thread is mid-way through `get_audio_samples`, that is a data race.

The fix before it matters: set `is_initialized` with **release-store semantics** in the backend, read it with **acquire-load** in the game. For now the plain `bool` works; note the upgrade path so it is not forgotten when a threaded backend is added.

---

### Debug / Visualisation

**Backend fills a debug info struct; the game overlay reads it. Never the other way around.**

The layering violation already fixed in this refactor — game adapter code importing `linux_debug_audio_latency` directly — is easy to repeat for new debug features. The correct pattern: a `GameAudioDebugInfo` struct (latency ms, buffer fill %, samples written, stream health flag) that backends populate and the game overlay reads from `GameAudioOutputBuffer`. The game layer never imports a backend-specific function to get diagnostic data.

---

### Questions to Ask Before Each New Feature

- Does this feature shape the signal? → It belongs in game code, not the backend.
- Does it need persistent per-frame state? → Goes in `GameMemory`, not DLL-local statics.
- Does it impose a minimum buffer size? → Document it; verify all backends can meet it.
- Does applying it in the backend produce results that differ from game-code application? → If yes, it is in the wrong layer.
- Is timing for this feature derived from `sample_clock` or the frame timer? → Should be `sample_clock`.
- What happens on hot-reload? → Any state not in `GameMemory` is lost.

---

## Audio Cases — How to Handle Them

### Case 1: Simple synthesized tone

```c
void get_audio_samples(GameMemory *memory, GameAudioOutputBuffer *audio) {
  for (i32 i = 0; i < audio->sample_count; i++) {
    float sample = sinf(state->tone_phase);
    state->tone_phase += 2.0f * PI * state->tone_hz / audio->samples_per_second;
    audio_write_sample(audio, i, sample, sample);  /* mono: L == R */
  }
}
```

Keep the phase accumulator in game state so it persists across calls. Mix in normalised `f32` (−1.0 to 1.0) — `audio_write_sample` converts to the backend's PCM format.

### Case 2: Streaming PCM from a file / decoder

Maintain a ring buffer in game memory. The backend calls `get_audio_samples` which drains from the ring into `audio->samples`. A separate loading step (or on-demand within the callback) refills the ring.

Key concern: **don't block** inside `get_audio_samples`. Decode ahead into a pre-allocated ring buffer (at least 2–4×`max_sample_count`).

```
+—————————+     decode     +—————————————+   copy   +—————————————+
| file/mp3 | ————————————> | ring buffer | ———————> | audio->samples |
+—————————+   (game loop)  +—————————————+  (callback) +—————————————+
```

### Case 3: Music sequencer / procedural

Generate samples from note events stored in game state. Advance a sample clock each call:

```c
i64 sample_clock = state->sample_clock;
for (i32 i = 0; i < audio->sample_count; i++) {
  // advance sequencer at sample_clock; mix_active_voices returns normalised f32
  float s = mix_active_voices_f32(state, sample_clock++);
  audio_write_sample(audio, i, s, s);
}
state->sample_clock = sample_clock;
```

### Case 4: Spatial / positional audio

Mix N mono sources into the stereo output buffer. Each source has a gain pair `(left_gain, right_gain)` computed from position. For simple games:

```c
i16 L = 0, R = 0;
for each active source:
  i16 src = source->oscillator_or_buffer_sample;
  L = clamp16(L + (i32)(src * source->left_gain));
  R = clamp16(R + (i32)(src * source->right_gain));
*out++ = L;
*out++ = R;
```

### Case 5: Dynamic / adaptive music (layers)

Pre-load N layered tracks (e.g., calm, tense, combat). Mix them with variable gains that transition smoothly:

```c
for (i32 i = 0; i < audio->sample_count; i++) {
  float s = 0;
  for (int t = 0; t < NUM_TRACKS; t++)
    s += track_sample(state->tracks[t]) * lerp(state->gain[t], state->target_gain[t], alpha);
  audio_write_sample(audio, i, s, s);
}
```

Drive `target_gain[]` from game state (e.g., proximity to enemies).

### Case 6: Silence / muted state

Always write silence explicitly; never leave `samples` uninitialised:

```c
if (state->muted || !audio->is_initialized) {
  memset(audio->samples, 0, (size_t)audio->sample_count * sizeof(i16) * 2);
  return;
}
```

Leaving the buffer partially filled causes pops when the backend ships stale data.

### Case 7: Sample rate mismatch

If a sound asset's sample rate differs from `audio->samples_per_second`, resample it before mixing. The simplest approach is nearest-neighbour:

```c
float ratio = (float)asset->sample_rate / (float)audio->samples_per_second;
float src_pos = state->asset_pos;
for (i32 i = 0; i < audio->sample_count; i++) {
  i32 idx = (i32)src_pos;
  ((i16 *)audio->samples)[i * 2]     = asset->data[idx * 2];     // L
  ((i16 *)audio->samples)[i * 2 + 1] = asset->data[idx * 2 + 1]; // R
  src_pos += ratio;
}
state->asset_pos = src_pos;
```

Linear interpolation eliminates aliasing for pitch shifts < 2×.

### Case 8: Hot-reloading game DLL with audio running

The game DLL owns `get_audio_samples`. On reload:

- The backend keeps writing silence until the new DLL is loaded (check `game_main_code->functions.get_audio_samples != NULL`).
- Restart any phase accumulators / decode positions that were local to the old DLL's static state.
- Persistent audio state (ring buffers, sample clock) should live in `GameMemory`, not in DLL-local statics.

### Case 9: Volume / gain per voice

Apply gain in the mixer before writing to `out->samples`. Never delegate to a backend API:

```c
for (i32 i = 0; i < audio->sample_count; i++) {
  i16 src = voice_next_sample(v);
  i32 scaled_L = (i32)((float)src * v->gain * v->pan_left);
  i32 scaled_R = (i32)((float)src * v->gain * v->pan_right);
  out_L = clamp_i16(out_L + scaled_L);
  out_R = clamp_i16(out_R + scaled_R);
}
```

### Case 10: Multiple voices + sub-buses

Allocate intermediate `float32` bus buffers from `GameMemory` (sized to `max_sample_count`). Mix voices into buses, apply bus gain, then sum into `out->samples` with a single float-to-i16 clamp pass:

```c
// 1. Zero bus buffers (allocated in GameMemory)
memset(bus_music, 0, sample_count * sizeof(float) * 2);
memset(bus_sfx,   0, sample_count * sizeof(float) * 2);

// 2. Mix voices into their bus
for each music_voice: mix_voice_float(bus_music, voice, sample_count);
for each sfx_voice:   mix_voice_float(bus_sfx,   voice, sample_count);

// 3. Apply bus gains and write final output via audio_write_sample
for (i32 i = 0; i < sample_count; i++) {
  float L = bus_music[i*2]   * state->music_gain
          + bus_sfx[i*2]     * state->sfx_gain;
  float R = bus_music[i*2+1] * state->music_gain
          + bus_sfx[i*2+1]   * state->sfx_gain;
  audio_write_sample(audio, i, L, R);  /* clamps + converts to backend format */
}
```

`float32` intermediate prevents clipping when many voices accumulate. `audio_write_sample` handles the format conversion once, at the output boundary.

### Case 11: Looping sound with wrap boundary

A loop point can fall in the middle of a single `get_audio_samples` call. Handle the partial copy explicitly:

```c
i32 remaining = audio->sample_count;
i32 write_pos = 0;
while (remaining > 0) {
  i32 available = asset->loop_end - v->cursor; // frames until loop point
  i32 to_copy   = remaining < available ? remaining : available;
  mix_range(out, write_pos, asset, v->cursor, to_copy);
  v->cursor += to_copy;
  write_pos += to_copy;
  remaining -= to_copy;
  if (v->cursor >= asset->loop_end)
    v->cursor = asset->loop_start; // wrap
}
```

This handles the loop point falling anywhere in the call, including clips shorter than `sample_count` that wrap multiple times.

### Case 12: Audio-video sync using sample clock

Drive animation and cutscene events from `sample_clock`, not the frame timer:

```c
double audio_time_seconds = (double)audio->sample_clock
                          / (double)audio->samples_per_second;

advance_timeline(state, audio_time_seconds); // subtitles, events, flip sync
```

`sample_clock` must be added to `GameAudioOutputBuffer` and incremented by the backend after each `get_audio_samples` call (`audio->sample_clock += audio->sample_count`). This gives hardware-accurate playback time on any backend.

### Case 13: Focus loss / game paused

Currently implemented: `game_get_audio_samples` writes silence and returns early when `game->is_paused` is true. `game` is `HandMadeHeroGameState *` cast from `memory->permanent_storage`. The platform backends are unaware of pause state:

```c
// In game_get_audio_samples (game callback — current approach):
if (!audio_buffer->is_initialized || audio_buffer->sample_count == 0) return;
HandMadeHeroGameState *game = (HandMadeHeroGameState *)memory->permanent_storage;
if (game->is_paused) {
  memset(audio_buffer->samples_buffer, 0,
         (size_t)audio_buffer->sample_count
           * (size_t)audio_format_bytes_per_sample(audio_buffer->format));
  return;  // backend sends silence; stream stays fed, no underruns
}
```

Future ideal — add `backend_pause_audio` to stop the hardware clock during pause (saves power, reduces ALSA ring-buffer churn):

```c
// In the main loop (aspirational):
if (!engine.is_paused && game->audio.is_initialized) {
  u32 n = backend_get_samples_to_write(&game->audio);
  if (n > 0) {
    game->audio.sample_count = (i32)n;
    game_main_code.functions.get_audio_samples(&game->memory, &game->audio);
    backend_send_samples(&game->audio);
  }
} else {
  backend_pause_audio(&game->audio); // no-op on backends that self-drain
}
```

### Case 14: Low-pass filter (effect with persistent history state)

Filters require state across calls. Store it in `GameMemory` so hot-reload preserves continuity:

```c
typedef struct { float prev_L; float prev_R; float cutoff; } LowPassState;

// In get_audio_samples, after mixing into float bus_sfx[sample_count*2]:
for (i32 i = 0; i < audio->sample_count; i++) {
  // one-pole IIR: y[n] = (1-a)*x[n] + a*y[n-1]
  lpf->prev_L = (1.0f - lpf->cutoff) * bus_sfx[i*2]   + lpf->cutoff * lpf->prev_L;
  lpf->prev_R = (1.0f - lpf->cutoff) * bus_sfx[i*2+1] + lpf->cutoff * lpf->prev_R;
  audio_write_sample(audio, i, lpf->prev_L, lpf->prev_R);
}
```

If `LowPassState` were a DLL-local static, hot-reload resets `prev_L/R` to 0 and produces a click at the boundary.

---

## Potential Issues Checklist

| Issue                                               | Symptom                                    | Fix                                                                                                                                                                                                      |
| --------------------------------------------------- | ------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Buffer overflow                                     | Crash or corruption                        | Always cap `sample_count` to `max_sample_count`                                                                                                                                                          |
| Stale buffer data                                   | Clicks/pops when muted                     | ✅ Done: `memset` to 0 on **every** paused frame in `game_get_audio_samples` (no `was_paused` static shortcut)                                                                                           |
| Phase discontinuity on reload                       | Click at reload boundary                   | Store phase in `GameMemory`                                                                                                                                                                              |
| ALSA underrun                                       | Distorted output, `EPIPE` errors           | Increase `safety_samples`; call `snd_pcm_recover`                                                                                                                                                        |
| Raylib buffer missed                                | Dropped audio frame                        | Loop `GetSamplesToWrite` up to N times per frame                                                                                                                                                         |
| Sample rate mismatch                                | Wrong pitch / wrong duration               | ✅ Done: `HHGameAudioState.samples_per_second` synced from `audio_buffer->samples_per_second` each callback; use `duration_ms * audio->samples_per_second / 1000.0f` — never hardcode `48000` or `48.0f` |
| `is_initialized` not checked                        | Crash on first frames                      | ✅ Done: early-return guard at top of `game_get_audio_samples`                                                                                                                                           |
| `is_initialized` only checked at startup            | Silent corruption on device change         | ✅ Done: backends re-check before every `get_samples_to_write` call; game callback guards on entry                                                                                                       |
| Emoji / non-ASCII in `printf`                       | Compiler warns/errors                      | Use `\u25xx` BMP-only escapes or literal UTF-8 bytes                                                                                                                                                     |
| Backend-specific type in `_common/` or `game/`      | Dead fields on other backends              | Move to backend-private struct                                                                                                                                                                           |
| `get_audio_samples` called from audio thread        | Malloc/lock inside causes dropout or crash | Use ring-buffer shim; fill on main thread only                                                                                                                                                           |
| Wrong sample format assumed                         | Distorted output on CoreAudio / WASAPI     | ✅ Done: `AudioSampleFormat` in `GameAudioOutputBuffer`; use `audio_write_sample()`                                                                                                                      |
| Debug function imported across layer boundary       | Breaks on new backend, coupling bloat      | ✅ Done: F1 key prints `GameAudioOutputBuffer` fields directly; `raylib/audio.h` removed from game adapter                                                                                               |
| Gain/pitch applied via backend API                  | Behaviour diverges between backends        | Apply in mixer in game code before writing to `samples`                                                                                                                                                  |
| Voice/mixer state in DLL-local static               | State reset on hot-reload; click/pop       | ✅ Done: `was_paused` static removed; all audio state (cursors, phases, filters, `samples_per_second`) in `GameMemory` via `HandMadeHeroGameState`                                                       |
| Mixing directly to `i16` across many voices         | Clipping/distortion on loud scenes         | ✅ Done: mix in normalised `f32`; call `audio_write_sample()` — never cast `samples_buffer` to `i16*` directly                                                                                           |
| Loop point mid-call not handled                     | Reads garbage past end of asset            | Use a partial-copy loop with explicit wrap logic in the mixer                                                                                                                                            |
| Effect history not preserved across calls           | Clicks at call boundaries (filter, reverb) | Store history in `GameMemory`; effects must accept arbitrary chunk sizes                                                                                                                                 |
| Effect requires min buffer size, backend gives less | Convolution artefacts, HRTF incorrect      | Document min buffer per effect; verify all backends can supply it                                                                                                                                        |
| Pause handled inside `get_audio_samples`            | Game logic leaks into audio callback       | ✅ Done: `game_get_audio_samples` writes silence and returns early when paused — platform backends are fully agnostic                                                                                    |
| A/V sync derived from frame timer                   | Drifts under CPU load                      | Add `sample_clock` to `GameAudioOutputBuffer`; derive time from it                                                                                                                                       |
| `is_initialized` is plain `bool` across threads     | Data race on device change (CoreAudio)     | Use atomic release-store / acquire-load before adding a threaded backend                                                                                                                                 |
