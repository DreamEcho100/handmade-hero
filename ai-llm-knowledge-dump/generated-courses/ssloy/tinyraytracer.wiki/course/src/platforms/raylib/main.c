/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/raylib/main.c — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform.h"
#include "../../game/base.h"
#include "../../game/main.h"
#include "../../utils/math.h"

static Texture2D g_texture = {0};

static void display_backbuffer(Backbuffer *bb) {
  UpdateTexture(g_texture, bb->pixels);
  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();
  float sx = (float)win_w / (float)bb->width;
  float sy = (float)win_h / (float)bb->height;
  float scale = MIN(sx, sy);
  float dst_w = (float)bb->width  * scale;
  float dst_h = (float)bb->height * scale;
  float off_x = ((float)win_w - dst_w) * 0.5f;
  float off_y = ((float)win_h - dst_h) * 0.5f;

  Rectangle src  = { 0.0f, 0.0f, (float)bb->width, (float)bb->height };
  Rectangle dest = { off_x, off_y, dst_w, dst_h };

  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexturePro(g_texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
  EndDrawing();
}

int main(void) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(GAME_W, GAME_H, TITLE);
  SetTargetFPS(TARGET_FPS);
  if (!IsWindowReady()) { fprintf(stderr, "InitWindow failed\n"); return 1; }
  SetWindowState(FLAG_WINDOW_RESIZABLE);

  Backbuffer bb = {
    .width = GAME_W, .height = GAME_H,
    .bytes_per_pixel = 4, .pitch = GAME_W * 4,
  };
  bb.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
  if (!bb.pixels) { CloseWindow(); return 1; }
  memset(bb.pixels, 0, (size_t)(GAME_W * GAME_H) * 4);

  Image img = {
    .data = bb.pixels, .width = GAME_W, .height = GAME_H,
    .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  g_texture = LoadTextureFromImage(img);
  SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);
  if (!IsTextureValid(g_texture)) {
    free(bb.pixels); CloseWindow(); return 1;
  }

  RaytracerState state;
  game_init(&state);

  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  float prev_mouse_x = 0.0f, prev_mouse_y = 0.0f;

  while (!WindowShouldClose()) {
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* ── Keyboard ──────────────────────────────────────────────────────── */
    UPDATE_BUTTON(&curr_input->buttons.quit,
                  IsKeyDown(KEY_ESCAPE) || IsKeyDown(KEY_Q));
    UPDATE_BUTTON(&curr_input->buttons.camera_left,
                  IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A));
    UPDATE_BUTTON(&curr_input->buttons.camera_right,
                  IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D));
    UPDATE_BUTTON(&curr_input->buttons.camera_up,
                  IsKeyDown(KEY_UP));
    UPDATE_BUTTON(&curr_input->buttons.camera_down,
                  IsKeyDown(KEY_DOWN));
    UPDATE_BUTTON(&curr_input->buttons.camera_forward,
                  IsKeyDown(KEY_W));
    UPDATE_BUTTON(&curr_input->buttons.camera_backward,
                  IsKeyDown(KEY_S) && !IsKeyDown(KEY_LEFT_SHIFT));

    /* Exports (Shift+key to avoid conflicts with WASD) */
    UPDATE_BUTTON(&curr_input->buttons.export_ppm, IsKeyDown(KEY_P));
    UPDATE_BUTTON(&curr_input->buttons.export_anaglyph,
                  IsKeyDown(KEY_A) && IsKeyDown(KEY_LEFT_SHIFT));
    UPDATE_BUTTON(&curr_input->buttons.export_sidebyside,
                  IsKeyDown(KEY_S) && IsKeyDown(KEY_LEFT_SHIFT));
    UPDATE_BUTTON(&curr_input->buttons.export_stereogram,
                  IsKeyDown(KEY_G) && !IsKeyDown(KEY_LEFT_SHIFT));
    UPDATE_BUTTON(&curr_input->buttons.export_stereogram_cross,
                  IsKeyDown(KEY_G) && IsKeyDown(KEY_LEFT_SHIFT));
    UPDATE_BUTTON(&curr_input->buttons.export_glsl,
                  IsKeyDown(KEY_L) && IsKeyDown(KEY_LEFT_SHIFT));

    /* Toggles */
    UPDATE_BUTTON(&curr_input->buttons.toggle_voxels, IsKeyDown(KEY_V));
    UPDATE_BUTTON(&curr_input->buttons.toggle_floor, IsKeyDown(KEY_F));
    UPDATE_BUTTON(&curr_input->buttons.toggle_boxes, IsKeyDown(KEY_B));
    UPDATE_BUTTON(&curr_input->buttons.toggle_meshes, IsKeyDown(KEY_M));
    UPDATE_BUTTON(&curr_input->buttons.toggle_reflections, IsKeyDown(KEY_R));
    UPDATE_BUTTON(&curr_input->buttons.toggle_refractions, IsKeyDown(KEY_T));
    UPDATE_BUTTON(&curr_input->buttons.toggle_shadows, IsKeyDown(KEY_H));
    UPDATE_BUTTON(&curr_input->buttons.toggle_envmap, IsKeyDown(KEY_E));
    UPDATE_BUTTON(&curr_input->buttons.toggle_aa, IsKeyDown(KEY_X));
    UPDATE_BUTTON(&curr_input->buttons.cycle_envmap_mode, IsKeyDown(KEY_C));
    UPDATE_BUTTON(&curr_input->buttons.scale_cycle, IsKeyDown(KEY_TAB));

    /* ── Mouse ─────────────────────────────────────────────────────────── */
    Vector2 mpos = GetMousePosition();
    curr_input->mouse.x  = mpos.x;
    curr_input->mouse.y  = mpos.y;
    curr_input->mouse.dx = mpos.x - prev_mouse_x;
    curr_input->mouse.dy = mpos.y - prev_mouse_y;
    prev_mouse_x = mpos.x;
    prev_mouse_y = mpos.y;

    curr_input->mouse.scroll     = GetMouseWheelMove();
    curr_input->mouse.left_down  = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    curr_input->mouse.right_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    curr_input->mouse.middle_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

    if (curr_input->buttons.quit.ended_down) break;

    float dt = GetFrameTime();
    game_update(&state, curr_input, dt);
    game_render(&state, &bb);
    display_backbuffer(&bb);
  }

  if (IsTextureValid(g_texture)) UnloadTexture(g_texture);
  free(bb.pixels);
  CloseWindow();
  return 0;
}
