#include "../../game/demo.h"
#include "../../platform.h"
#include "../../utils/backbuffer.h"
#include "../../utils/math.h"

#include <raylib.h>
#include <stddef.h> // provides 'defer' as alias for '_Defer'
#include <stdio.h>

typedef struct {
  Texture2D texture;
  int prev_win_w;
  int prev_win_h;
} RaylibGameState;

static RaylibGameState g_raylib = {0};

/* NEW: Apply scale mode */
static void apply_scale_mode(PlatformGameProps *props, int win_w, int win_h) {
  int new_w, new_h;

  switch (props->scale_mode) {
  case WINDOW_SCALE_MODE_FIXED:
    new_w = GAME_W;
    new_h = GAME_H;
    break;

  case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
    new_w = win_w;
    new_h = win_h;
    break;

  case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
    platform_compute_aspect_size(win_w, win_h, 4.0f, 3.0f, &new_w, &new_h);
    break;

  default:
    new_w = GAME_W;
    new_h = GAME_H;
    break;
  }

  if (new_w != props->backbuffer.width || new_h != props->backbuffer.height) {
    backbuffer_resize(&props->backbuffer, new_w, new_h);

    /* Recreate GPU texture */
    UnloadTexture(g_raylib.texture);

    Image img = {.data = props->backbuffer.pixels,
                 .width = new_w,
                 .height = new_h,
                 .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    g_raylib.texture = LoadTextureFromImage(img);
    SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);
  }
}

static void display_backbuffer(Backbuffer *backbuffer,
                               WindowScaleMode scale_mode) {
  /* Upload CPU pixels to GPU texture */
  UpdateTexture(g_raylib.texture, backbuffer->pixels);

  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();

  float off_x, off_y, dst_w, dst_h;

  if (scale_mode == WINDOW_SCALE_MODE_FIXED) {
    /* GPU letterbox scaling */
    float sx = (float)win_w / (float)backbuffer->width;
    float sy = (float)win_h / (float)backbuffer->height;
    float scale = MIN(sx, sy);
    dst_w = (float)backbuffer->width * scale;
    dst_h = (float)backbuffer->height * scale;
    off_x = ((float)win_w - dst_w) * 0.5f;
    off_y = ((float)win_h - dst_h) * 0.5f;
  } else {
    /* Dynamic modes: center backbuffer in window (1:1 scale) */
    dst_w = (float)backbuffer->width;
    dst_h = (float)backbuffer->height;
    off_x = ((float)win_w - dst_w) * 0.5f;
    off_y = ((float)win_h - dst_h) * 0.5f;
  }

  Rectangle src = {0.0f, 0.0f, (float)backbuffer->width,
                   (float)backbuffer->height};
  Rectangle dest = {off_x, off_y, dst_w, dst_h};

  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexturePro(g_raylib.texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
  EndDrawing();
}

int main(void) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(GAME_W, GAME_H, TITLE);
  SetTargetFPS(TARGET_FPS);
  SetWindowState(FLAG_WINDOW_RESIZABLE);

  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    fprintf(stderr, "Failed to allocate game props\n");
    return 1;
  }
  Backbuffer *backbuffer = &props.backbuffer;

  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  /* Initialize previous window size */
  g_raylib.prev_win_w = GetScreenWidth();
  g_raylib.prev_win_h = GetScreenHeight();

  // Create GPU texture — PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 matches our
  // [R,G,B,A] layout
  Image img = {.data = backbuffer->pixels,
               .width = GAME_W,
               .height = GAME_H,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  g_raylib.texture = LoadTextureFromImage(img);
  SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);

  while (!WindowShouldClose()) {
    /* Prepare: copy ended_down from prev; reset half_transitions */
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* Poll Raylib input into GameInput */
    if (IsKeyDown(KEY_ESCAPE) || IsKeyDown(KEY_Q)) {
      UPDATE_BUTTON(&curr_input->buttons.quit, 1);
      break;
    }
    if (IsKeyDown(KEY_SPACE)) {
      UPDATE_BUTTON(&curr_input->buttons.play_tone, 1);
    } else {
      UPDATE_BUTTON(&curr_input->buttons.play_tone, 0);
    }

    /* NEW: Handle TAB for scale mode cycling */
    if (IsKeyDown(KEY_TAB)) {
      UPDATE_BUTTON(&curr_input->buttons.cycle_scale_mode, 1);
    } else {
      UPDATE_BUTTON(&curr_input->buttons.cycle_scale_mode, 0);
    }

    /* Quit on Q pressed */
    if (curr_input->buttons.quit.ended_down) {
      break;
    }

    /* NEW: Cycle scale mode on TAB press */
    if (curr_input->buttons.cycle_scale_mode.ended_down &&
        curr_input->buttons.cycle_scale_mode.half_transitions > 0) {
      props.scale_mode = (props.scale_mode + 1) % WINDOW_SCALE_MODE_COUNT;
      printf("Scale mode: %d\n", props.scale_mode);
    }

    /* NEW: Handle window resize */
    if (IsWindowResized()) {
      int win_w = GetScreenWidth();
      int win_h = GetScreenHeight();
      g_raylib.prev_win_w = win_w;
      g_raylib.prev_win_h = win_h;
      apply_scale_mode(&props, win_w, win_h);
    }

    /* Fill: platform event loop writes into curr_input */
    /*    (X11: XPending drain loop / Raylib: IsKeyDown polling) */

    /* React: game reads curr_input */
    demo_render(&props.backbuffer, curr_input, props.world_config,
                props.scale_mode, GetFPS());

    display_backbuffer(backbuffer, props.scale_mode);
  }

  UnloadTexture(g_raylib.texture);
  platform_game_props_free(&props);
  CloseWindow();

  return 0;
}