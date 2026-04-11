# Lesson 25 -- Background Asset Loading

## Observable Outcome
The game window appears and gameplay starts instantly -- the player can fly and dodge obstacles from frame 1. There is no loading screen. After approximately 2-5 seconds (depending on disk speed), background music fades in. The audio thread loads all 15 clips (13 WAV + 2 OGG) in parallel with gameplay. Sound effects become available as they finish loading. The game is fully playable throughout the loading process.

## New Concepts (max 3)
1. Background threading with `pthread_create` and `pthread_detach` for fire-and-forget audio loading
2. Thread-safe initialization flag: a single `initialized` int that transitions from 0 to 1, checked each frame
3. Graceful degradation: the game runs silently (outputting zeroed audio buffers) until the loading thread completes

## Prerequisites
- Lesson 18 (WAV loading, AudioClip, loaded_audio_mix)
- Lesson 19 (OGG loading, music voice, game_audio_init)
- Basic understanding of POSIX threads (pthread_create, pthread_detach)

## Background

Loading 15 audio files from disk takes significant time -- hundreds of milliseconds to several seconds depending on the storage medium. If we load synchronously during `game_init`, the window stays black (or does not appear at all) until all assets are ready. This creates a terrible first impression: the user clicks the executable and nothing visually happens for seconds.

The solution is to load audio on a background thread while the game runs. The game starts in PLAYING phase immediately with the player visible and controllable. The audio mixer checks whether clips are loaded before trying to play them -- if `initialized` is false, it outputs silence (zeroed buffers). When the background thread finishes, it sets `initialized = 1`, and the next frame's audio callback starts mixing real audio.

We use `pthread_detach` instead of `pthread_join` because we do not need to wait for the thread to complete. The game simply polls the `initialized` flag each frame. This is a "fire and forget" pattern: launch the thread, detach it, and let it clean up its own resources when it returns. The alternative -- `pthread_join` in the game loop -- would block the game thread and defeat the purpose of background loading.

The thread safety model here is deliberately simple. The `initialized` flag is the only shared state that transitions (from 0 to 1). It is an int, which on all modern architectures is atomically written. The audio data (clip arrays, sample buffers) is written by the loading thread before setting `initialized = 1`, and read by the mixer only after checking `initialized == 1`. This creates a happens-before relationship without requiring mutexes or atomics -- the flag acts as a release/acquire barrier in practice.

## Code Walkthrough

### Step 1: The audio loading thread function (game/main.c)

```c
static void *audio_load_thread_fn(void *arg) {
  GameAudio *audio = (GameAudio *)arg;
  game_audio_init(audio); /* Blocking -- loads all 15 clips */
  printf("Audio thread: loading complete\n");
  return NULL;
}
```

This function is the thread entry point. It receives the GameAudio pointer as its argument, calls `game_audio_init` which loads all clips synchronously (blocking within this thread), and returns. The `printf` confirms completion in the terminal.

### Step 2: Thread creation in game_init (game/main.c)

```c
void game_init(GameState *state, PlatformGameProps *props) {
  /* ... memset, player_init, obstacles_init ... */

  /* Initialize audio state (no clips loaded yet) */
  game_audio_init_state(&state->audio);

  /* Launch background loading thread */
  {
    pthread_t audio_thread;
    pthread_create(&audio_thread, NULL, audio_load_thread_fn, &state->audio);
    pthread_detach(audio_thread); /* Fire and forget */
  }

  /* ... particles_init, high_scores_load ... */
}
```

`game_audio_init_state` zeroes the audio state and sets default volumes without loading any files. The `pthread_create` call spawns the loading thread, passing `&state->audio` as the argument. `pthread_detach` immediately releases the thread handle -- we never call `pthread_join`.

### Step 3: game_audio_init_state vs game_audio_init (game/audio.c)

Two initialization functions serve different purposes:

```c
/* Called synchronously from game_init -- just sets up state */
void game_audio_init_state(GameAudio *audio) {
  memset(audio, 0, sizeof(*audio));
  loaded_audio_init(&audio->loaded);
  rng_seed(&s_audio_rng, 9876);
  for (int i = 0; i < CLIP_COUNT; i++) {
    audio->clip_indices[i] = -1;
  }
}

/* Called on the background thread -- loads all 15 clips (blocking) */
int game_audio_init(GameAudio *audio) {
  game_audio_init_state(audio);
  for (int i = 0; i < CLIP_COUNT; i++) {
    game_audio_load_one_clip(audio, i);
  }
  return 0;
}
```

