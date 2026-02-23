#define _XOPEN_SOURCE 700 /* for nanosleep() */
#define _POSIX_C_SOURCE 200809L

#include "platform.h"
#include "tetris.h"

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

typedef struct {
  Display *display;
  Window window;
  GC gc;
  XEvent event;
  int screen;
  int width;
  int height;
  bool is_running;
  struct {
    unsigned long black;
    unsigned long white;
    unsigned long gray;

    unsigned long cyan;
    unsigned long blue;
    unsigned long orange;
    unsigned long yellow;
    unsigned long green;
    unsigned long magenta;
    unsigned long red;
  } alloc_colors;
} X11_State;
bool g_is_game_running = true;
X11_State g_x11 = {0};
double g_last_frame_time = 0.0;
int g_fps = 60;
double g_target_frame_time = 1.0 / 60.0;

static double g_start_time = 0.0;

static double get_current_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

void platform_sleep_ms(int ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;               /* whole seconds: 50/1000 = 0         */
  ts.tv_nsec = (ms % 1000) * 1000000L; /* remainder in nanoseconds: 50×1M=50M */
  nanosleep(&ts, NULL);
}

double platform_get_time(void) {
  if (g_start_time == 0.0) {
    g_start_time = get_current_time();
  }
  return get_current_time() - g_start_time;
}

static unsigned long alloc_color(const char *name) {
  XColor color, exact;
  Colormap cmap = DefaultColormap(g_x11.display, g_x11.screen);
  XAllocNamedColor(g_x11.display, cmap, name, &color, &exact);
  return color.pixel;
}

static void draw_cell(int col, int row, unsigned long color) {
  int x = col * CELL_SIZE + 1;
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 1;
  int h = CELL_SIZE - 1;

  XSetForeground(g_x11.display, g_x11.gc, color);
  XFillRectangle(g_x11.display, g_x11.window, g_x11.gc, x, y, w, h);
}

unsigned long get_tetromino_color_by_index(TETROMINO_BY_IDX piece_index) {
  switch (piece_index) {
  case TETROMINO_I_IDX: {
    return g_x11.alloc_colors.cyan;
  }
  case TETROMINO_J_IDX: {
    return g_x11.alloc_colors.blue;
  }
  case TETROMINO_L_IDX: {
    return g_x11.alloc_colors.orange;
  }
  case TETROMINO_O_IDX: {
    return g_x11.alloc_colors.yellow;
  }
  case TETROMINO_S_IDX: {
    return g_x11.alloc_colors.green;
  }
  case TETROMINO_T_IDX: {
    return g_x11.alloc_colors.magenta;
  }
  case TETROMINO_Z_IDX: {
    return g_x11.alloc_colors.red;
  }
  default: {
    // Impossible, but just in case, return gray for invalid piece index
    printf(
        "Warning: Invalid piece index %d in get_tetromino_color_by_index()\n",
        piece_index);
    return g_x11.alloc_colors.gray;
  }
  }
}

static void draw_piece(int piece_index, int field_col, int field_row,
                       unsigned long color, TETROMINO_R_DIR rotation) {
  int px, py;

  for (py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = tetromino_pos_value(px, py, rotation);

      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(field_col + px, field_row + py, color);
      }
    }
  }
}

