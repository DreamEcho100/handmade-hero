#ifndef PLATFORM_H
#define PLATFORM_H

#include "./game/base.h"
#include "./utils/audio.h"
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

  props->title = TITLE;
  props->width = SCREEN_INITIAL_WIDTH;
  props->height = SCREEN_INITIAL_HEIGHT;
  props->is_running = true;

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
  props->game.audio.sample_count = 0;
  props->game.audio.is_initialized = false;

  /* Allocate enough for 8 frames @ 60 Hz — well above any single write.  */
  /* Backends (ALSA: ≤samples_per_frame*4, Raylib: 4096) stay within this. */
  int max_samples = (props->game.audio.samples_per_second / 60) * 8;
  props->game.audio.max_sample_count = max_samples; /* capacity cap */
  props->game.audio.samples_buffer =
      (int16_t *)malloc((size_t)max_samples * 2 * sizeof(int16_t));

  if (!props->game.audio.samples_buffer) {
    fprintf(stderr, "❌ Failed to allocate audio buffer\n");
    free(props->game.backbuffer.pixels);
    return 1;
  }
  printf("✓ Audio buffer: %d max stereo pairs (%.0f ms @ %d Hz)\n", max_samples,
         (float)max_samples / props->game.audio.samples_per_second * 1000.0f,
         props->game.audio.samples_per_second);

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
 * Input Buffer Swap
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Swap the CONTENTS of two GameInput structs so that:
 *   old_input     ← what current_input held last frame
 *   current_input ← gets old_input (ready for prepare_input_frame)
 *
 * Called each frame BEFORE prepare_input_frame().
 */
static inline void platform_swap_input_buffers(GameInput *old_input,
                                               GameInput *current_input) {
  GameInput temp = *old_input;
  *old_input = *current_input;
  *current_input = temp;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform API (implemented by backend)
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_init(PlatformGameProps *props);
void platform_shutdown(void);
void platform_get_input(GameInput *input, PlatformGameProps *props);

#endif /* PLATFORM_H */
