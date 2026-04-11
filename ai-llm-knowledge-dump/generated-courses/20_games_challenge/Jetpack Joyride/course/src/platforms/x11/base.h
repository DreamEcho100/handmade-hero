#define _POSIX_C_SOURCE 200809L

#ifndef PLATFORMS_X11_BASE_H
#define PLATFORMS_X11_BASE_H

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/base.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 02 — X11State struct, Display, Window, GLXContext, texture_id.
 *             Global `g_x11` declaration.
 *
 * LESSON 12 — Audio fields (alsa_pcm, running_sample_index) added.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdbool.h>
#include <stdint.h>

#include <GL/glx.h>
#include <X11/Xlib.h>

/* ─────────────────────────────────────────────────────────────────────────
 * X11State — all X11/GLX window state in one struct.
 *
 * LESSON 02 — Created in this form: Display, Window, GLXContext.
 * LESSON 08 — `window_w` / `window_h` used for letterbox math.
 * LESSON 09 — `vsync_enabled` gate on software frame limiter.
 * ─────────────────────────────────────────────────────────────────────────
 */
typedef struct {
  Display    *display;          /* X11 server connection (XOpenDisplay).    */
  Window      window;           /* The application window (XCreateWindow).  */
  GLXContext  gl_context;       /* OpenGL rendering context (GLXCreateContext). */
  int         screen;           /* Default screen number.                   */

  int         window_w;         /* Current window width  (updated on ConfigureNotify). */
  int         window_h;         /* Current window height (updated on ConfigureNotify). */

  Atom        wm_delete_window; /* WM_DELETE_WINDOW atom (close-button event). */
  bool        vsync_enabled;    /* true if GLX_EXT/MESA swap control succeeded. */
  GLuint      texture_id;       /* GPU texture for the backbuffer upload.   */
} X11State;

extern X11State g_x11;

#endif /* PLATFORMS_X11_BASE_H */
