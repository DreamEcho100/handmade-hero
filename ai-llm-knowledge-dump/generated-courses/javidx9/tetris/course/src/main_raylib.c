/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  main_raylib.c  —  Raylib Platform Backend  (Phase 2)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  WHAT IS A PLATFORM LAYER?
 *  ─────────────────────────
 *  Handmade Hero principle: The platform layer is a thin translator.
 *
 *  Its only jobs are:
 *    1. Map OS/library events  →  GameInput  (keyboard → button states)
 *    2. Map OS timer           →  delta_time (seconds since last frame)
 *    3. Map OS audio callbacks →  AudioOutputBuffer (our PCM samples)
 *    4. Call game functions;   the game NEVER calls platform functions back.
 *
 *  The game code (game.c / game.h) is 100% platform-agnostic.  You could
 *  swap this file for main_x11.c and the game would behave identically.
 *
 *  RAYLIB SPECIFICS
 *  ────────────────
 *  Raylib is a "batteries-included" C library that wraps GLFW + OpenAL.
 *  Because it already handles windowing, input, and audio, our platform
 *  layer is extremely thin:
 *
 *    • Input:  IsKeyDown / IsKeyPressed / IsKeyReleased (polling, not events)
 *    • Timer:  GetFrameTime()  returns seconds since previous frame
 *    • Audio:  double-buffered AudioStream; IsAudioStreamProcessed() = "ready"
 *    • Display: Image → Texture2D → UpdateTexture → DrawTexture every frame
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "game.h"
#include "platform.h"

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>  /* JS: like import { malloc, free } — heap allocation */
#include <string.h>  /* JS: like import { memset, memcpy } — raw memory ops */

/* ══════════════════════════════════════════════════════════════════════════
 * Platform State
 * ══════════════════════════════════════════════════════════════════════════
 *
 * JS: Think of this as a module-level object that persists for the entire
 * lifetime of the program — like a top-level `const state = {}` in Node.
 *
 * C: `static` at file scope means "this symbol is private to this .c file"
 *    (like a module-private variable).  The `= {0}` zero-initialises every
 *    field — equivalent to `= {}` in JS but explicitly filling with zeroes.
 */

typedef struct {
  Texture2D texture;         /* GPU-side handle for our pixel buffer */
  AudioStream stream;        /* Raylib's double-buffered PCM stream handle */
  int16_t *sample_buffer;    /* JS: Int16Array — heap-allocated PCM scratch buffer */
  int sample_buffer_size;    /* capacity in *stereo frames* (not bytes) */
  int buffer_size_frames;    /* Raylib internal buffer size (frames per fill) */
} RaylibState;

/* JS: like `let g_raylib = {}` at module scope, but zero-filled at startup */
static RaylibState g_raylib = {0};

/* ══════════════════════════════════════════════════════════════════════════
 * Audio Initialization  (Casey's Latency Model — Raylib Adaptation)
 * ══════════════════════════════════════════════════════════════════════════
 *
 *  Casey's Latency Model (from Handmade Hero):
 *  ────────────────────────────────────────────
 *  Instead of letting the OS decide how much audio to buffer, we decide.
 *  We keep exactly `latency_samples` worth of audio ahead of the play cursor
 *  at all times.  That gives us predictable, low latency.
 *
 *  Raylib vs ALSA:
 *  ───────────────
 *  ALSA (the X11 backend) lets us call snd_pcm_delay() to query exactly how
 *  many samples are queued in the hardware buffer.  Raylib does NOT expose
 *  this.  Instead, IsAudioStreamProcessed() returns a plain boolean: "has
 *  a buffer been consumed yet?"
 *
 *  Consequence: we need a larger safety margin and we fill ALL consumed
 *  buffers every frame rather than calculating an exact write amount.
 *
 * Handmade Hero principle: Audio phase is continuous.
 *    inst->phase is never reset between fills; the sine wave is seamless.
 *
 * ══════════════════════════════════════════════════════════════════════════
 */

