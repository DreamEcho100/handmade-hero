#ifndef GAME_SNAKE_H
#define GAME_SNAKE_H

#include "./base.h"

#include "../utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

void prepare_input_frame(const GameInput *old_input, GameInput *current_input);

void game_init(GameState *game_state,
               AudioOutputBuffer *game_audio_output_buffer);
void game_update(GameState *game_state, const GameInput *input,
                 AudioOutputBuffer *game_audio_output_buffer, float delta_time);
void game_render(GameState *game_state, Backbuffer *backbuffer);

#endif // GAME_SNAKE_H