All `clip_indices` are initialized to -1 (invalid). As each clip loads, its index is set to a valid value. The `initialized` flag is set by `game_audio_load_one_clip` when the last clip finishes:

```c
int game_audio_load_one_clip(GameAudio *audio, int clip_index) {
  /* ... load WAV or OGG ... */

  if (clip_index >= CLIP_COUNT - 1) {
    int loaded = 0;
    for (int i = 0; i < CLIP_COUNT; i++) {
      if (audio->clip_indices[i] >= 0) loaded++;
    }
    printf("Game audio: loaded %d/%d clips\n", loaded, CLIP_COUNT);
    audio->initialized = (loaded > 0) ? 1 : 0;
    return 1; /* All done */
  }
  return 0;
}
```

### Step 4: Graceful silence while loading (game/audio.c)

The mixer outputs silence when audio is not yet initialized:

```c
void game_audio_mix(GameAudio *audio, AudioOutputBuffer *buf, int num_frames) {
  if (!audio->initialized) {
    memset(buf->samples, 0, (size_t)num_frames * AUDIO_BYTES_PER_FRAME);
    return;
  }
  loaded_audio_mix(&audio->loaded, buf, num_frames);
}
```

The platform audio callback calls `game_get_audio_samples` every chunk (4096 frames). If the loading thread has not finished, we memset the buffer to zero -- producing silence. No pops, no clicks, no undefined behavior.

### Step 5: Auto-starting music after load (game/main.c)

Each frame in game_update, we check if loading just completed:

```c
/* Check if background audio thread finished loading */
if (state->audio.initialized && !state->audio.loaded.music_voice.active) {
  game_audio_play_music(&state->audio, CLIP_BG_MUSIC, 0.3f);
}
```

This condition triggers exactly once: when `initialized` becomes true (audio loaded) and the music voice is not yet active (has not been started). After this call, `music_voice.active` becomes 1, preventing the condition from firing again.

### Step 6: Play functions guard on initialized (game/audio.c)

All play functions check the flag before accessing clip data:

```c
void game_audio_play_ex(GameAudio *audio, ClipID id,
                        float volume, float pitch) {
  if (!audio->initialized) return;
  if (id < 0 || id >= CLIP_COUNT) return;

  int idx = audio->clip_indices[id];
  if (idx < 0) return;
  /* ... pitch variation, volume adjustment, play ... */
}
```

The triple guard (initialized, valid ID, valid clip index) ensures we never access unloaded clip data. Game code can call `game_audio_play` at any time without knowing whether loading is complete -- failed plays are silently ignored.

### Step 7: Linking with pthreads

The build script includes `-lpthread`:

```bash
BACKEND_LIBS="-lm -lX11 -lxkbcommon -lasound -lGL -lGLX -lpthread"
```

Both the X11 and Raylib backends link against pthreads for the background loading thread.

## Common Mistakes

**Using `pthread_join` in the game loop.** Joining blocks the calling thread until the target thread exits. If you join in `game_init`, the game window stays frozen during loading -- defeating the entire purpose of background loading.

**Reading clip data before checking `initialized`.** Without the guard in `game_audio_play_ex`, the mixer could read a clip that is half-loaded: the `samples` pointer might be set but `sample_count` might still be zero (or vice versa). The `initialized` flag ensures all clip data is fully written before any reads begin.

**Not zeroing the audio buffer during silent frames.** If `game_audio_mix` returns without writing to the buffer, the platform backend sends whatever garbage was in the buffer to the speakers. This produces loud noise or clicks. Always memset to zero for silence.

**Detaching a thread and then accessing the pthread_t variable.** After `pthread_detach`, the `audio_thread` variable is no longer meaningful. The code correctly scopes it within a block `{ pthread_t audio_thread; ... }` to prevent accidental reuse.

**Setting `initialized = 1` before all clips are loaded.** If you set the flag after the first clip loads instead of the last, the mixer will try to play clips that have not been decoded yet, accessing invalid memory. The flag must be the very last thing set after all loading is complete.
