/* =============================================================================
 * main_x11.c — Linux Platform Layer (X11 + GLX + ALSA)
 * =============================================================================
 *
 * This file implements the three platform functions declared in platform.h:
 *   platform_init()      — open X11 window + GL context + ALSA device
 *   platform_shutdown()  — drain/close ALSA, destroy GL context + window
 *
 * and contains main() which drives the game loop.
 *
 * WHAT THIS FILE KNOWS ABOUT:
 *   X11  — window system (Xlib)
 *   GLX  — OpenGL context creation within an X11 window
 *   ALSA — Advanced Linux Sound Architecture (audio output)
 *
 * WHAT THIS FILE MUST NOT KNOW ABOUT:
 *   Any game logic — it only calls asteroids_init/update/render/audio via
 *   the platform.h contract.  The game code never #includes X11.h.
 *
 * COURSE NOTES (bugs fixed vs reference asteroids source):
 *   1. XkbKeycodeToKeysym instead of XLookupKeysym (correct key names)
 *   2. XkbSetDetectableAutoRepeat to suppress X11 fake key-repeat events
 *   3. WM_DELETE_WINDOW protocol so window X button triggers clean exit
 *   4. ALSA snd_pcm_sw_params + start_threshold=1 to prevent silent start
 *   5. GL_RGBA (not GL_BGRA) to match GAME_RGBA byte layout [R][G][B][A]
 *   6. inputs[2] double-buffer + prepare_input_frame for correct "just pressed"
 *   7. platform_shutdown() for clean teardown (no hanging terminal)
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* X11 / GLX / OpenGL */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>   /* XkbKeycodeToKeysym, XkbSetDetectableAutoRepeat */
#include <GL/gl.h>
#include <GL/glx.h>

/* ALSA */
#include <alsa/asoundlib.h>

/* Game contract */
#include "platform.h"

/* ══════ Global Platform State ════════════════════════════════════════════

   Global variables are acceptable in platform code: there is always exactly
   one window, one GL context, and one audio device per process.
   Game logic variables must NEVER be global (use GameState).             */
static Display     *g_display   = NULL;
static Window       g_window    = 0;
static GLXContext   g_gl_ctx    = NULL;
static Atom         g_wm_delete = 0;     /* WM_DELETE_WINDOW atom */
static int          g_is_running = 0;
static int          g_win_w = SCREEN_W;  /* current window width  */
static int          g_win_h = SCREEN_H;  /* current window height */

static snd_pcm_t   *g_alsa_pcm = NULL;
static int          g_alsa_period_size = 1024;  /* frames per ALSA write */

/* Pixel backbuffer storage (platform allocates; game writes) */
static uint32_t g_pixel_buf[SCREEN_W * SCREEN_H];
static GLuint   g_texture = 0;

/* ══════ platform_init ══════════════════════════════════════════════════════

   Opens the X11 display, creates a window with an OpenGL context, and
   initialises the ALSA audio device.                                      */
