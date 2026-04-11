/* ══════════════════════════════════════════════════════════════════════ */
/*               main.c — Raylib Backend for LOATs Demo                  */
/*                                                                       */
/*  Opens a window, runs the game loop, feeds input to game_update,      */
/*  renders game_render output to screen via Raylib texture.             */
/* ══════════════════════════════════════════════════════════════════════ */
#include "raylib.h"
#include "../../platform.h"

#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset */
#include <stdio.h>  /* snprintf */

int main(void)
{
    /* ── Window ── */
    InitWindow(SCREEN_W, SCREEN_H, GAME_TITLE);
    SetTargetFPS(TARGET_FPS);

    /* ── Pixel buffer (CPU-side backbuffer) ── */
    uint32_t *pixels = (uint32_t *)malloc(SCREEN_W * SCREEN_H * sizeof(uint32_t));
    if (!pixels) return 1;
    memset(pixels, 0, SCREEN_W * SCREEN_H * sizeof(uint32_t));

    /* Raylib texture for uploading our CPU pixels to the GPU. */
    Image img = {
        .data    = pixels,
        .width   = SCREEN_W,
        .height  = SCREEN_H,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D tex = LoadTextureFromImage(img);

    /* ── Game state ── */
    game_state state;
    memset(&state, 0, sizeof(state));
    state.current_scene = 8; /* default scene */
    game_init(&state);

    /* ── Main loop ── */
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f; /* cap delta to prevent spiral of death */

        /* ── Input ── */
        game_input input = {0};
        input.left  = IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A);
        input.right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
        input.up    = IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W);
        input.down  = IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S);
        input.space = IsKeyPressed(KEY_SPACE);

        /* Scene selection: number keys 1-9 */
        for (int k = KEY_ONE; k <= KEY_NINE; k++) {
            if (IsKeyPressed(k)) {
                input.scene_key = k - KEY_ONE + 1;
                break;
            }
        }
        /* Lab keys — always available */
        if (IsKeyPressed(KEY_P)) input.scene_key = 10;   /* Particle Lab */
        if (IsKeyPressed(KEY_L)) input.scene_key = 11;   /* Data Layout Lab */
        if (IsKeyPressed(KEY_G)) input.scene_key = 12;   /* Spatial Partition Lab (G=Grid) */
        if (IsKeyPressed(KEY_M)) input.scene_key = 13;   /* Memory Arena Lab */
        if (IsKeyPressed(KEY_I)) input.scene_key = 14;   /* Infinite Growth Lab */
        if (IsKeyPressed(KEY_TAB)) input.tab_pressed = true; /* cycle mode in labs */

        /* Reset on R. */
        if (IsKeyPressed(KEY_R)) {
            game_init(&state);
        }

        /* ── Update ── */
        game_update(&state, &input, dt);

        /* ── Render to CPU pixel buffer ── */
        game_render(&state, pixels, SCREEN_W, SCREEN_H);

        /* ── Upload pixels to GPU and draw ── */
        /* HUD text is already rendered into the pixel buffer by game_render
         * using the FONT_8X8 bitmap font. Both backends show identical output. */
        UpdateTexture(tex, pixels);
        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexture(tex, 0, 0, WHITE);
        EndDrawing();
    }

    /* ── Cleanup ── */
    UnloadTexture(tex);
    free(pixels);
    CloseWindow();
    return 0;
}
