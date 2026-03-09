#include "../../game/main.h"
#include "../../platform.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Raylib State
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  Texture2D texture;
  AudioStream audio_stream;
  int buffer_size_frames;
  bool audio_initialized;
} RaylibState;

static RaylibState g_raylib = {0};

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing
 * ═══════════════════════════════════════════════════════════════════════════
 */

double platform_get_time(void) { return GetTime(); }

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio
 * ═══════════════════════════════════════════════════════════════════════════
 */

/*
 * Raylib audio init only needs AudioOutputBuffer — no ALSA-style config.
 * Raylib's own audio thread handles buffering; we just push chunks when
 * IsAudioStreamProcessed() returns true.
 */
static int platform_audio_init(AudioOutputBuffer *audio_buffer) {
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "⚠ Raylib: Audio device not ready\n");
    audio_buffer->is_initialized = false;
    return 1;
  }

  /*
   * Buffer size: 4096 samples ≈ 85 ms at 48 kHz.
   *
   * Raylib's audio thread consumes at ~46.875 Hz (48000/1024), so with a
   * 60 Hz game loop the average is 0.78 buffers/frame.  4096 samples of
   * headroom means starvation requires ~5 missed frames — essentially never.
   */
  int buffer_size = 4096;
  g_raylib.buffer_size_frames = buffer_size;

  SetAudioStreamBufferSizeDefault(buffer_size);

  g_raylib.audio_stream =
      LoadAudioStream(audio_buffer->samples_per_second, 16, 2);

  if (!IsAudioStreamValid(g_raylib.audio_stream)) {
    fprintf(stderr, "⚠ Raylib: Failed to create audio stream\n");
    CloseAudioDevice();
    audio_buffer->is_initialized = false;
    return 1;
  }

  /* Pre-fill BOTH buffers with silence (Raylib double-buffers) */
  memset(audio_buffer->samples_buffer, 0, buffer_size * 2 * sizeof(int16_t));
  UpdateAudioStream(g_raylib.audio_stream, audio_buffer->samples_buffer,
                    buffer_size);
  UpdateAudioStream(g_raylib.audio_stream, audio_buffer->samples_buffer,
                    buffer_size);

  PlayAudioStream(g_raylib.audio_stream);

  audio_buffer->is_initialized = true;
  g_raylib.audio_initialized = true;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 RAYLIB AUDIO\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:     %d Hz\n", audio_buffer->samples_per_second);
  printf("  Buffer size:     %d samples (%.0f ms)\n", buffer_size,
         (float)buffer_size / audio_buffer->samples_per_second * 1000.0f);
  printf("✓ Audio initialized\n");
  printf("═══════════════════════════════════════════════════════════\n\n");

  return 0;
}