void platform_init(void) {

    /* ── Open X11 display ────────────────────────────────────────────── */
    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        fprintf(stderr, "Error: cannot open X11 display\n");
        exit(1);
    }
    int screen = DefaultScreen(g_display);

    /* ── Choose a GLX visual ─────────────────────────────────────────── */
    static int attr[] = {
        GLX_RGBA, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE,   8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE,  8,
        None
    };
    XVisualInfo *vi = glXChooseVisual(g_display, screen, attr);
    if (!vi) {
        fprintf(stderr, "Error: glXChooseVisual failed\n");
        exit(1);
    }

    /* ── Create window ───────────────────────────────────────────────── */
    Colormap cmap = XCreateColormap(g_display,
                                    RootWindow(g_display, vi->screen),
                                    vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap   = cmap;
    swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;

    g_window = XCreateWindow(
        g_display, RootWindow(g_display, vi->screen),
        100, 100, SCREEN_W, SCREEN_H, 0,
        vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa);

    XStoreName(g_display, g_window, "Asteroids");
    XMapWindow(g_display, g_window);

    /* ── Register WM_DELETE_WINDOW protocol ─────────────────────────────
       Without this, clicking the window's X button would send a
       DestroyNotify event that kills the process immediately, leaving
       ALSA in an undefined state and hanging the terminal.
       With WM_DELETE_WINDOW, we receive a ClientMessage event instead
       and can do clean shutdown.                                         */
    g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);

    /* ── Suppress X11 fake key-repeat events ─────────────────────────── */
    XkbSetDetectableAutoRepeat(g_display, True, NULL);

    /* ── Create GLX context and OpenGL texture ───────────────────────── */
    g_gl_ctx = glXCreateContext(g_display, vi, NULL, GL_TRUE);
    glXMakeCurrent(g_display, g_window, g_gl_ctx);
    XFree(vi);

    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* Allocate texture storage once (pixels filled per frame) */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 SCREEN_W, SCREEN_H, 0,
                 GL_RGBA,           /* MUST match GAME_RGBA byte layout: [R][G][B][A] */
                 GL_UNSIGNED_BYTE, NULL);

    /* ── ALSA audio setup ─────────────────────────────────────────────── */
    int err = snd_pcm_open(&g_alsa_pcm, "default",
                           SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Warning: ALSA open failed: %s (audio disabled)\n",
                snd_strerror(err));
        g_alsa_pcm = NULL;
    } else {
        /* Hardware parameters */
        snd_pcm_hw_params_t *hw;
        snd_pcm_hw_params_alloca(&hw);
        snd_pcm_hw_params_any(g_alsa_pcm, hw);
        snd_pcm_hw_params_set_access(g_alsa_pcm, hw,
                                     SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(g_alsa_pcm, hw,
                                     SND_PCM_FORMAT_S16_LE);
        unsigned int rate = 44100;
        snd_pcm_hw_params_set_rate_near(g_alsa_pcm, hw, &rate, NULL);
        snd_pcm_hw_params_set_channels(g_alsa_pcm, hw, 2);

        snd_pcm_uframes_t buf_size = 4096;
        snd_pcm_hw_params_set_buffer_size_near(g_alsa_pcm, hw, &buf_size);
        snd_pcm_uframes_t period = (snd_pcm_uframes_t)g_alsa_period_size;
        snd_pcm_hw_params_set_period_size_near(g_alsa_pcm, hw, &period, NULL);
        g_alsa_period_size = (int)period;
        snd_pcm_hw_params(g_alsa_pcm, hw);

        /* Software parameters — CRITICAL FIX:
           Without setting start_threshold to 1, ALSA waits for the buffer
           to fill to its default threshold before starting playback.  This
           causes the first second of audio to be silent.
           Fix: load current sw_params, override start_threshold, then apply. */
        snd_pcm_sw_params_t *sw;
        snd_pcm_sw_params_alloca(&sw);
        snd_pcm_sw_params_current(g_alsa_pcm, sw);   /* load defaults first */
        snd_pcm_sw_params_set_start_threshold(g_alsa_pcm, sw, 1);
        snd_pcm_sw_params(g_alsa_pcm, sw);            /* apply              */
    }

    g_is_running = 1;
}

/* ══════ platform_shutdown ════════════════════════════════════════════════
   Release all OS resources.  Must be called before exit.                  */
void platform_shutdown(void) {
    /* Drain and close ALSA so audio hardware is left in a clean state */
    if (g_alsa_pcm) {
        snd_pcm_drain(g_alsa_pcm);
        snd_pcm_close(g_alsa_pcm);
        g_alsa_pcm = NULL;
    }

    /* Destroy GL context */
    if (g_gl_ctx) {
        glXMakeCurrent(g_display, None, NULL);
        glXDestroyContext(g_display, g_gl_ctx);
        g_gl_ctx = NULL;
    }

    /* Destroy window and close display */
    if (g_window) {
        XDestroyWindow(g_display, g_window);
        g_window = 0;
    }
    if (g_display) {
        XCloseDisplay(g_display);
        g_display = NULL;
    }
}

