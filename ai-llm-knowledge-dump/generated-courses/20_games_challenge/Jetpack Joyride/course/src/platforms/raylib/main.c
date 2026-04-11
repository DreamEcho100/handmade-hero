/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/raylib/main.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Raylib backend: window, input, audio, backbuffer upload.
 * Adapted from Platform Foundation Course.
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

/* ── Raylib-specific state ──────────────────────────────────────────────── */
typedef struct {
  Texture2D texture;
  AudioStream audio_stream;
  int buffer_size_frames;
  int audio_initialized;
  int prev_win_w;
  int prev_win_h;
} RaylibState;

static RaylibState g_raylib = {0};

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio
 * ═══════════════════════════════════════════════════════════════════════════
 */
static int init_audio(PlatformAudioConfig *cfg, AudioOutputBuffer *audio_buf) {
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "Raylib: Audio device not ready\n");
    return -1;
  }

  SetAudioStreamBufferSizeDefault(cfg->chunk_size);

  g_raylib.audio_stream =
      LoadAudioStream((unsigned)cfg->sample_rate, 32, (unsigned)cfg->channels);
  g_raylib.buffer_size_frames = cfg->chunk_size;

  if (!IsAudioStreamValid(g_raylib.audio_stream)) {
    fprintf(stderr, "Raylib: Failed to create audio stream\n");
    CloseAudioDevice();
    return -1;
  }

  /* Pre-fill two silent buffers to prevent startup pop. */
  memset(audio_buf->samples, 0,
         (size_t)cfg->chunk_size * AUDIO_BYTES_PER_FRAME);
  UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, cfg->chunk_size);
  UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, cfg->chunk_size);

  PlayAudioStream(g_raylib.audio_stream);
  cfg->raylib_stream = &g_raylib.audio_stream;
  g_raylib.audio_initialized = 1;

  printf("Audio initialized (Raylib, %d Hz, %d-frame buffer)\n",
         cfg->sample_rate, cfg->chunk_size);
  return 0;
}