int platform_init(const char *title, int width, int height) {
  g_last_frame_time = platform_get_time();
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "Failed to open display\n");
    return 1;
  }

  /* Disable X11 keyboard auto-repeat detection */
  Bool supported;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);
  if (supported) {
    printf("✓ Detectable auto-repeat enabled\n");
  } else {
    printf("⚠ Detectable auto-repeat not supported\n");
  }

  g_x11.screen = DefaultScreen(g_x11.display);

  g_x11.alloc_colors.black = BlackPixel(g_x11.display, g_x11.screen);
  g_x11.alloc_colors.white = WhitePixel(g_x11.display, g_x11.screen);
  g_x11.alloc_colors.gray = alloc_color("gray50");
  g_x11.alloc_colors.cyan = alloc_color("cyan");
  g_x11.alloc_colors.cyan = alloc_color("cyan");
  g_x11.alloc_colors.magenta = alloc_color("magenta");
  g_x11.alloc_colors.yellow = alloc_color("yellow");
  g_x11.alloc_colors.green = alloc_color("green");
  g_x11.alloc_colors.red = alloc_color("red");
  g_x11.alloc_colors.blue = alloc_color("blue");
  g_x11.alloc_colors.orange = alloc_color("orange");

  g_x11.window = XCreateSimpleWindow(
      g_x11.display,                           // display
      RootWindow(g_x11.display, g_x11.screen), // parent window
      100,
      100,                      // x, y
      width, height,            // w, h
      1,                        // border width
      g_x11.alloc_colors.black, // border color
      g_x11.alloc_colors.black  // background color (dark blue)
  );

  g_x11.gc = XCreateGC(g_x11.display, g_x11.window, 0, NULL);

  XStoreName(g_x11.display, g_x11.window, title);
  XSelectInput(g_x11.display, g_x11.window,
               ExposureMask | KeyPressMask | KeyReleaseMask);
  XMapWindow(g_x11.display, g_x11.window); // Show the window
  XFlush(g_x11.display); // Force all the buffered commands to be sent to the X
                         // server

  return 0;
}

void platform_get_input(GameState *state, GameInput *input) {

  while (XPending(g_x11.display) > 0) {
    XNextEvent(g_x11.display, &g_x11.event);
    switch (g_x11.event.type) {
    case KeyPress: {
      KeySym key =
          XLookupKeysym(&g_x11.event.xkey,
                        0); // `0` is the index for the first keysym in the list
                            // (for non-modified keys), and `1` would be for the
                            // second keysym (for Shift-modified keys), etc.
      switch (key) {
      case XK_q:
      case XK_Q:
      case XK_Escape: {
        g_is_game_running = false;
        break;
      }
      case XK_x:
      case XK_X: {
        UPDATE_BUTTON(input->rotate_x.button, 1);
        input->rotate_x.value = TETROMINO_ROTATE_X_GO_RIGHT;
        break;
      }
      case XK_z:
      case XK_Z: {
        UPDATE_BUTTON(input->rotate_x.button, 1);
        input->rotate_x.value = TETROMINO_ROTATE_X_GO_LEFT;
        break;
      }
      case XK_Left:
      case XK_A:
      case XK_a: {
        UPDATE_BUTTON(input->move_left.button, 1);
        break;
      }
      case XK_Right:
      case XK_D:
      case XK_d: {
        UPDATE_BUTTON(input->move_right.button, 1);
        break;
      }
      case XK_Down:
      case XK_S:
      case XK_s: {
        UPDATE_BUTTON(input->move_down.button, 1);
        break;
      }
      }

      break;
    }

    case KeyRelease: {
      KeySym key =
          XLookupKeysym(&g_x11.event.xkey,
                        0); // `0` is the index for the first keysym in the list
                            // (for non-modified keys), and `1` would be for the
                            // second keysym (for Shift-modified keys), etc.
      switch (key) {
      case XK_x:
      case XK_X: {
        UPDATE_BUTTON(input->rotate_x.button, 0);
        input->rotate_x.value = TETROMINO_ROTATE_X_NONE;
        break;
      }
      case XK_z:
      case XK_Z: {
        UPDATE_BUTTON(input->rotate_x.button, 0);
        input->rotate_x.value = TETROMINO_ROTATE_X_NONE;
        break;
      }
      case XK_Left:
      case XK_A:
      case XK_a: {
        UPDATE_BUTTON(input->move_left.button, 0);
        break;
      }
      case XK_Right:
      case XK_D:
      case XK_d: {
        UPDATE_BUTTON(input->move_right.button, 0);
        break;
      }
      case XK_Down:
      case XK_S:
      case XK_s: {
        UPDATE_BUTTON(input->move_down.button, 0);
        break;
      }
      }

      break;
    }

    /* "User clicked the ✕ button" is sent as a ClientMessage, not a KeyPress.
     */
    case (ClientMessage): {
      g_is_game_running = true;
      break;
    }
    }
  }
}