int platform_audio_init(PlatformAudioConfig *config) {
  /* JS: like `audioCtx = new AudioContext()` */
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "Failed to initialize audio device\n");
    config->is_initialized = 0;
    return 1; /* non-zero = failure (C convention; no exceptions here) */
  }

  config->samples_per_second = 48000; /* 48 kHz — standard for games */

  /* ── Casey's latency calculations ──────────────────────────────────────
   *
   * samples_per_frame  = how many stereo samples the game produces each tick
   * latency_samples    = target "buffer ahead" depth (frames_of_latency ticks)
   * safety_samples     = small extra cushion so we never underrun
   *
   * JS analogy: imagine you're streaming video.  latency_samples is the
   * number of pre-downloaded frames you keep in reserve so that a short
   * network hiccup doesn't cause a stutter.
   */
  config->samples_per_frame = config->samples_per_second / config->hz;
  config->latency_samples   = config->samples_per_frame * config->frames_of_latency;
  config->safety_samples    = config->samples_per_frame / 3;
  config->running_sample_index = 0; /* monotonic counter of all samples ever written */

  /*
   * Buffer size for Raylib:
   *
   * We can't query "how many samples are queued", so we use 2× frame size
   * (~33 ms at 60 fps) as our buffer.  Larger than ALSA's buffer, but
   * necessary because IsAudioStreamProcessed() is just true/false.
   *
   * Think of it like double-buffering a canvas — you write to the back
   * while the front is displayed.
   * JS: like double-buffering a canvas — you write to the back while front is displayed
   */
  config->buffer_size_samples    = config->samples_per_frame * 2;
  g_raylib.buffer_size_frames    = config->buffer_size_samples;

  /* Print diagnostic info so students can see the calculated values */
  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 RAYLIB AUDIO (Casey's Latency Model)\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:     %d Hz\n", config->samples_per_second);
  printf("  Game update:     %d Hz\n", config->hz);
  printf("  Samples/frame:   %d (%.1f ms)\n", config->samples_per_frame,
         (float)config->samples_per_frame / config->samples_per_second * 1000.0f);
  printf("  Target latency:  %d samples (%.1f ms, %d frames)\n",
         config->latency_samples,
         (float)config->latency_samples / config->samples_per_second * 1000.0f,
         config->frames_of_latency);
  printf("  Safety margin:   %d samples (%.1f ms)\n", config->safety_samples,
         (float)config->safety_samples / config->samples_per_second * 1000.0f);
  printf("  Buffer size:     %d samples (%.1f ms)\n",
         config->buffer_size_samples,
         (float)config->buffer_size_samples / config->samples_per_second * 1000.0f);

  /*
   * MUST call SetAudioStreamBufferSizeDefault BEFORE LoadAudioStream.
   * Raylib allocates its internal DMA buffers at stream creation time;
   * if we try to change the size later it has no effect.
   */
  SetAudioStreamBufferSizeDefault(config->buffer_size_samples);

  /*
   * Create the stream:  48000 Hz, 16-bit signed PCM, 2 channels (stereo).
   * JS: think `audioCtx.createBuffer(2, bufferSize, 48000)`
   */
  g_raylib.stream = LoadAudioStream(config->samples_per_second, 16, 2);

  if (!IsAudioStreamValid(g_raylib.stream)) {
    fprintf(stderr, "Failed to create audio stream\n");
    CloseAudioDevice();
    config->is_initialized = 0;
    return 1;
  }

  /*
   * Allocate our CPU-side scratch buffer.
   *
   * JS: like `new Int16Array(bufferSize * 2)`  (* 2 for stereo interleaved)
   * C:  malloc returns a void*, we cast to int16_t*
   *     sizeof(int16_t) = 2 bytes; int16_t is a 16-bit signed integer type
   *
   * "4 frames" gives us room to generate audio a little ahead of schedule.
   */
  g_raylib.sample_buffer_size = config->samples_per_frame * 4;
  g_raylib.sample_buffer =
      (int16_t *)malloc(g_raylib.sample_buffer_size * 2 * sizeof(int16_t));
      /*                                               ^            ^
       *                                           stereo       bytes per sample
       */

  if (!g_raylib.sample_buffer) {
    /* malloc returns NULL on failure — always check! */
    fprintf(stderr, "Failed to allocate audio buffer\n");
    UnloadAudioStream(g_raylib.stream);
    CloseAudioDevice();
    config->is_initialized = 0;
    return 1;
  }

  /*
   * Pre-fill BOTH Raylib buffers with silence before starting playback.
   *
   * WHY:  Raylib is double-buffered internally.  When PlayAudioStream()
   *       is called the device starts consuming buffer A immediately.
   *       If buffer B is empty (garbage), we'll hear a click or pop.
   *       We fill both with zeroes (silence) so the device has latency_samples
   *       worth of clean audio before our game loop even runs.
   *
   * JS:   like calling `bufferSource.start()` only after you've pre-filled
   *       the AudioWorklet's ring-buffer.
   *
   * memset: fills a block of memory with a byte value.
   * JS: like `buffer.fill(0)`
   */
  memset(g_raylib.sample_buffer, 0,
         g_raylib.buffer_size_frames * 2 * sizeof(int16_t));

  UpdateAudioStream(g_raylib.stream, g_raylib.sample_buffer,
                    g_raylib.buffer_size_frames); /* fill buffer A */
  UpdateAudioStream(g_raylib.stream, g_raylib.sample_buffer,
                    g_raylib.buffer_size_frames); /* fill buffer B */

  /* NOW start playback — both buffers are primed with silence */
  PlayAudioStream(g_raylib.stream);

  config->is_initialized = 1;
  printf("  Pre-filled:      %d samples (%.1f ms)\n",
         g_raylib.buffer_size_frames * 2,
         (float)(g_raylib.buffer_size_frames * 2) / config->samples_per_second *
             1000.0f);
  printf("═══════════════════════════════════════════════════════════\n\n");

  return 0;
}

