/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/raylib/main.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 01 — InitWindow, SetTargetFPS, main loop shell.
 *             First window on screen.
 *
 * LESSON 03 — LoadTextureFromImage with PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
 *             UpdateTexture, DrawTexture. First pixel on screen.
 *             R↔B channel swap: our 0xAARRGGBB matches R8G8B8A8 directly.
 *             Both Raylib (PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) and X11 (GL_RGBA)
 *             use the same [R,G,B,A] byte order — no swap needed on either.
 *
 * LESSON 07 — IsKeyPressed / IsKeyReleased with UPDATE_BUTTON.
 *             Double-buffered input via platform_swap_input_buffers.
 *
 * LESSON 08 — GetFrameTime(), letterbox via DrawTexturePro + dest Rectangle.
 *             FPS counter via GetFPS() fed into demo_render.
 *             DE100 frame timing note: Raylib's SetTargetFPS handles the
 *             sleep internally; we replicate the timing pattern for pedagogy
 *             but do not override Raylib's scheduler.
 *
 * LESSON 09 — WindowScaleMode: apply_scale_mode() resizes backbuffer on
 *             window resize; display_backbuffer() adapts to each mode.
 *             TAB key cycles FIXED → DYNAMIC_MATCH → DYNAMIC_ASPECT.
 *
 * LESSON 11 — Full audio: pre-fill 2 silent buffers,
 * while-IsAudioStreamProcessed drain loop, UpdateAudioStream.
 *
 * LESSON 15 — game/demo.c replaced by game/main.c.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../game/base.h"
#include "../../game/main.h"
#include "../../platform.h"
#include "../../utils/backbuffer.h"
#include "../../utils/perf.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Raylib-specific state
 * ─────────────────────────────────────────────────────────────────────────
 * LESSON 09 — prev_win_w/h track last known window size so we only call
 * apply_scale_mode() when the window actually resizes (IsWindowResized).   */
typedef struct {
  Texture2D texture;
  AudioStream audio_stream;
  int buffer_size_frames;
  int audio_initialized;
  int window_focused;
  int audio_paused;
  int prev_win_w;
  int prev_win_h;
} RaylibState;

static RaylibState g_raylib = {0};

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio (LESSON 09 minimal → LESSON 11 full)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 09 — Minimal path students write first:
 *   InitAudioDevice();
 *   SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);
 *   g_raylib.audio_stream = LoadAudioStream(AUDIO_SAMPLE_RATE, 32, 2);
 *   PlayAudioStream(g_raylib.audio_stream);
 *   // → first audible tone on space press
 *
 * LESSON 11 — Full path (what this file contains): adds pre-fill of two
 *   silent buffers and the while-IsAudioStreamProcessed drain loop.
 *   The contrast between L09 (simple) and L11 (robust) is the teaching moment.
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int init_audio(PlatformAudioConfig *cfg, AudioOutputBuffer *audio_buf) {
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "⚠ Raylib: Audio device not ready\n");
    return -1;
  }

  /* LESSON 11 — SetAudioStreamBufferSizeDefault must be called BEFORE
   * LoadAudioStream.  The default (4096) matches our AUDIO_CHUNK_SIZE.   */
  SetAudioStreamBufferSizeDefault(cfg->chunk_size);

  g_raylib.audio_stream =
      LoadAudioStream((unsigned)cfg->sample_rate, 32, (unsigned)cfg->channels);
  g_raylib.buffer_size_frames = cfg->chunk_size;

  if (!IsAudioStreamValid(g_raylib.audio_stream)) {
    fprintf(stderr, "⚠ Raylib: Failed to create audio stream\n");
    CloseAudioDevice();
    return -1;
  }

  /* LESSON 11 — Pre-fill two silent buffers.
   * Raylib uses a double-buffer internally; pre-filling both ensures the
   * stream starts playing immediately without a pop.                      */
  {
    int bps = audio_format_bytes_per_sample(audio_buf->format);
    memset(audio_buf->samples_buffer, 0, (size_t)cfg->chunk_size * (size_t)bps);
  }
  UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples_buffer,
                    cfg->chunk_size);
  UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples_buffer,
                    cfg->chunk_size);

  PlayAudioStream(g_raylib.audio_stream);

  /* Store stream pointer in cfg for shutdown (opaque void*). */
  cfg->raylib_stream = &g_raylib.audio_stream;
  g_raylib.audio_initialized = 1;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 RAYLIB AUDIO\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:  %d Hz\n", cfg->sample_rate);
  printf("  Buffer:       %d frames (~%.0f ms)\n", cfg->chunk_size,
         (float)cfg->chunk_size / (float)cfg->sample_rate * 1000.0f);
  printf("✓ Audio initialized\n");
  printf("═══════════════════════════════════════════════════════════\n\n");
  return 0;
}