static void shutdown_audio(void) {
  if (g_raylib.audio_initialized) {
    StopAudioStream(g_raylib.audio_stream);
    UnloadAudioStream(g_raylib.audio_stream);
    CloseAudioDevice();
    g_raylib.audio_initialized = 0;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Scale mode
 * ═══════════════════════════════════════════════════════════════════════════
 */
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
    /* 16:9 aspect for 320x180 */
    platform_compute_aspect_size(win_w, win_h, 16.0f, 9.0f, &new_w, &new_h);
    break;
  default:
    new_w = GAME_W;
    new_h = GAME_H;
    break;
  }

  if (new_w != props->backbuffer.width || new_h != props->backbuffer.height) {
    backbuffer_resize(&props->backbuffer, new_w, new_h);

    UnloadTexture(g_raylib.texture);
    Image img = {
        .data    = props->backbuffer.pixels,
        .width   = new_w,
        .height  = new_h,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_raylib.texture = LoadTextureFromImage(img);
    SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Display
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void display_backbuffer(Backbuffer *backbuffer,
                               WindowScaleMode scale_mode) {
  UpdateTexture(g_raylib.texture, backbuffer->pixels);

  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();

  float off_x, off_y, dst_w, dst_h;

  if (scale_mode == WINDOW_SCALE_MODE_FIXED) {
    float scale = 0;
    int ioff_x = 0, ioff_y = 0;
    platform_compute_letterbox(win_w, win_h, backbuffer->width,
                               backbuffer->height, &scale, &ioff_x, &ioff_y);
    dst_w = (float)backbuffer->width  * scale;
    dst_h = (float)backbuffer->height * scale;
    off_x = (float)ioff_x;
    off_y = (float)ioff_y;
  } else {
    dst_w = (float)backbuffer->width;
    dst_h = (float)backbuffer->height;
    off_x = ((float)win_w - dst_w) * 0.5f;
    off_y = ((float)win_h - dst_h) * 0.5f;
  }

  Rectangle src  = {0.0f, 0.0f, (float)backbuffer->width,
                    (float)backbuffer->height};
  Rectangle dest = {off_x, off_y, dst_w, dst_h};

  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexturePro(g_raylib.texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
  EndDrawing();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio drain
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void process_audio(GameState *game_state, AudioOutputBuffer *audio_buf) {
  if (!g_raylib.audio_initialized)
    return;

  if (!IsAudioStreamPlaying(g_raylib.audio_stream))
    PlayAudioStream(g_raylib.audio_stream);

  while (IsAudioStreamProcessed(g_raylib.audio_stream)) {
    int frames = g_raylib.buffer_size_frames;
    if (frames > audio_buf->max_sample_count)
      frames = audio_buf->max_sample_count;

    game_get_audio_samples(game_state, audio_buf, frames);
    UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, frames);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */
int main(void) {
  /* Create window at 3x native resolution (960x540). */
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(GAME_W * 3, GAME_H * 3, TITLE);
  SetTargetFPS(TARGET_FPS);

  if (!IsWindowReady()) {
    fprintf(stderr, "Raylib: InitWindow failed\n");
    return 1;
  }

  SetWindowState(FLAG_WINDOW_RESIZABLE);

  /* ── Shared resource allocation ─────────────────────────────────────── */
  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    fprintf(stderr, "Out of memory\n");
    CloseWindow();
    return 1;
  }

  /* Create GPU texture from backbuffer. */
  Image img = {
      .data    = props.backbuffer.pixels,
      .width   = GAME_W,
      .height  = GAME_H,
      .mipmaps = 1,
      .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  g_raylib.texture = LoadTextureFromImage(img);
  SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);

  if (!IsTextureValid(g_raylib.texture)) {
    fprintf(stderr, "Raylib: LoadTextureFromImage failed\n");
    platform_game_props_free(&props);
    CloseWindow();
    return 1;
  }

  g_raylib.prev_win_w = GetScreenWidth();
  g_raylib.prev_win_h = GetScreenHeight();

  if (init_audio(&props.audio_config, &props.audio_buffer) != 0) {
    fprintf(stderr, "Audio init failed -- continuing without audio\n");
  }

  /* ── Game state ─────────────────────────────────────────────────────── */
  GameState game_state = {0};
  game_init(&game_state, &props);

  /* Audio loads on background pthread — game starts immediately */

  /* ── Input double buffer ────────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  printf("Platform initialized (Raylib backend)\n");

  /* ── Main loop ──────────────────────────────────────────────────────── */
  while (!WindowShouldClose() && game_state.is_running) {
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* Poll input */
    UPDATE_BUTTON(&curr_input->buttons.action,
                  (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.quit,
                  IsKeyDown(KEY_ESCAPE) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.pause,
                  IsKeyDown(KEY_P) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.restart,
                  IsKeyDown(KEY_R) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.left,
                  (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) ? 1 : 0);
    UPDATE_BUTTON(&curr_input->buttons.right,
                  (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) ? 1 : 0);

    /* Mouse position */
    Vector2 mouse = GetMousePosition();
    curr_input->mouse_x = mouse.x;
    curr_input->mouse_y = mouse.y;
    curr_input->mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1 : 0;

    if (curr_input->buttons.quit.ended_down)
      break;

    /* React to window resize */
    if (IsWindowResized()) {
      int win_w = GetScreenWidth();
      int win_h = GetScreenHeight();
      g_raylib.prev_win_w = win_w;
      g_raylib.prev_win_h = win_h;
      apply_scale_mode(&props, win_w, win_h);
    }

    /* Update game */
    float dt = GetFrameTime();
    if (dt > 0.05f) dt = 0.05f; /* Cap to prevent spiral of death */

    /* Sync camera from game state to world config */
    props.world_config.camera_x = game_state.camera.x;
    props.world_config.camera_y = game_state.camera.y;
    props.world_config.camera_zoom = game_state.camera.zoom;

    game_update(&game_state, curr_input, dt);

    /* Audio */
    process_audio(&game_state, &props.audio_buffer);

    /* Render */
    TempMemory frame_scratch = arena_begin_temp(&props.scratch);
    game_render(&game_state, &props.backbuffer, props.world_config,
                &props.scratch);
    arena_end_temp(frame_scratch);
    arena_check(&props.scratch);

    display_backbuffer(&props.backbuffer, props.scale_mode);

#ifdef BENCH_DURATION_S
    if (GetTime() >= (double)BENCH_DURATION_S) {
      printf("\n[bench] %.0f seconds elapsed\n", GetTime());
      perf_print_all();
      break;
    }
#endif
  }

  /* ── Cleanup ────────────────────────────────────────────────────────── */
  game_cleanup(&game_state);
  shutdown_audio();
  if (IsTextureValid(g_raylib.texture))
    UnloadTexture(g_raylib.texture);
  platform_game_props_free(&props);
  CloseWindow();
  return 0;
}
