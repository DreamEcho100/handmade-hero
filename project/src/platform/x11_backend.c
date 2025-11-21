#include "../game/game.h"
#include "platform.h"
#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>

int platform_main() {
  int width = 800, height = 600;

  Display *d = XOpenDisplay(NULL);
  Window root = DefaultRootWindow(d);

  Window win = XCreateSimpleWindow(d, root, 0, 0, width, height, 0,
                                   BlackPixel(d, 0), WhitePixel(d, 0));

  XSelectInput(d, win, ExposureMask | KeyPressMask);
  XMapWindow(d, win);

  GC gc = XCreateGC(d, win, 0, NULL);

  uint32_t *buffer = malloc(width * height * sizeof(uint32_t));
  XImage *img = XCreateImage(d, DefaultVisual(d, 0), 24, ZPixmap, 0,
                             (char *)buffer, width, height, 32, 0);

  GameState state = {0};

  while (1) {
    while (XPending(d)) {
      XEvent e;
      XNextEvent(d, &e);

      if (e.type == KeyPress)
        return 0;
    }

    game_update(&state, buffer, width, height);

    XPutImage(d, win, gc, img, 0, 0, 0, 0, width, height);
  }

  return 0;
}
