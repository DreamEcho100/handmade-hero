#include "init.h"
#include "main.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if DE100_INTERNAL
#include "../../engine/_common/debug-file-io.h"
#endif

GAME_INIT(game_init) {
  (void)input;
  (void)buffer;
  HandMadeHeroGameState *game_state =
      (HandMadeHeroGameState *)memory->permanent_storage.base;

  if (!memory->is_initialized) { // false (skip!)
                                 // Never runs again!
                                 // First-time initialization
#if DE100_INTERNAL
    printf("[GAME] First-time init\n");
#endif

#if DE100_INTERNAL
    char *Filename = __FILE__;

    DebugReadFileResult file = debug_platform_read_entire_file(Filename);
    if (file.contents.base) {
      debug_platform_write_entire_file("out/test.out", file.size,
                                       file.contents.base);
      debug_platform_free_file_memory(&file.contents);
      printf("Wrote test.out\n");
    }
#endif
    printf("[GAME INIT] game_update_and_render from build id %d\n",
           1); // change this number each rebuild

    // Initialize audio state
    game_state->audio.tone.frequency = 256;
    game_state->audio.tone.phase = 0.0f;
    game_state->audio.tone.volume = 1.0f;
    game_state->audio.tone.pan_position = 0.0f;
    game_state->audio.tone.is_playing = true;
    game_state->audio.master_volume = 1.0f;

    game_state->gradient_state = (GradientState){0};
    game_state->pixel_state = (PixelState){0};
    game_state->speed = 5;

    memory->is_initialized = true;
#if DE100_INTERNAL
    printf("[GAME] Init complete\n");
#endif
    return;
  }
}