void platform_audio_shutdown(void) {
  if (g_raylib.sample_buffer) {
    free(g_raylib.sample_buffer);   /* JS: like `buffer = null` — releases heap */
    g_raylib.sample_buffer = NULL;  /* set to NULL so double-free is safe */
  }
  if (IsAudioStreamValid(g_raylib.stream)) {
    StopAudioStream(g_raylib.stream);
    UnloadAudioStream(g_raylib.stream);
  }
  CloseAudioDevice();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Audio Update  (Casey's Model Adapted for Raylib)
 * ══════════════════════════════════════════════════════════════════════════
 *
 *  Called once per game frame (inside the main loop).
 *
 *  Raylib's double-buffer model:
 *  ─────────────────────────────
 *    Buffer A ──► audio device (playing)
 *    Buffer B ──► available for CPU to fill
 *
 *  When the device finishes buffer A it flips: A becomes writable, B plays.
 *  IsAudioStreamProcessed() returns true when a buffer has been consumed
 *  and is now waiting for fresh samples.
 *
 *  Strategy: fill ALL available (processed) buffers this frame.
 *  ──────────────────────────────────────────────────────────────
 *  On a normal frame we'll fill 0 or 1 buffer.
 *  On a frame spike (GC pause, OS preemption) we might need to fill 2.
 *  The max_buffers=4 guard prevents an infinite loop if something goes wrong.
 *
 * ══════════════════════════════════════════════════════════════════════════
 */

static void platform_audio_update(GameState *game_state,
                                  PlatformAudioConfig *config) {
  if (!config->is_initialized)
    return;

  int buffers_filled = 0;
  const int max_buffers = 4; /* safety cap — handles frame spikes gracefully */

  while (buffers_filled < max_buffers &&
         IsAudioStreamProcessed(g_raylib.stream)) {
    /*
     * Build an AudioOutputBuffer descriptor.
     *
     * JS: think `{ data: Int16Array, sampleRate: 48000, length: N }`
     * C:  compound literal  { .field = value }  — initialises by field name
     *     (like object shorthand, but the struct layout is fixed at compile time)
     */
    AudioOutputBuffer buffer = {
        .samples            = g_raylib.sample_buffer,
        .samples_per_second = config->samples_per_second,
        .sample_count       = g_raylib.buffer_size_frames,
    };

    /*
     * Ask the game to fill `buffer.sample_count` stereo PCM frames.
     * The game writes its sine waves / samples into buffer.samples.
     * Note: the game never knows it's running under Raylib.
     */
    game_get_audio_samples(game_state, &buffer);

    /* Hand the filled buffer to Raylib — it copies to its DMA buffer */
    UpdateAudioStream(g_raylib.stream, buffer.samples, buffer.sample_count);

    /* Keep our monotonic counter updated (useful for sync/debug) */
    config->running_sample_index += buffer.sample_count;
    buffers_filled++;
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Platform Init / Shutdown
 * ══════════════════════════════════════════════════════════════════════════
 */

int platform_init(PlatformGameProps *props) {
  /*
   * FLAG_WINDOW_RESIZABLE  — allow the user to drag window edges
   * FLAG_VSYNC_HINT        — ask the driver to sync to monitor refresh
   *
   * SetConfigFlags must be called BEFORE InitWindow; Raylib reads these
   * flags during window creation.
   */
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(props->width, props->height, props->title);
  SetTargetFPS(60); /* fallback cap if VSync isn't available */

  /*
   * Image → Texture2D pipeline (software renderer to GPU)
   * ──────────────────────────────────────────────────────
   *
   * Our game renders into a CPU-side pixel array (props->backbuffer.pixels).
   * To display it, we need to get those pixels onto the GPU.  The pipeline:
   *
   *   1. Image       — Raylib struct that wraps our existing pixel pointer.
   *                    NO copy yet; it just describes the memory layout.
   *   2. LoadTextureFromImage() — uploads the pixels to a GPU texture ONCE.
   *                    Creates the OpenGL texture object.
   *   3. Every frame: UpdateTexture() — re-uploads the changed pixels.
   *                    Only the raw pixel data changes; the texture handle stays.
   *   4. DrawTexture() — tells the GPU to blit the texture to screen.
   *
   * JS analogy: think of Image as an ImageData object, Texture2D as a
   * canvas that's been drawn once with `ctx.putImageData`, and UpdateTexture
   * as calling `ctx.putImageData` again each frame.
   */
  Image img = {
      .data    = props->backbuffer.pixels,
      .width   = props->backbuffer.width,
      .height  = props->backbuffer.height,
      .mipmaps = 1,                              /* no mipmaps — pixel-art style */
      .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, /* 4 bytes per pixel: RGBA */
  };
  g_raylib.texture = LoadTextureFromImage(img);
  /* NOTE: img.data still points to our backbuffer — we own that memory.
   * Raylib copies the pixels into GPU memory; our CPU buffer remains valid. */

  platform_audio_init(&props->audio);

  return 0;
}

void platform_shutdown(void) {
  platform_audio_shutdown();
  UnloadTexture(g_raylib.texture); /* free GPU texture memory */
  CloseWindow();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  /* Handmade Hero principle: The platform layer is a thin translator.
   *
   * Everything in main() is platform glue.  Notice that:
   *   - We initialise the platform first, then the game.
   *   - The game never touches any Raylib symbols.
   *   - The loop order is always: input → update → audio → render → display.
   */

  /*
   * PlatformGameProps bundles window size, backbuffer, and audio config.
   *
   * JS: like `const props = platformGamePropsInit()` — a factory function
   *     that heap-allocates the backbuffer and sets sane defaults.
   *
   * C:  `= {0}` zero-initialises the struct on the stack.
   *     platform_game_props_init() then fills in the real values.
   */
  PlatformGameProps platform_game_props = {0};
  if (platform_game_props_init(&platform_game_props) != 0) {
    /* platform_game_props_init prints its own error message */
    return 1;
  }

  if (platform_init(&platform_game_props) != 0) {
    return 1;
  }

  /*
   * Double-buffer pattern for input
   * ─────────────────────────────────
   *
   *  JS: like double-buffering a canvas — you write to the back while front is displayed
   *
   * inputs[0] = "current frame" button states
   * inputs[1] = "previous frame" button states
   *
   * We swap the pointers each frame so that the game always has access
   * to both "what is pressed NOW" and "what was pressed LAST frame",
   * which is how we detect rising edges (just-pressed) and falling edges
   * (just-released).
   *
   * C: `GameInput inputs[2]` declares an array of 2 structs ON THE STACK.
   *    JS: roughly `const inputs = [new GameInput(), new GameInput()]`
   *    `&inputs[0]` is a pointer to the first element.
   *    JS: roughly `inputs[0]` (JS refs are already pointer-like)
   */
  GameInput inputs[2]        = {0};
  GameInput *current_input   = &inputs[0]; /* points INTO the array */
  GameInput *old_input       = &inputs[1];

  /*
   * GameState lives on the stack.  It holds the entire Tetris simulation:
   * the field grid, the current piece, score, audio state, etc.
   *
   * JS: like `const gameState = gameInit()`, but in C the struct is declared
   *     in this scope and game_init() receives a pointer to it.
   */
  GameState game_state = {0};
  game_init(&game_state);

  game_audio_init(&game_state.audio, platform_game_props.audio.samples_per_second);
  game_music_play(&game_state.audio);

  /* ─── Main Loop ─────────────────────────────────────────────────────── */
  while (!WindowShouldClose()) {
    /*
     * delta_time: seconds elapsed since the previous frame.
     *
     * Raylib tracks this internally; GetFrameTime() returns it.
     * At 60 fps it's ~0.0167 seconds (16.7 ms).
     *
     * JS: like `const deltaTime = (performance.now() - lastTime) / 1000`
     */
    float delta_time = GetFrameTime();

    /* ══ Input ═══════════════════════════════════════════════════════════
     *
     * prepare_input_frame MUST come BEFORE reading new input.
     *
     * What it does:
     *   - Copies current_input → old_input  (save last frame's state)
     *   - Resets transition counters in current_input to 0
     *
     * WHY BEFORE?  If we read keys first, we'd immediately overwrite the
     * data we're trying to save.  Order matters.
     *
     * JS analogy: it's like doing
     *   `prevKeys = { ...keys }; keys = {}` BEFORE polling the keyboard,
     *   not after.
     */
    prepare_input_frame(old_input, current_input);

    /* Quit keys — checked before UPDATE_BUTTON to exit immediately */
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
      game_state.should_quit = 1;
      break;
    }

    game_state.should_restart = IsKeyPressed(KEY_R);

    /*
     * Rotation — edge-triggered (single press, not held)
     *
     * IsKeyPressed() returns true only on the FIRST frame the key is down,
     * unlike IsKeyDown() which is true every frame it's held.
     *
     * We also store which direction to rotate in next_rotate_x_value so
     * the game update function knows whether to spin left or right.
     */
    if (IsKeyPressed(KEY_X)) {
      UPDATE_BUTTON(current_input->rotate_x, 1);
      game_state.current_piece.next_rotate_x_value = TETROMINO_ROTATE_X_GO_RIGHT;
    } else if (IsKeyPressed(KEY_Z)) {
      UPDATE_BUTTON(current_input->rotate_x, 1);
      game_state.current_piece.next_rotate_x_value = TETROMINO_ROTATE_X_GO_LEFT;
    } else if (IsKeyReleased(KEY_X) || IsKeyReleased(KEY_Z)) {
      UPDATE_BUTTON(current_input->rotate_x, 0);
      game_state.current_piece.next_rotate_x_value = TETROMINO_ROTATE_X_NONE;
    }

    /*
     * Movement — level-triggered (held = continuously active)
     *
     * UPDATE_BUTTON(button, is_down)
     * ───────────────────────────────
     *  Raylib polling model vs X11 event model:
     *  ─────────────────────────────────────────
     *  Raylib: IsKeyDown() returns the CURRENT state right now.  There are
     *          no events; we poll on every frame.  UPDATE_BUTTON translates
     *          the boolean state into our transition-counting GameButtonState.
     *
     *  X11:    We receive KeyPress / KeyRelease events in a queue.  Each
     *          event fires UPDATE_BUTTON once.  Between events the state is
     *          unchanged.  This is the classic OS event model.
     *
     *  Both models converge on the same GameButtonState representation:
     *    ended_down           = is the button held right now?
     *    half_transition_count = how many times did it change state this frame?
     *
     *  JS: think of Raylib's model as reading `e.key` from a `keydown`
     *      event listener that fires every animation frame, versus X11's
     *      model which queues discrete events like DOM's `addEventListener`.
     */
    UPDATE_BUTTON(current_input->move_left,
                  IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    UPDATE_BUTTON(current_input->move_right,
                  IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
    UPDATE_BUTTON(current_input->move_down,
                  IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));

    /* ══ Update ══════════════════════════════════════════════════════════ */
    game_update(&game_state, current_input, delta_time);

    /*
     * Audio — fill any Raylib buffers that have been consumed since last
     * frame.  Must run AFTER game_update so the game's audio state (queued
     * sounds, music position) is current before we generate PCM samples.
     */
    platform_audio_update(&game_state, &platform_game_props.audio);

    /* ══ Render ══════════════════════════════════════════════════════════
     *
     * game_render writes pixels into platform_game_props.backbuffer.pixels
     * (a CPU-side uint32_t array).  This is "software rendering": no GPU
     * calls happen inside game_render at all.
     */
    game_render(&platform_game_props.backbuffer, &game_state);

    /* ══ Display ═════════════════════════════════════════════════════════
     *
     * UpdateTexture re-uploads the CPU pixel array to the GPU texture.
     * This is the "software renderer → GPU" bridge each frame.
     *
     * Cost: one DMA transfer of (width × height × 4) bytes per frame.
     * At 320×240 that's ~300 KB — trivial on modern hardware.
     */
    UpdateTexture(g_raylib.texture, platform_game_props.backbuffer.pixels);

    BeginDrawing();
    ClearBackground(BLACK); /* fill letterbox areas with black */

    /*
     * Handle window resize.
     *
     * When the user drags the window edge, GetScreenWidth/Height return the
     * new dimensions.  We update our props so offset calculations below are
     * correct.  The backbuffer itself never changes size — we just letterbox.
     *
     * COURSE NOTE: Removed debug printf from reference - not needed in final code
     */
    if (IsWindowResized()) {
      platform_game_props.width  = GetScreenWidth();
      platform_game_props.height = GetScreenHeight();
    }

    /*
     * Centre the backbuffer inside the (possibly resized) window.
     *
     * offset_x = (window_width  - backbuffer_width)  / 2
     * offset_y = (window_height - backbuffer_height) / 2
     *
     * If the window is smaller than the backbuffer the offset goes negative,
     * which crops the image — a simple, correct fallback.
     *
     * JS: like `canvas.style.marginLeft = (windowW - gameW) / 2 + 'px'`
     */
    int offset_x =
        (platform_game_props.width - platform_game_props.backbuffer.width) / 2;
    int offset_y =
        (platform_game_props.height - platform_game_props.backbuffer.height) / 2;

    DrawTexture(g_raylib.texture, offset_x, offset_y, WHITE);
    /* WHITE tint = "draw the texture as-is"; any other color multiplies RGBA */

    EndDrawing(); /* presents the frame (swaps buffers, VSync if enabled) */

    /*
     * Swap input buffers — current becomes old, old becomes current.
     * Next iteration's prepare_input_frame will reset the new "current".
     *
     * JS: like `[prevInput, input] = [input, prevInput]` at frame end.
     */
    platform_swap_input_buffers(old_input, current_input);
  }

  platform_game_props_free(&platform_game_props); /* free backbuffer pixels */
  platform_shutdown();                            /* free GPU texture, close audio */
  return 0;
}