static void platform_audio_shutdown(void) {
  if (g_raylib.audio_initialized) {
    StopAudioStream(g_raylib.audio_stream);
    UnloadAudioStream(g_raylib.audio_stream);
    CloseAudioDevice();
    g_raylib.audio_initialized = false;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Init
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_init(PlatformGameProps *props) {
  SetTraceLogLevel(LOG_WARNING);

  InitWindow(props->width, props->height, props->title);
  SetTargetFPS(60);

  if (!IsWindowReady()) {
    fprintf(stderr, "❌ Raylib: Failed to create window\n");
    return 1;
  }

  Image img = {.data = props->game.backbuffer.pixels,
               .width = props->game.backbuffer.width,
               .height = props->game.backbuffer.height,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  g_raylib.texture = LoadTextureFromImage(img);

  if (!IsTextureValid(g_raylib.texture)) {
    fprintf(stderr, "❌ Raylib: Failed to create texture\n");
    CloseWindow();
    return 1;
  }

  if (platform_audio_init(&props->game.audio) != 0) {
    fprintf(stderr, "⚠ Audio init failed, continuing without audio\n");
  }

  printf("✓ Platform initialized (Raylib)\n");
  return 0;
}

void platform_shutdown(void) {
  platform_audio_shutdown();
  if (IsTextureValid(g_raylib.texture)) {
    UnloadTexture(g_raylib.texture);
  }
  CloseWindow();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input
 * ═══════════════════════════════════════════════════════════════════════════
 */

void platform_get_input(GameInput *input, PlatformGameProps *props) {
  if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE) || WindowShouldClose()) {
    props->is_running = false;
    input->quit = true;
  }

  if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_SPACE)) {
    input->restart = 1;
  }

  if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
    UPDATE_BUTTON(input->turn_left, 1);
  } else if (IsKeyReleased(KEY_LEFT) || IsKeyReleased(KEY_A)) {
    UPDATE_BUTTON(input->turn_left, 0);
  }

  if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
    UPDATE_BUTTON(input->turn_right, 1);
  } else if (IsKeyReleased(KEY_RIGHT) || IsKeyReleased(KEY_D)) {
    UPDATE_BUTTON(input->turn_right, 0);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Display
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void display_backbuffer(Backbuffer *bb) {
  UpdateTexture(g_raylib.texture, bb->pixels);
  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexture(g_raylib.texture, 0, 0, WHITE);
  EndDrawing();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Processing
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int g_raylib_audio_calls = 0;    /* DEBUG */
static int g_raylib_buffers_filled = 0; /* DEBUG */

static void process_audio(PlatformGameProps *props,
                          GameAudioState *game_audio) {
  if (!props->game.audio.is_initialized || !g_raylib.audio_initialized)
    return;

  if (!IsAudioStreamPlaying(g_raylib.audio_stream)) {
    printf("[RAYLIB_AUDIO] Stream not playing, restarting\n");
    PlayAudioStream(g_raylib.audio_stream);
  }

  g_raylib_audio_calls++;

  /* Push one buffer per IsAudioStreamProcessed() tick.
   * Raylib double-buffers internally; we may fill 0, 1, or 2 per frame. */
  int buffers_this_frame = 0;
  while (IsAudioStreamProcessed(g_raylib.audio_stream)) {
    props->game.audio.sample_count = g_raylib.buffer_size_frames;
    game_get_audio_samples(game_audio, &props->game.audio);
    UpdateAudioStream(g_raylib.audio_stream, props->game.audio.samples_buffer,
                      g_raylib.buffer_size_frames);
    buffers_this_frame++;
    g_raylib_buffers_filled++;
  }

  /* DEBUG: Log every 60 frames (~1 second) */
  if (g_raylib_audio_calls % 60 == 0) {
    printf("[RAYLIB_AUDIO] calls=%d total_buffers=%d avg_per_frame=%.2f\n",
           g_raylib_audio_calls, g_raylib_buffers_filled,
           (float)g_raylib_buffers_filled / g_raylib_audio_calls);
  }

  /* DEBUG: Warn if we filled 0 buffers (potential underrun) */
  if (buffers_this_frame == 0 && g_raylib_audio_calls > 10) {
    /* Only warn after initial startup */
    static int zero_buffer_count = 0;
    zero_buffer_count++;
    if (zero_buffer_count % 30 == 1) { /* Don't spam */
      printf("[RAYLIB_AUDIO] WARNING: No buffers needed this frame (call=%d)\n",
             g_raylib_audio_calls);
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main Loop
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0)
    return 1;

  if (platform_init(&props) != 0)
    return 1;

  GameInput input = {0};
  GameState state = {0};
  state.audio.samples_per_second = props.game.audio.samples_per_second;
  game_init(&state, &props.game.audio);

  while (props.is_running && !WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.05f)
      dt = 0.05f;

    prepare_input_frame(&input);
    platform_get_input(&input, &props);
    if (input.quit)
      break;

    game_update(&state, &input, &props.game.audio, dt);
    game_audio_update(&state.audio, dt);
    process_audio(&props, &state.audio);

    game_render(&state, &props.game.backbuffer);
    display_backbuffer(&props.game.backbuffer);
  }

  platform_game_props_free(&props);
  platform_shutdown();
  return 0;
}
