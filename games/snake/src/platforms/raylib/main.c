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
 * Audio
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int platform_audio_init(AudioOutputBuffer *audio_buffer) {
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "⚠ Raylib: Audio device not ready\n");
    audio_buffer->is_initialized = false;
    return 1;
  }

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

  /* Pre-fill both double-buffers with silence */
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
  SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);

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
    input->quit = 1;
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

static void process_audio(PlatformGameProps *props,
                          GameAudioState *game_audio) {
  if (!props->game.audio.is_initialized || !g_raylib.audio_initialized)
    return;

  if (!IsAudioStreamPlaying(g_raylib.audio_stream)) {
    PlayAudioStream(g_raylib.audio_stream);
  }

  while (IsAudioStreamProcessed(g_raylib.audio_stream)) {
    /* Clamp chunk to the game buffer's allocated capacity. */
    int chunk = g_raylib.buffer_size_frames;
    if (props->game.audio.max_sample_count > 0 &&
        chunk > props->game.audio.max_sample_count) {
      chunk = props->game.audio.max_sample_count;
    }
    props->game.audio.sample_count = chunk;
    game_get_audio_samples(game_audio, &props->game.audio);
    UpdateAudioStream(g_raylib.audio_stream, props->game.audio.samples_buffer,
                      chunk);
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

  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameState state = {0};
  state.audio.samples_per_second = props.game.audio.samples_per_second;
  game_init(&state, &props.game.audio);

  while (props.is_running && !WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.05f)
      dt = 0.05f;

    /* Double-buffered input */
    platform_swap_input_buffers(&inputs[0], &inputs[1]);
    prepare_input_frame(&inputs[0], &inputs[1]);
    platform_get_input(&inputs[1], &props);
    if (inputs[1].quit)
      break;

    game_update(&state, &inputs[1], &props.game.audio, dt);
    game_audio_update(&state.audio, dt);
    process_audio(&props, &state.audio);

    game_render(&state, &props.game.backbuffer);
    display_backbuffer(&props.game.backbuffer);
  }

  platform_game_props_free(&props);
  platform_shutdown();
  return 0;
}