void platform_render(GameState *state) {

  XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.black);
  XFillRectangle(g_x11.display, g_x11.window, g_x11.gc, 0, 0,
                 FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE);

  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];

      switch (cell_value) {
      case TETRIS_FIELD_EMPTY: {
        continue; // Skip empty cells
      }

      case TETRIS_FIELD_I:
      case TETRIS_FIELD_J:
      case TETRIS_FIELD_L:
      case TETRIS_FIELD_O:
      case TETRIS_FIELD_S:
      case TETRIS_FIELD_T:
      case TETRIS_FIELD_Z: {
        draw_cell(
            col, row,
            get_tetromino_color_by_index(
                cell_value - 1)); // Draw pieces based on their index (subtract
                                  // 1 because field values are piece index + 1)
        break;
      }

      case TETRIS_FIELD_WALL: {
        draw_cell(col, row, g_x11.alloc_colors.gray); // Draw walls in gray
        break;
      }
      }
    }
  }

  draw_piece(state->current_piece.index, state->current_piece.col,
             state->current_piece.row,
             get_tetromino_color_by_index(state->current_piece.index),
             state->current_piece.rotation);

  int font_size = 16;
  int sx = FIELD_WIDTH * CELL_SIZE - font_size * 6;
  int base_pos_y = 95;
  char buf[32];
  XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.white);
  XDrawString(g_x11.display, g_x11.window, g_x11.gc, sx, base_pos_y, "PIECES",
              6);
  XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.yellow);
  snprintf(buf, sizeof(buf), "%d", state->pieces_count);
  XDrawString(g_x11.display, g_x11.window, g_x11.gc, sx, base_pos_y + font_size,
              buf, (int)strlen(buf));

  if (state->game_over) {
    int cx = FIELD_WIDTH * CELL_SIZE / 2;
    int cy = FIELD_HEIGHT * CELL_SIZE / 2;
    /* Dark background box */
    XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.black);
    XFillRectangle(g_x11.display, g_x11.window, g_x11.gc, cx - 60, cy - 30, 120,
                   60);
    /* "GAME OVER" in red */
    XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.red);
    XDrawString(g_x11.display, g_x11.window, g_x11.gc, cx - 28, cy - 8,
                "GAME OVER", 9);
    /* Restart hint in white */
    XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.white);
    XDrawString(g_x11.display, g_x11.window, g_x11.gc, cx - 42, cy + 12,
                "R=Restart  Q=Quit", 17);
  }

  XFlush(g_x11.display);
}

void platform_shutdown(void) {
  XFreeGC(g_x11.display, g_x11.gc);
  XDestroyWindow(g_x11.display, g_x11.window);
  XCloseDisplay(g_x11.display);
}

int main(void) {

  int screen_width = FIELD_WIDTH * CELL_SIZE;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  if (platform_init("Tetris", screen_width, screen_height) != 0) {
    return 1;
  }

  GameInput game_input = {0};
  GameState game_state = {0};
  game_init(&game_state, &game_input);

  while (g_is_game_running) {
    double current_time = platform_get_time();
    double delta_time = current_time - g_last_frame_time;
    g_last_frame_time = current_time;

    prepare_input_frame(&game_input);
    platform_get_input(&game_state, &game_input);
    tetris_update(&game_state, &game_input, (float)delta_time);
    platform_render(&game_state);

    double frame_time = platform_get_time() - current_time;
    if (frame_time < g_target_frame_time) {
      platform_sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
    }
  }

  platform_shutdown();
  return 0;
}