static void shutdown_audio(void) {
  if (g_raylib.audio_initialized) {
    StopAudioStream(g_raylib.audio_stream);
    UnloadAudioStream(g_raylib.audio_stream);
    CloseAudioDevice();
    g_raylib.audio_initialized = 0;
    g_raylib.audio_paused = 0;
  }
}

static void handle_focus_change(void) {
  int is_focused;

  if (!g_raylib.audio_initialized)
    return;

  is_focused = IsWindowFocused() ? 1 : 0;
  if (is_focused == g_raylib.window_focused)
    return;

  g_raylib.window_focused = is_focused;
  if (is_focused) {
    ResumeAudioStream(g_raylib.audio_stream);
    g_raylib.audio_paused = 0;
    printf("[focus] Raylib gained focus — audio resumed\n");
  } else {
    PauseAudioStream(g_raylib.audio_stream);
    g_raylib.audio_paused = 1;
    printf("[focus] Raylib lost focus — audio paused\n");
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Scale mode (LESSON 09)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * apply_scale_mode — called when the window is resized (IsWindowResized) or
 * when the user cycles the scale mode (TAB).
 *
 * In FIXED mode: backbuffer stays 800×600; GPU letterbox handles scaling.
 * In DYNAMIC modes: backbuffer is resized to match the new window size, then
 * the GPU texture is recreated at the new resolution.
 *
 * GPU texture recreate: Raylib's Texture2D can't be resized in-place —
 * UnloadTexture + LoadTextureFromImage is the correct pattern.             */
static void apply_scale_mode(PlatformGameProps *props, int win_w, int win_h) {
  int new_w, new_h;

  switch (props->scale_mode) {
  case WINDOW_SCALE_MODE_FIXED:
    new_w = GAME_W;
    new_h = GAME_H;
    break;
  case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
    new_w = win_w;
    new_h = win_h;
    break;
  case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
    platform_compute_aspect_size(win_w, win_h, 4.0f, 3.0f, &new_w, &new_h);
    break;
  default:
    new_w = GAME_W;
    new_h = GAME_H;
    break;
  }

  if (new_w != props->backbuffer.width || new_h != props->backbuffer.height) {
    backbuffer_resize(&props->backbuffer, new_w, new_h);

    /* Recreate GPU texture at new dimensions. */
    UnloadTexture(g_raylib.texture);
    Image img = {
        .data = props->backbuffer.pixels,
        .width = new_w,
        .height = new_h,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_raylib.texture = LoadTextureFromImage(img);
    SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Display (LESSON 03, LESSON 08, LESSON 09)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * FIXED mode:   letterbox the backbuffer into the window (black bars).
 * DYNAMIC modes: backbuffer already matches the window; center at 1:1 scale.
 *
 * JS analogy: CSS `object-fit: contain` (FIXED) vs `object-fit: fill`
 * (DYNAMIC). */
static void display_backbuffer(Backbuffer *backbuffer,
                               WindowScaleMode scale_mode) {
  /* LESSON 03 — Upload CPU pixels → GPU texture. */
  UpdateTexture(g_raylib.texture, backbuffer->pixels);

  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();

  float off_x, off_y, dst_w, dst_h;

  if (scale_mode == WINDOW_SCALE_MODE_FIXED) {
    /* LESSON 08 — GPU letterbox scaling */
    float scale = 0;
    int ioff_x = 0, ioff_y = 0;
    platform_compute_letterbox(win_w, win_h, backbuffer->width,
                               backbuffer->height, &scale, &ioff_x, &ioff_y);
    dst_w = (float)backbuffer->width * scale;
    dst_h = (float)backbuffer->height * scale;
    off_x = (float)ioff_x;
    off_y = (float)ioff_y;
  } else {
    /* LESSON 09 — Dynamic modes: backbuffer already sized to fit; center 1:1.
     */
    dst_w = (float)backbuffer->width;
    dst_h = (float)backbuffer->height;
    off_x = ((float)win_w - dst_w) * 0.5f;
    off_y = ((float)win_h - dst_h) * 0.5f;
  }

  Rectangle src = {0.0f, 0.0f, (float)backbuffer->width,
                   (float)backbuffer->height};
  Rectangle dest = {off_x, off_y, dst_w, dst_h};

  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexturePro(g_raylib.texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
  EndDrawing();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio drain (LESSON 11)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 11 — while loop: consume ALL processed buffers this frame.
 * IsAudioStreamProcessed returns true when Raylib's internal buffer needs
 * refilling.  Using `while` (not `if`) prevents the stream from draining
 * to silence if two buffers become ready in the same frame.              */
static void process_audio(GameAppState *game, AudioOutputBuffer *audio_buf) {
  if (!g_raylib.audio_initialized || g_raylib.audio_paused)
    return;

  if (!IsAudioStreamPlaying(g_raylib.audio_stream))
    PlayAudioStream(g_raylib.audio_stream);

  while (IsAudioStreamProcessed(g_raylib.audio_stream)) {
    int frames = g_raylib.buffer_size_frames;
    if (frames > audio_buf->max_sample_count)
      frames = audio_buf->max_sample_count;

    game_app_get_audio_samples(game, audio_buf, frames);
    UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples_buffer, frames);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  /* LESSON 01 — Create window and set target FPS.
   * SetTargetFPS(60) causes Raylib's EndDrawing() to sleep internally —
   * we don't need a manual sleep loop for Raylib.                        */
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(GAME_W, GAME_H, TITLE);
  SetTargetFPS(TARGET_FPS);

  if (!IsWindowReady()) {
    fprintf(stderr, "❌ Raylib: InitWindow failed\n");
    return 1;
  }

  /* Allow window resizing (letterbox handles the rest). */
  SetWindowState(FLAG_WINDOW_RESIZABLE);

  /* ── Shared resource allocation (backbuffer + audio) ─────────────── */
  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    fprintf(stderr, "❌ Out of memory\n");
    CloseWindow();
    return 1;
  }

  /* LESSON 03 — Create GPU texture from the backbuffer.
   * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 — matches our [R,G,B,A] memory layout.
   * Both Raylib and X11 use the same [R,G,B,A] format; no R↔B swap needed.  */
  Image img = {
      .data = props.backbuffer.pixels,
      .width = GAME_W,
      .height = GAME_H,
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  g_raylib.texture = LoadTextureFromImage(img);
  SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);

  if (!IsTextureValid(g_raylib.texture)) {
    fprintf(stderr, "❌ Raylib: LoadTextureFromImage failed\n");
    platform_game_props_free(&props);
    CloseWindow();
    return 1;
  }

  /* LESSON 09 — record initial window size for resize detection. */
  g_raylib.prev_win_w = GetScreenWidth();
  g_raylib.prev_win_h = GetScreenHeight();
  g_raylib.window_focused = 1;
  g_raylib.audio_paused = 0;

  if (init_audio(&props.audio_config, &props.audio_buffer) != 0) {
    fprintf(stderr, "⚠ Audio init failed — continuing without audio\n");
  }

  /* ── Game state ────────────────────────────────────────────────────── */
  GameAppState *game = NULL;
  if (game_app_init(&game, &props.perm) != 0) {
    fprintf(stderr, "❌ Failed to initialize game facade\n");
    shutdown_audio();
    if (IsTextureValid(g_raylib.texture))
      UnloadTexture(g_raylib.texture);
    platform_game_props_free(&props);
    CloseWindow();
    return 1;
  }

  /* ── Input double buffer ───────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  printf("✓ Platform initialized (Raylib backend)\n");

  /* ── Main loop ─────────────────────────────────────────────────────── */
  while (!WindowShouldClose()) {
    /* LESSON 07 — Double-buffered input swap + prepare */
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* LESSON 07 — Poll Raylib input into GameInput */
    if (IsKeyDown(KEY_ESCAPE)) {
      UPDATE_BUTTON(&curr_input->buttons.quit, 1);
      break;
    }
    UPDATE_BUTTON(&curr_input->buttons.play_tone, IsKeyDown(KEY_SPACE) ? 1 : 0);

    /* LESSON 09 — TAB key for scale mode cycling */
    UPDATE_BUTTON(&curr_input->buttons.cycle_scale_mode,
                  IsKeyDown(KEY_TAB) ? 1 : 0);

    /* LESSON 17 — Camera control keys */
    UPDATE_BUTTON(&curr_input->buttons.cam_up, IsKeyDown(KEY_UP) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.cam_down, IsKeyDown(KEY_DOWN) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.cam_left, IsKeyDown(KEY_LEFT) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.cam_right, IsKeyDown(KEY_RIGHT) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.zoom_in, IsKeyDown(KEY_E) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.zoom_out, IsKeyDown(KEY_Q) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.cam_reset, IsKeyDown(KEY_C) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.prev_scene,
                  IsKeyDown(KEY_LEFT_BRACKET) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.next_scene,
                  IsKeyDown(KEY_RIGHT_BRACKET) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.reload_scene, IsKeyDown(KEY_R) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.toggle_debug_hud,
                  IsKeyDown(KEY_F1) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.toggle_runtime_override,
                  IsKeyDown(KEY_F2) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.clear_runtime_override,
                  IsKeyDown(KEY_F3) ? 1 : 0);

    if (curr_input->buttons.quit.ended_down)
      break;

    /* LESSON 09 — Cycle scale mode on TAB press */
    if (button_just_pressed(curr_input->buttons.cycle_scale_mode)) {
      props.scale_mode =
          (WindowScaleMode)((props.scale_mode + 1) % WINDOW_SCALE_MODE_COUNT);
    }

    /* LESSON 09 — React to window resize */
    if (IsWindowResized()) {
      int win_w = GetScreenWidth();
      int win_h = GetScreenHeight();
      g_raylib.prev_win_w = win_w;
      g_raylib.prev_win_h = win_h;
      apply_scale_mode(&props, win_w, win_h);
    }

    handle_focus_change();

    PERF_BEGIN_NAMED(rl_game_update, "raylib/game_app_update");
    game_app_update(game, curr_input, GetFrameTime(), &props.level);
    PERF_END(rl_game_update);

    /* LESSON 11 — Fill audio stream buffers */
    PERF_BEGIN_NAMED(rl_process_audio, "raylib/process_audio");
    process_audio(game, &props.audio_buffer);
    PERF_END(rl_process_audio);

    /* LESSON 03/08/09 — Render demo frame + display
     *
     * LESSON 16 — Wrap each frame in a TempMemory checkpoint.
     * All scratch allocations inside demo_render are freed by arena_end_temp.
     * arena_check asserts no orphaned arena_begin_temp calls remain.        */
    PERF_BEGIN_NAMED(rl_demo_render, "raylib/demo_render+display");
    TempMemory frame_scratch = arena_begin_temp(&props.scratch);
    game_app_render(game, &props.backbuffer, GetFPS(), props.scale_mode,
                    props.world_config, &props.perm, &props.level,
                    &props.scratch);
    arena_end_temp(frame_scratch);
    arena_check(&props.scratch);

    display_backbuffer(&props.backbuffer, props.scale_mode);
    PERF_END(rl_demo_render);

    /* LESSON 09 — BENCH_DURATION_S: auto-exit after N seconds and print
     * profiler summary.  Enabled by --bench=N in build-dev.sh.            */
#ifdef BENCH_DURATION_S
    if (GetTime() >= (double)BENCH_DURATION_S) {
      printf("\n[bench] %.0f seconds elapsed — printing profiler summary.\n",
             GetTime());
      perf_print_all();
      break;
    }
#endif
  }

  /* ── Cleanup ───────────────────────────────────────────────────────── */
  shutdown_audio();
  if (IsTextureValid(g_raylib.texture))
    UnloadTexture(g_raylib.texture);
  platform_game_props_free(&props);
  CloseWindow();
  return 0;
}
