# Lesson 15 — X11 Backend and Polish

## What you'll learn
- What the X11 backend does (and why it's 2× the code)
- Key fixes vs naïve X11 + ALSA code
- Background music loop
- `best_score` persistence across restarts
- Build system summary

---

## X11 backend overview

The X11 backend replaces Raylib with three Linux-specific APIs:

| API | Purpose | Header |
|-----|---------|--------|
| Xlib | Window creation, keyboard events | `X11/Xlib.h` |
| GLX | OpenGL context inside an X11 window | `GL/glx.h` |
| ALSA | PCM audio output | `alsa/asoundlib.h` |

The game loop is identical to Raylib: update → render → audio → display.
The difference is ~150 extra lines of boilerplate.

---

## Critical X11 fixes

### 1. XkbKeycodeToKeysym (not XLookupKeysym)

```c
KeySym sym = XkbKeycodeToKeysym(g_display, event.xkey.keycode, 0, 0);
```

`XLookupKeysym` uses the raw key table without layout mapping — on a
non-QWERTY keyboard, 'A' may not be at the expected position.
`XkbKeycodeToKeysym` uses the active keyboard layout, giving correct key names.

### 2. XkbSetDetectableAutoRepeat

```c
XkbSetDetectableAutoRepeat(g_display, True, NULL);
```

By default, X11 generates fake KeyPress events while a key is held.
This would cause `game_sustain_sound` to see repeated "just-pressed" events.
`SetDetectableAutoRepeat` suppresses fake repeats: KeyPress fires once on
press, KeyRelease fires once on release.

### 3. WM_DELETE_WINDOW protocol

```c
g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);
```

Without this, clicking the window X button sends `DestroyNotify`, which
kills the process immediately — leaving ALSA in an undefined state and
hanging the terminal.  With `WM_DELETE_WINDOW`, the window manager sends
a `ClientMessage` instead, and we can call `platform_shutdown()` cleanly.

---

## ALSA: start_threshold fix

```c
snd_pcm_sw_params_t *sw;
snd_pcm_sw_params_alloca(&sw);
snd_pcm_sw_params_current(g_alsa_pcm, sw);  // load defaults FIRST
snd_pcm_sw_params_set_start_threshold(g_alsa_pcm, sw, 1);
snd_pcm_sw_params(g_alsa_pcm, sw);
```

Without `start_threshold=1`, ALSA waits for the ring buffer to fill to
its default threshold before starting playback.  That default is often
the full buffer size (~4096 frames = ~93 ms) — causing silent audio for
almost a second after startup.  Setting threshold to 1 means playback
starts as soon as the first frame arrives.

---

## ALSA underrun recovery

```c
int rc = snd_pcm_writei(g_alsa_pcm, audio_samples,
                        (snd_pcm_uframes_t)g_alsa_period_size);
if (rc == -EPIPE) {
    // Buffer underrun: ALSA ran out of data — recover and continue
    snd_pcm_prepare(g_alsa_pcm);
}
```

`-EPIPE` means ALSA's ring buffer was empty when the hardware tried to
read from it (underrun).  `snd_pcm_prepare` resets the PCM stream to
the `PREPARED` state so writing can resume.

Unlike Raylib's `IsAudioStreamProcessed` gate, the X11 backend calls
`game_get_audio_samples` every frame synchronously.

---

## GL_RGBA vs GL_BGRA

```c
// CORRECT:
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GAME_W, GAME_H, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, NULL);
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GAME_W, GAME_H,
                GL_RGBA, GL_UNSIGNED_BYTE, g_pixel_buf);
```

`GAME_RGBA(r, g, b, a)` packs bytes as `[R][G][B][A]` in memory.
`GL_RGBA` reads them in the same order.  Using `GL_BGRA` would swap R↔B →
white asteroids would appear cyan, cyan asteroids appear red.

---

## Background music loop

```c
// In asteroids_update() — game loop plays music continuously:
if (!game_is_sound_playing(&state->audio, SOUND_MUSIC_GAMEPLAY)) {
    game_play_sound(&state->audio, SOUND_MUSIC_GAMEPLAY);
}
```

`SOUND_MUSIC_GAMEPLAY` has duration 3.0s and `ENVELOPE_FADEOUT`.
When it expires, `game_is_sound_playing` returns false, and we restart it.
The FADEOUT envelope means the last few milliseconds fade out, minimising
the click at loop point.

On restart, play the jingle instead:

```c
void asteroids_init(GameState *state) {
    // ...
    game_play_sound(&state->audio, SOUND_MUSIC_RESTART);
}
```

---

## best_score persistence

`best_score` survives restarts because `asteroids_init` saves and restores it:

```c
void asteroids_init(GameState *state) {
    int saved_best         = state->best_score;
    GameAudioState saved_audio = state->audio;

    memset(state, 0, sizeof(*state));

    state->best_score = saved_best;  // restored after memset
    state->audio      = saved_audio;
    // ...
}
```

It does NOT persist across process restarts (no file I/O).  To persist to
disk you'd use `fopen`/`fwrite` — a natural next extension.

---

## Build system summary

```bash
# Build with Raylib (default):
bash build-dev.sh

# Build with X11 (Linux only):
bash build-dev.sh --backend=x11

# Debug build (ASAN + UBSan):
bash build-dev.sh --debug

# Build and run:
bash build-dev.sh -r

# Combined:
bash build-dev.sh --backend=x11 --debug -r
```

Key compiler flags:
```
-DRENDER_COORD_MODE=1   explicit coordinate mode only
-Wall -Wextra           all common warnings
-g -O0                  debug symbols, no optimisation
-fsanitize=address,undefined  (--debug only)
```

Common sources (shared by both backends):
```
src/game/game.c
src/game/audio.c
src/utils/draw-shapes.c
src/utils/draw-text.c
```

Backend-specific:
```
src/platforms/raylib/main.c  + -lraylib -lpthread -ldl
src/platforms/x11/main.c    + -lX11 -lxkbcommon -lGL -lGLX -lasound
```

---

## Key takeaways

1. X11 needs 3 APIs (Xlib, GLX, ALSA); Raylib wraps all three in one call.
2. `XkbSetDetectableAutoRepeat` suppresses fake key-repeat events.
3. `WM_DELETE_WINDOW` lets us call `platform_shutdown()` on window close.
4. ALSA `start_threshold=1` prevents a silent first second of audio.
5. `GL_RGBA` (not `GL_BGRA`) matches our `[R][G][B][A]` pixel layout.
6. Music loop: poll `game_is_sound_playing` and restart when expired.
7. `best_score` survives restarts; save/restore in `asteroids_init`.
