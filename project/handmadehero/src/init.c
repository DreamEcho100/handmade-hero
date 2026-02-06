// IWYU pragma: keep // clangd: unused-include-ignore // NOLINTNEXTLINE(clang-diagnostic-unused-include)
#include "./inputs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../../engine/game/game-loader.h"
#include "main.h"

#if DE100_INTERNAL
#include "../../engine/_common/debug-file-io.h"
#endif

GAME_INIT(game_init) {
  (void)thread_context;
  (void)inputs;
  (void)buffer;
  // Add at the end of the file to verify
  DEV_ASSERT_MSG(sizeof(_GameButtonsCounter) == sizeof(GameButtonState) * 12,
                 "Button struct size mismatch");

  HandMadeHeroGameState *game_state =
      (HandMadeHeroGameState *)memory->permanent_storage;

  if (!memory->is_initialized) { // false (skip!)
                                 // Never runs again!
                                 // First-time initialization
#if DE100_INTERNAL
    printf("[GAME] First-time init\n");
#endif

#if DE100_INTERNAL
    char *Filename = __FILE__;

    De100DebugDe100FileReadResult file =
        de100_debug_platform_read_entire_file(Filename);
    if (file.memory.base) {
      de100_debug_platform_write_entire_file("out/test.out", file.size,
                                             file.memory.base);
      de100_debug_platform_free_de100_file_memory(&file.memory);
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