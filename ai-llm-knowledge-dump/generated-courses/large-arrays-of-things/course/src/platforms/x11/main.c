/* Enable POSIX APIs: clock_gettime, nanosleep, struct timespec. */
#define _POSIX_C_SOURCE 199309L

/* ══════════════════════════════════════════════════════════════════════ */
/*               main.c — X11/GLX Backend for LOATs Demo                 */
/*                                                                       */
/*  Minimal X11 backend: opens a window, uploads CPU pixels via OpenGL   */
/*  texture, handles keyboard input, runs the game loop.                 */
/*                                                                       */
/*  This is a SIMPLIFIED version of the Asteroids/Platform Foundation    */
/*  X11 backend — just enough for the LOATs demo.                        */
/* ══════════════════════════════════════════════════════════════════════ */
#include "../../platform.h"

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════ */
/*                   Timing                                */
/* ══════════════════════════════════════════════════════ */

static double get_time_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ══════════════════════════════════════════════════════ */
/*                     Main                                */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    /* ── Open X11 display and create window ── */
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open X11 display\n"); return 1; }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    /* GLX visual for OpenGL context. */
    int visual_attribs[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None
    };
    XVisualInfo *vi = glXChooseVisual(dpy, screen, visual_attribs);
    if (!vi) { fprintf(stderr, "No suitable GLX visual\n"); return 1; }

    Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    XSetWindowAttributes swa = {0};
    swa.colormap = cmap;
    swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;

    Window win = XCreateWindow(dpy, root, 0, 0, SCREEN_W, SCREEN_H, 0,
                               vi->depth, InputOutput, vi->visual,
                               CWColormap | CWEventMask, &swa);
    XStoreName(dpy, win, GAME_TITLE);
    XMapWindow(dpy, win);

    /* WM_DELETE_WINDOW protocol — so clicking X button works. */
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    /* Disable X11 auto-repeat to avoid phantom key releases. */
    XkbSetDetectableAutoRepeat(dpy, True, NULL);

    /* ── GLX context ── */
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    XFree(vi);

    /* ── OpenGL texture for pixel upload ── */
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCREEN_W, SCREEN_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    /* ── Pixel buffer ── */
    uint32_t *pixels = (uint32_t *)malloc(SCREEN_W * SCREEN_H * sizeof(uint32_t));
    if (!pixels) return 1;
    memset(pixels, 0, SCREEN_W * SCREEN_H * sizeof(uint32_t));

    /* ── Game state ── */
    game_state state;
    memset(&state, 0, sizeof(state));
    state.current_scene = 8; /* default scene */
    game_init(&state);

    /* ── Input state ── */
    game_input input = {0};

    /* ── Main loop ── */
    bool running = true;
    double prev_time = get_time_sec();

    while (running) {
        /* Reset one-shot inputs each frame */
        input.space = false;
        input.scene_key = 0;

        /* ── Process X11 events ── */
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            if (ev.type == ClientMessage &&
                (Atom)ev.xclient.data.l[0] == wm_delete) {
                running = false;
                break;
            }

            if (ev.type == KeyPress || ev.type == KeyRelease) {
                KeySym sym = XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0);
                bool down = (ev.type == KeyPress);

                switch (sym) {
                    case XK_Left:  case XK_a: input.left  = down; break;
                    case XK_Right: case XK_d: input.right = down; break;
                    case XK_Up:    case XK_w: input.up    = down; break;
                    case XK_Down:  case XK_s: input.down  = down; break;
                    case XK_space:
                        if (down) input.space = true;
                        break;
                    case XK_1: case XK_2: case XK_3: case XK_4: case XK_5:
                    case XK_6: case XK_7: case XK_8: case XK_9:
                        if (down) input.scene_key = sym - XK_1 + 1;
                        break;
                    /* Lab keys — always available */
                    case XK_p:
                        if (down) input.scene_key = 10; /* Particle Lab */
                        break;
                    case XK_l:
                        if (down) input.scene_key = 11; /* Data Layout Lab */
                        break;
                    case XK_g:
                        if (down) input.scene_key = 12; /* Spatial Partition Lab */
                        break;
                    case XK_m:
                        if (down) input.scene_key = 13; /* Memory Arena Lab */
                        break;
                    case XK_i:
                        if (down) input.scene_key = 14; /* Infinite Growth Lab */
                        break;
                    case XK_Tab:
                        if (down) input.tab_pressed = true; /* cycle layout */
                        break;
                    case XK_r:
                        if (down) game_init(&state);
                        break;
                    case XK_Escape: case XK_q:
                        running = false;
                        break;
                    default: break;
                }
            }
        }

        /* ── Delta time ── */
        double now = get_time_sec();
        float dt = (float)(now - prev_time);
        prev_time = now;
        if (dt > 0.1f) dt = 0.1f;

        /* ── Update ── */
        game_update(&state, &input, dt);

        /* ── Render to CPU pixel buffer ── */
        game_render(&state, pixels, SCREEN_W, SCREEN_H);

        /* ── Upload pixels to GPU and draw fullscreen quad ── */
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(-1,  1);
            glTexCoord2f(1, 0); glVertex2f( 1,  1);
            glTexCoord2f(1, 1); glVertex2f( 1, -1);
            glTexCoord2f(0, 1); glVertex2f(-1, -1);
        glEnd();

        glXSwapBuffers(dpy, win);

        /* HUD text is rendered into the pixel buffer by game_render
         * using the FONT_8X8 bitmap font. Both backends show identical output. */

        /* ── Simple frame limiter (~60 FPS) ── */
        struct timespec sleep_ts = {0, 16000000}; /* ~16ms */
        nanosleep(&sleep_ts, NULL);
    }

    /* ── Cleanup ── */
    glDeleteTextures(1, &tex);
    free(pixels);
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    return 0;
}
