#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/player.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Player initialization, physics update, rendering, and damage handling.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./types.h"
#include "./base.h"
#include "../utils/backbuffer.h"

/* Initialize the player struct and load sprites. */
int player_init(Player *player);

/* Free player sprite resources. */
void player_free(Player *player);

/* Update player physics and state for one frame. */
void player_update(Player *player, GameInput *input, float dt);

/* Render the player to the backbuffer. */
void player_render(const Player *player, Backbuffer *bb, float camera_x);

/* Apply damage to the player. Returns 1 if player died, 0 if shield absorbed. */
int player_hurt(Player *player);

/* Trigger death state. */
void player_die(Player *player);

#endif /* GAME_PLAYER_H */
