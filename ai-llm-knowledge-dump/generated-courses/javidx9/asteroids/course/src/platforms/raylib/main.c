/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/raylib/main.c — Raylib Backend for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * This backend uses Raylib for windowing, input, rendering, and audio.
 * It implements the platform/game contract from platform.h.
 *
 * KEY DIFFERENCES FROM X11 BACKEND
 * ──────────────────────────────────
 * Raylib:  ~120 lines.  Raylib handles all the heavy lifting.
 * X11:     ~250 lines.  Manual Xlib + GLX + ALSA.
 * Same game logic in both; radically different implementation complexity.
 *
 * AUDIO MODEL
 * ───────────
 * Float32 AudioStream (bitsPerSample=32) feeds Raylib's miniaudio backend.
 * Two-loop architecture:
 *   - Game loop (~60 Hz):  game_audio_update(), then process_audio() drains
 *     any ready buffers via while(IsAudioStreamProcessed).
 *   - Audio synthesis:     game_get_audio_samples() fills each buffer.
 *
 * Pre-fill: two silent buffers pushed on init so the stream starts
 * immediately without a pop.
 *
 * SCALE MODE
 * ──────────
 * TAB cycles: FIXED (letterbox) → DYNAMIC_MATCH → DYNAMIC_ASPECT.
 * FIXED mode keeps the backbuffer at GAME_W×GAME_H; the GPU quad scales it.
 * DYNAMIC modes resize the backbuffer to match the window, then recreate
 * the GPU texture.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../game/base.h"
#include "../../game/game.h"
#include "../../platform.h"
#include "../../utils/backbuffer.h"

/* ── Raylib-specific state ─────────────────────────────────────────────── */
typedef struct {
  Texture2D   texture;
  AudioStream audio_stream;
  int         buffer_size_frames;
  int         audio_initialized;
  int         prev_win_w;
  int         prev_win_h;
} RaylibState;

static RaylibState g_rl = {0};

/* ── Audio ─────────────────────────────────────────────────────────────── */
static int init_audio(PlatformAudioConfig *cfg, AudioOutputBuffer *audio_buf) {
  InitAudioDevice();
  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "⚠ Raylib: Audio device not ready\n");
    return -1;
  }

  /* SetAudioStreamBufferSizeDefault MUST be called before LoadAudioStream. */
  SetAudioStreamBufferSizeDefault(cfg->chunk_size);

  /* bitsPerSample=32 → float32 format matching our AudioOutputBuffer.      */
  g_rl.audio_stream = LoadAudioStream((unsigned)cfg->sample_rate, 32,
                                      (unsigned)cfg->channels);
  g_rl.buffer_size_frames = cfg->chunk_size;

  if (!IsAudioStreamValid(g_rl.audio_stream)) {
    fprintf(stderr, "⚠ Raylib: Failed to create audio stream\n");
    CloseAudioDevice();
    return -1;
  }

  /* Pre-fill two silent buffers so the stream starts immediately. */
  memset(audio_buf->samples, 0,
         (size_t)cfg->chunk_size * AUDIO_BYTES_PER_FRAME);
  UpdateAudioStream(g_rl.audio_stream, audio_buf->samples, cfg->chunk_size);
  UpdateAudioStream(g_rl.audio_stream, audio_buf->samples, cfg->chunk_size);

  PlayAudioStream(g_rl.audio_stream);
  cfg->raylib_stream = &g_rl.audio_stream;
  g_rl.audio_initialized = 1;
  return 0;
}

static void shutdown_audio(void) {
  if (g_rl.audio_initialized) {
    StopAudioStream(g_rl.audio_stream);
    UnloadAudioStream(g_rl.audio_stream);
    CloseAudioDevice();
    g_rl.audio_initialized = 0;
  }
}

/* Drain any ready audio buffers (while loop: consume ALL this frame).      */
static void process_audio(GameAudioState *audio, AudioOutputBuffer *buf) {
  if (!g_rl.audio_initialized) return;

  if (!IsAudioStreamPlaying(g_rl.audio_stream))
    PlayAudioStream(g_rl.audio_stream);

  while (IsAudioStreamProcessed(g_rl.audio_stream)) {
    int frames = g_rl.buffer_size_frames;
    if (frames > buf->max_sample_count) frames = buf->max_sample_count;
    game_get_audio_samples(audio, buf, frames);
    UpdateAudioStream(g_rl.audio_stream, buf->samples, frames);
  }
}

