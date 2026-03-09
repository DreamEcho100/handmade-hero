#ifndef PLATFORM_H
#define PLATFORM_H

#include "./game/base.h"
#include "./utils/audio.h" /* AudioOutputBuffer */
#include "./utils/backbuffer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Configuration
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  Backbuffer backbuffer;
  AudioOutputBuffer audio;
} GameProps;

typedef struct {
  const char *title;
  int width;
  int height;
  GameProps game;
  bool is_running;
} PlatformGameProps;

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Game Props Init/Free
 * ═══════════════════════════════════════════════════════════════════════════
 */

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define TITLE ("Snake (" TOSTRING(BACKEND) ") - yt/@javidx9")

static inline int platform_game_props_init(PlatformGameProps *props) {
  printf("═══════════════════════════════════════════════════════════\n");
  printf("🐍 SNAKE GAME INITIALIZATION\n");
  printf("═══════════════════════════════════════════════════════════\n");

  /* Window config */
  props->title = TITLE;
  props->width = SCREEN_INITIAL_WIDTH;
  props->height = SCREEN_INITIAL_HEIGHT;
  props->is_running = true;

  /* Backbuffer allocation */
  props->game.backbuffer.width = props->width;
  props->game.backbuffer.height = props->height;
  props->game.backbuffer.bytes_per_pixel = 4;
  props->game.backbuffer.pitch = props->width * 4;
  props->game.backbuffer.pixels =
      (uint32_t *)malloc(props->width * props->height * sizeof(uint32_t));

  if (!props->game.backbuffer.pixels) {
    fprintf(stderr, "❌ Failed to allocate backbuffer\n");
    return 1;
  }
  printf("✓ Backbuffer: %dx%d\n", props->width, props->height);

  props->game.audio.samples_per_second = 48000;
  props->game.audio.sample_count = 0; /* Set by platform */
  props->game.audio.is_initialized = false;

  /* Audio buffer - must fit the largest single platform write.
   * Raylib uses a 4096-sample stream buffer, so allocate 8 frames (~6400)
   * to cover any reasonable backend with headroom to spare. */
  int max_samples = (props->game.audio.samples_per_second / 60) * 8;
  props->game.audio.samples_buffer =
      (int16_t *)malloc(max_samples * sizeof(int16_t) * 2); /* Stereo */

  if (!props->game.audio.samples_buffer) {
    fprintf(stderr, "❌ Failed to allocate audio buffer\n");
    free(props->game.backbuffer.pixels);
    return 1;
  }
  printf("✓ Audio buffer: %d max samples\n", max_samples);

  printf("═══════════════════════════════════════════════════════════\n\n");
  return 0;
}

static inline void platform_game_props_free(PlatformGameProps *props) {
  if (props->game.backbuffer.pixels) {
    free(props->game.backbuffer.pixels);
    props->game.backbuffer.pixels = NULL;
  }
  if (props->game.audio.samples_buffer) {
    free(props->game.audio.samples_buffer);
    props->game.audio.samples_buffer = NULL;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform API (implemented by backend)
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_init(PlatformGameProps *props);
void platform_shutdown(void);
void platform_get_input(GameInput *input, PlatformGameProps *props);
double platform_get_time(void);

#endif /* PLATFORM_H */
