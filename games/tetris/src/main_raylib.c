#include "game.h"
#include "platform.h"

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform State
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  Texture2D texture;
  AudioStream stream;
  int16_t *sample_buffer;
  int sample_buffer_size;
  int buffer_size_frames; /* Raylib internal buffer size */
} RaylibState;

static RaylibState g_raylib = {0};

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Initialization (Casey's Latency Model - Raylib Adaptation)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Key difference from ALSA: We can't query exact buffer state, so we need:
 * 1. Larger buffers (more margin for timing variance)
 * 2. Pre-fill before starting playback
 * 3. Fill aggressively every frame
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_audio_init(PlatformAudioConfig *config) {
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "Failed to initialize audio device\n");
    config->is_initialized = 0;
    return 1;
  }

  config->samples_per_second = 48000;

  /* Casey's latency calculations */
  config->samples_per_frame = config->samples_per_second / config->hz;
  config->latency_samples =
      config->samples_per_frame * config->frames_of_latency;
  config->safety_samples = config->samples_per_frame / 3;
  config->running_sample_index = 0;

  /*
   * CRITICAL FIX: Use larger buffer size for Raylib
   *
   * Unlike ALSA where we can query exact delay, Raylib uses
   * IsAudioStreamProcessed() which is just a boolean. Larger buffers give us
   * more margin for timing variance.
   *
   * 2x frame size = ~33ms buffer, matches our latency target
   */
  config->buffer_size_samples = config->samples_per_frame * 2;
  g_raylib.buffer_size_frames = config->buffer_size_samples;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 RAYLIB AUDIO (Casey's Latency Model)\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:     %d Hz\n", config->samples_per_second);
  printf("  Game update:     %d Hz\n", config->hz);
  printf("  Samples/frame:   %d (%.1f ms)\n", config->samples_per_frame,
         (float)config->samples_per_frame / config->samples_per_second *
             1000.0f);
  printf("  Target latency:  %d samples (%.1f ms, %d frames)\n",
         config->latency_samples,
         (float)config->latency_samples / config->samples_per_second * 1000.0f,
         config->frames_of_latency);
  printf("  Safety margin:   %d samples (%.1f ms)\n", config->safety_samples,
         (float)config->safety_samples / config->samples_per_second * 1000.0f);
  printf("  Buffer size:     %d samples (%.1f ms)\n",
         config->buffer_size_samples,
         (float)config->buffer_size_samples / config->samples_per_second *
             1000.0f);

  /* MUST set buffer size BEFORE creating stream */
  SetAudioStreamBufferSizeDefault(config->buffer_size_samples);

  /* Create audio stream */
  g_raylib.stream = LoadAudioStream(config->samples_per_second, 16, 2);

  if (!IsAudioStreamValid(g_raylib.stream)) {
    fprintf(stderr, "Failed to create audio stream\n");
    CloseAudioDevice();
    config->is_initialized = 0;
    return 1;
  }

  /* Allocate sample buffer (enough for several frames) */
  g_raylib.sample_buffer_size = config->samples_per_frame * 4;
  g_raylib.sample_buffer =
      (int16_t *)malloc(g_raylib.sample_buffer_size * 2 * sizeof(int16_t));

  if (!g_raylib.sample_buffer) {
    fprintf(stderr, "Failed to allocate audio buffer\n");
    UnloadAudioStream(g_raylib.stream);
    CloseAudioDevice();
    config->is_initialized = 0;
    return 1;
  }

  /*
   * CRITICAL FIX: Pre-fill buffers with silence before starting playback
   *
   * This ensures we have latency_samples worth of audio buffered before
   * the audio device starts consuming. Prevents initial clicks/underruns.
   *
   * Raylib uses double-buffering, so we fill both buffers.
   */
  memset(g_raylib.sample_buffer, 0,
         g_raylib.buffer_size_frames * 2 * sizeof(int16_t));

  /* Fill first buffer */
  UpdateAudioStream(g_raylib.stream, g_raylib.sample_buffer,
                    g_raylib.buffer_size_frames);
  /* Fill second buffer */
  UpdateAudioStream(g_raylib.stream, g_raylib.sample_buffer,
                    g_raylib.buffer_size_frames);

  /* Now start playback */
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
    free(g_raylib.sample_buffer);
    g_raylib.sample_buffer = NULL;
  }
  if (IsAudioStreamValid(g_raylib.stream)) {
    StopAudioStream(g_raylib.stream);
    UnloadAudioStream(g_raylib.stream);
  }
  CloseAudioDevice();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Update (Casey's Model Adapted for Raylib)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Raylib's model:
 * - Double-buffered internally
 * - IsAudioStreamProcessed() returns true when a buffer was consumed
 * - We fill consumed buffers immediately to maintain latency
 *
 * Key differences from ALSA:
 * - Can't query exact play position
 * - Must fill exactly buffer_size_frames per call
 * - Fill ALL available buffers each frame to stay ahead
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void platform_audio_update(GameState *game_state,
                                  PlatformAudioConfig *config) {
  if (!config->is_initialized)
    return;

  /*
   * Fill ALL available buffers this frame
   *
   * Raylib double-buffers, so we might need to fill 0, 1, or 2 buffers
   * depending on how much time passed since last frame.
   *
   * Max 4 iterations as safety limit (handles frame spikes)
   */
  int buffers_filled = 0;
  const int max_buffers = 4;

  while (buffers_filled < max_buffers &&
         IsAudioStreamProcessed(g_raylib.stream)) {
    /* Generate exactly buffer_size_frames samples */
    AudioOutputBuffer buffer = {.samples_buffer = g_raylib.sample_buffer,
                                .samples_per_second =
                                    config->samples_per_second,
                                .sample_count = g_raylib.buffer_size_frames};

    game_get_audio_samples(game_state, &buffer);
    UpdateAudioStream(g_raylib.stream, buffer.samples_buffer,
                      buffer.sample_count);

    config->running_sample_index += buffer.sample_count;
    buffers_filled++;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Init / Shutdown
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_init(PlatformGameProps *props) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(props->width, props->height, props->title);
  SetTargetFPS(60);

  Image img = {.data = props->backbuffer.pixels,
               .width = props->backbuffer.width,
               .height = props->backbuffer.height,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  g_raylib.texture = LoadTextureFromImage(img);

  platform_audio_init(&props->audio);

  return 0;
}

void platform_shutdown(void) {
  platform_audio_shutdown();
  UnloadTexture(g_raylib.texture);
  CloseWindow();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  PlatformGameProps platform_game_props = {0};
  if (platform_game_props_init(&platform_game_props) != 0) {
    return 1;
  }

  if (platform_init(&platform_game_props) != 0) {
    return 1;
  }

  GameInput inputs[2] = {0};
  GameInput *current_input = &inputs[0];
  GameInput *old_input = &inputs[1];

  GameState game_state = {0};
  game_init(&game_state);

  game_audio_init(&game_state.audio,
                  platform_game_props.audio.samples_per_second);
  game_music_play(&game_state.audio);

  while (!WindowShouldClose()) {
    float delta_time = GetFrameTime();

    /* ═══════════════════════════════════════════════════════════════════
     * Input
     * ═════════��═════════════════════════════════════════════════════════ */
    prepare_input_frame(old_input, current_input);

    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
      game_state.should_quit = 1;
      break;
    }

    game_state.should_restart = IsKeyPressed(KEY_R);

    if (IsKeyPressed(KEY_X)) {
      UPDATE_BUTTON(current_input->rotate_x, 1);
      game_state.current_piece.next_rotate_x_value =
          TETROMINO_ROTATE_X_GO_RIGHT;
    } else if (IsKeyPressed(KEY_Z)) {
      UPDATE_BUTTON(current_input->rotate_x, 1);
      game_state.current_piece.next_rotate_x_value = TETROMINO_ROTATE_X_GO_LEFT;
    } else if (IsKeyReleased(KEY_X) || IsKeyReleased(KEY_Z)) {
      UPDATE_BUTTON(current_input->rotate_x, 0);
      game_state.current_piece.next_rotate_x_value = TETROMINO_ROTATE_X_NONE;
    }

    UPDATE_BUTTON(current_input->move_left,
                  IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    // if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
    //   printf("Is KeyPressed: LEFT\n");
    //   UPDATE_BUTTON(current_input->move_left, true);
    // } else if (IsKeyReleased(KEY_A) || IsKeyReleased(KEY_LEFT)) {
    //   UPDATE_BUTTON(current_input->move_left, false);
    // }
    //
    UPDATE_BUTTON(current_input->move_right,
                  IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
    // if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
    //   UPDATE_BUTTON(current_input->move_right, true);
    // } else if (IsKeyReleased(KEY_D) || IsKeyReleased(KEY_RIGHT)) {
    //   UPDATE_BUTTON(current_input->move_right, false);
    // }
    //
    UPDATE_BUTTON(current_input->move_down,
                  IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
    // if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
    //   UPDATE_BUTTON(current_input->move_down, true);
    // } else if (IsKeyReleased(KEY_S) || IsKeyReleased(KEY_DOWN)) {
    //   UPDATE_BUTTON(current_input->move_down, false);
    // }

    /* ═══════════════════════════════════════════════════════════════════
     * Update
     * ═══════════════════════════════════════════════════════════════════ */
    game_update(&game_state, current_input, delta_time);

    /* Audio - fill any consumed buffers */
    platform_audio_update(&game_state, &platform_game_props.audio);

    /* ═══════════════════════════════════════════════════════════════════
     * Render
     * ═══════════════════════════════════════════════════════════════════ */
    game_render(&platform_game_props.backbuffer, &game_state);

    /* ═══════════════════════════════════════════════════════════════════
     * Display
     * ═══════════════════════════════════════════════════════════════════ */
    UpdateTexture(g_raylib.texture, platform_game_props.backbuffer.pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    if (IsWindowResized()) {
      printf("Window resized: %d x %d\n", GetScreenWidth(), GetScreenHeight());
      platform_game_props.width = GetScreenWidth();
      platform_game_props.height = GetScreenHeight();
    }
    int offset_x =
        (platform_game_props.width - platform_game_props.backbuffer.width) / 2;
    int offset_y =
        (platform_game_props.height - platform_game_props.backbuffer.height) /
        2;
    DrawTexture(g_raylib.texture, offset_x, offset_y, WHITE);
    EndDrawing();

    platform_swap_input_buffers(old_input, current_input);
  }

  platform_game_props_free(&platform_game_props);
  platform_shutdown();
  return 0;
}