/* ── Scale mode ────────────────────────────────────────────────────────── */
static void apply_scale_mode(PlatformGameProps *props, int win_w, int win_h) {
  int new_w, new_h;
  switch (props->scale_mode) {
  case WINDOW_SCALE_MODE_FIXED:
    new_w = GAME_W; new_h = GAME_H; break;
  case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
    new_w = win_w;  new_h = win_h;  break;
  case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
    platform_compute_aspect_size(win_w, win_h, 4.0f, 3.0f, &new_w, &new_h);
    break;
  default:
    new_w = GAME_W; new_h = GAME_H; break;
  }

  if (new_w != props->backbuffer.width || new_h != props->backbuffer.height) {
    backbuffer_resize(&props->backbuffer, new_w, new_h);
    UnloadTexture(g_rl.texture);
    Image img = {
        .data    = props->backbuffer.pixels,
        .width   = new_w, .height = new_h,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_rl.texture = LoadTextureFromImage(img);
    SetTextureFilter(g_rl.texture, TEXTURE_FILTER_POINT);
  }
}

/* ── Display ────────────────────────────────────────────────────────────── */
static void display_backbuffer(Backbuffer *bb, WindowScaleMode mode) {
  UpdateTexture(g_rl.texture, bb->pixels);

  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();

  float off_x, off_y, dst_w, dst_h;
  if (mode == WINDOW_SCALE_MODE_FIXED) {
    float scale = 0;
    int iox = 0, ioy = 0;
    platform_compute_letterbox(win_w, win_h, bb->width, bb->height,
                               &scale, &iox, &ioy);
    dst_w = (float)bb->width  * scale;
    dst_h = (float)bb->height * scale;
    off_x = (float)iox;
    off_y = (float)ioy;
  } else {
    dst_w = (float)bb->width;
    dst_h = (float)bb->height;
    off_x = ((float)win_w - dst_w) * 0.5f;
    off_y = ((float)win_h - dst_h) * 0.5f;
  }

  Rectangle src  = {0.0f, 0.0f, (float)bb->width, (float)bb->height};
  Rectangle dest = {off_x, off_y, dst_w, dst_h};

  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexturePro(g_rl.texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
  EndDrawing();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */
int main(void) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(GAME_W, GAME_H, TITLE);
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(TARGET_FPS);

  if (!IsWindowReady()) {
    fprintf(stderr, "❌ Raylib: InitWindow failed\n");
    return 1;
  }

  /* ── Allocate shared resources ────────────────────────────────────────── */
  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    fprintf(stderr, "❌ Out of memory\n");
    CloseWindow();
    return 1;
  }

  /* GPU texture */
  {
    Image img = {
        .data    = props.backbuffer.pixels,
        .width   = GAME_W, .height = GAME_H,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_rl.texture = LoadTextureFromImage(img);
    SetTextureFilter(g_rl.texture, TEXTURE_FILTER_POINT);
  }

  if (!IsTextureValid(g_rl.texture)) {
    fprintf(stderr, "❌ Raylib: LoadTextureFromImage failed\n");
    platform_game_props_free(&props);
    CloseWindow();
    return 1;
  }

  g_rl.prev_win_w = GetScreenWidth();
  g_rl.prev_win_h = GetScreenHeight();

  if (init_audio(&props.audio_config, &props.audio_buffer) != 0)
    fprintf(stderr, "⚠ Audio init failed — continuing without audio\n");

  /* ── Game state ───────────────────────────────────────────────────────── */
  GameState state;
  memset(&state, 0, sizeof(state));
  state.audio.samples_per_second = AUDIO_SAMPLE_RATE;
  game_audio_init(&state.audio);
  asteroids_init(&state);

  /* ── Input double buffer ──────────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr = &inputs[0];
  GameInput *prev = &inputs[1];

  /* ── Main loop ────────────────────────────────────────────────────────── */
  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.066f) dt = 0.066f;
    if (dt < 0.0f)   dt = 0.0f;

    /* ── Input ──────────────────────────────────────────────────────────── */
    platform_swap_input_buffers(&curr, &prev);
    prepare_input_frame(curr, prev);

    /* Asteroids controls */
    UPDATE_BUTTON(&curr->buttons.quit,             IsKeyDown(KEY_ESCAPE)  ? 1 : 0);
    UPDATE_BUTTON(&curr->buttons.cycle_scale_mode, IsKeyDown(KEY_TAB)     ? 1 : 0);
    UPDATE_BUTTON(&curr->buttons.thrust,
                  (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W))   ? 1 : 0);
    UPDATE_BUTTON(&curr->buttons.rotate_left,
                  (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A))   ? 1 : 0);
    UPDATE_BUTTON(&curr->buttons.rotate_right,
                  (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))   ? 1 : 0);
    UPDATE_BUTTON(&curr->buttons.fire,   IsKeyDown(KEY_SPACE)  ? 1 : 0);
    UPDATE_BUTTON(&curr->buttons.restart,
                  (IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_R))   ? 1 : 0);

    if (curr->buttons.quit.ended_down) break;

    /* TAB: cycle scale mode */
    if (button_just_pressed(curr->buttons.cycle_scale_mode)) {
      props.scale_mode = (WindowScaleMode)(
          (props.scale_mode + 1) % WINDOW_SCALE_MODE_COUNT);
    }

    /* Window resize */
    if (IsWindowResized()) {
      apply_scale_mode(&props, GetScreenWidth(), GetScreenHeight());
    }

    /* ── Game update ────────────────────────────────────────────────────── */
    asteroids_update(&state, curr, dt);

    /* ── Audio update + drain ───────────────────────────────────────────── */
    float dt_ms = GetFrameTime() * 1000.0f;
    game_audio_update(&state.audio, dt_ms);
    process_audio(&state.audio, &props.audio_buffer);

    /* ── Render ─────────────────────────────────────────────────────────── */
    asteroids_render(&state, &props.backbuffer, props.world_config);
    display_backbuffer(&props.backbuffer, props.scale_mode);
  }

  /* ── Cleanup ─────────────────────────────────────────────────────────── */
  shutdown_audio();
  if (IsTextureValid(g_rl.texture)) UnloadTexture(g_rl.texture);
  platform_game_props_free(&props);
  CloseWindow();
  return 0;
}