/* ══════ process_events — read pending X11 events ══════════════════════════

   Fills or updates `input` based on keyboard events received from X11.
   Returns 0 to keep running, non-zero to quit.

   KEY HANDLING:
   ─────────────
   XkbKeycodeToKeysym(display, keycode, group=0, level=0) returns the
   keysym (logical key name) from a hardware keycode.  This is correct
   even when the keyboard layout remaps keys.

   half_transition_count counts how many times this key was pressed or
   released this frame.  For normal frame rates, this is usually 0 or 1,
   but during fast input it can be 2+ (e.g. rapid tap-fire).             */
static void process_events(GameInput *input) {
    while (XPending(g_display) > 0) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {

            /* ── WM_DELETE_WINDOW: user clicked the X button ──────────── */
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == g_wm_delete) {
                    input->quit = 1;
                    g_is_running = 0;
                }
                break;

            /* ── Window resize ───────────────────────────────────────── */
            case ConfigureNotify:
                g_win_w = event.xconfigure.width;
                g_win_h = event.xconfigure.height;
                break;

            /* ── Key press / release ─────────────────────────────────── */
            case KeyPress:
            case KeyRelease: {
                int is_press = (event.type == KeyPress);
                KeySym sym = XkbKeycodeToKeysym(g_display,
                                                event.xkey.keycode, 0, 0);

                /* Map keysym to our GameButtonState */
                GameButtonState *btn = NULL;
                switch (sym) {
                    case XK_Left:  case XK_a: case XK_A:
                        btn = &input->left;   break;
                    case XK_Right: case XK_d: case XK_D:
                        btn = &input->right;  break;
                    case XK_Up:    case XK_w: case XK_W:
                        btn = &input->up;     break;
                    case XK_space: case XK_Return:
                        btn = &input->fire;   break;
                    case XK_Escape: case XK_q: case XK_Q:
                        input->quit = 1;
                        g_is_running = 0;
                        break;
                    default:
                        break;
                }

                if (btn) {
                    /* Only record a transition if the state actually changed */
                    if (btn->ended_down != is_press) {
                        btn->ended_down = is_press;
                        btn->half_transition_count++;
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

/* ══════ display_backbuffer — upload pixels to GPU and blit ════════════════

   Uploads our software pixel array to the OpenGL texture, then draws a
   full-screen textured quad using the fixed-function pipeline.

   When the window is resized, the game canvas (SCREEN_W × SCREEN_H) is
   scaled uniformly to fit and centered with black letterbox bars.

   GL_RGBA matches GAME_RGBA byte layout in memory [R][G][B][A]:
     memory byte 0 = R, 1 = G, 2 = B, 3 = A.
   OpenGL's GL_RGBA format reads bytes in R,G,B,A order — an exact match.
   *** Using GL_BGRA here would swap red and blue, causing cyan asteroids! *** */
static void display_backbuffer(void) {
    glBindTexture(GL_TEXTURE_2D, g_texture);

    /* Upload new pixel data */
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0, SCREEN_W, SCREEN_H,
                    GL_RGBA,            /* byte order: R G B A */
                    GL_UNSIGNED_BYTE,
                    g_pixel_buf);

    /* ── Compute letterbox viewport ──────────────────────────────────────
       Scale the game canvas to fit inside the current window while
       maintaining the original aspect ratio.  Centre the result.          */
    float scale_x = (float)g_win_w / (float)SCREEN_W;
    float scale_y = (float)g_win_h / (float)SCREEN_H;
    float scale   = (scale_x < scale_y) ? scale_x : scale_y;
    int   vw = (int)((float)SCREEN_W * scale);
    int   vh = (int)((float)SCREEN_H * scale);
    int   vx = (g_win_w - vw) / 2;
    int   vy = (g_win_h - vh) / 2;

    /* Clear entire window black (fills letterbox bars) */
    glViewport(0, 0, g_win_w, g_win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Draw game canvas in the centered viewport */
    glViewport(vx, vy, vw, vh);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glXSwapBuffers(g_display, g_window);
}

/* ══════ main ═══════════════════════════════════════════════════════════════

   The game loop:
     1. Measure frame time (dt)
     2. Process OS events (keyboard)
     3. Prepare input double buffer
     4. Update game simulation
     5. Render into pixel backbuffer
     6. Output audio
     7. Upload pixels to GPU + swap
     8. Repeat                                                             */
int main(void) {
    /* ── Initialise platform (window + audio) ───────────────────────── */
    platform_init();

    /* ── Initialise game state on the stack ─────────────────────────── */
    GameState state;
    memset(&state, 0, sizeof(state));
    state.audio.samples_per_second = 44100;
    game_audio_init(&state);
    asteroids_init(&state);

    /* ── Pixel backbuffer (written by game, read by display) ─────────── */
    AsteroidsBackbuffer bb;
    bb.pixels = g_pixel_buf;
    bb.width  = SCREEN_W;
    bb.height = SCREEN_H;
    bb.pitch  = SCREEN_W * 4;  /* bytes per row = 4 bytes per pixel × width */

    /* ── Audio output buffer ─────────────────────────────────────────── */
    int16_t audio_samples[4096 * 2];  /* period_size * channels */
    AudioOutputBuffer audio_buf;
    audio_buf.samples            = audio_samples;
    audio_buf.samples_per_second = 44100;
    audio_buf.sample_count       = g_alsa_period_size;

    /* ── Input double-buffer ─────────────────────────────────────────── */
    GameInput inputs[2];
    memset(inputs, 0, sizeof(inputs));

    /* ── Frame timing ────────────────────────────────────────────────── */
    struct timespec ts_last, ts_now;
    clock_gettime(CLOCK_MONOTONIC, &ts_last);

    /* ══════ Main Game Loop ════════════════════════════════════════════ */
    while (g_is_running) {
        /* ── Compute dt ──────────────────────────────────────────────── */
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        double dt = (ts_now.tv_sec  - ts_last.tv_sec) +
                    (ts_now.tv_nsec - ts_last.tv_nsec) * 1e-9;
        ts_last = ts_now;
        /* Clamp dt: if the window was minimised or the system was slow,
           a huge dt would make objects teleport.  Cap at ~4 frames.     */
        if (dt > 0.066) dt = 0.066;
        if (dt < 0.0)   dt = 0.0;

        /* ── Input: prepare double buffer ────────────────────────────── */
        /* Swap: inputs[0] ← inputs[1] (old = last frame's state)
                 inputs[1] ← inputs[0] (current = ready to overwrite)    */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);

        /* ── Input: read X11 events into inputs[1] ───────────────────── */
        process_events(&inputs[1]);

        if (inputs[1].quit) break;

        /* ── Update game simulation ───────────────────────────────────── */
        asteroids_update(&state, &inputs[1], (float)dt);

        /* ── Render into pixel backbuffer ────────────────────────────── */
        asteroids_render(&state, &bb);

        /* ── Audio ───────────────────────────────────────────────────── */
        if (g_alsa_pcm) {
            game_get_audio_samples(&state, &audio_buf);
            int rc = snd_pcm_writei(g_alsa_pcm, audio_samples,
                                    (snd_pcm_uframes_t)g_alsa_period_size);
            if (rc == -EPIPE) {
                /* Buffer underrun: ALSA ran out of data.  Recover and continue. */
                snd_pcm_prepare(g_alsa_pcm);
            }
        }

        /* ── Display ─────────────────────────────────────────────────── */
        display_backbuffer();
    }
    /* ══════ End of Game Loop ══════════════════════════════════════════ */

    platform_shutdown();
    return 0;
}
