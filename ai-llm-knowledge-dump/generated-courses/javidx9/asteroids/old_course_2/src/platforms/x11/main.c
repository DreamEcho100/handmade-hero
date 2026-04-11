/* =============================================================================
 * platforms/x11/main.c — Linux Native Platform Backend (X11 + GLX + ALSA)
 * =============================================================================
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * This file implements the Linux platform layer using:
 *   X11  — window creation and keyboard input (Xlib)
 *   GLX  — OpenGL context inside an X11 window
 *   ALSA — PCM audio output
 *
 * It implements platform_init() / platform_shutdown() declared in platform.h,
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
 * PLATFORM CONTRACT (platform.h):
 *   The game calls asteroids_render(state, bb, world_config).
 *   This file owns the backbuffer storage and the world_config origin.
 *   PlatformGameProps bundles them so the platform can pass both at once.
 *
 * FIXES vs the original reference code:
 *   1. XkbKeycodeToKeysym instead of XLookupKeysym (correct key names)
 *   2. XkbSetDetectableAutoRepeat to suppress X11 fake key-repeat events
 *   3. WM_DELETE_WINDOW protocol so window X button triggers clean exit
 *   4. ALSA snd_pcm_sw_params + start_threshold=1 to prevent silent start
 *   5. GL_RGBA (not GL_BGRA) to match GAME_RGBA byte layout [R][G][B][A]
 *   6. inputs[2] double-buffer + prepare_input_frame for "just pressed"
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

/* Game/platform contract */
#include "../../platform.h"

/* ══════ Global Platform State ════════════════════════════════════════════
 *
 * Global variables are acceptable in platform code: there is always exactly
 * one window, one GL context, and one audio device per process.
 * Game logic variables must NEVER be global — use GameState.             */
static Display     *g_display    = NULL;
static Window       g_window     = 0;
static GLXContext   g_gl_ctx     = NULL;
static Atom         g_wm_delete  = 0;       /* WM_DELETE_WINDOW atom      */
static int          g_is_running = 0;
static int          g_win_w      = GAME_W;  /* current window width       */
static int          g_win_h      = GAME_H;  /* current window height      */

static snd_pcm_t   *g_alsa_pcm        = NULL;
static int          g_alsa_period_size = 1024;  /* frames per ALSA write  */

/* Pixel backbuffer storage (platform allocates; game writes).
   Size: GAME_W × GAME_H × 4 bytes per pixel (RGBA).                       */
static uint32_t g_pixel_buf[GAME_W * GAME_H];
static GLuint   g_texture = 0;

/* ══════ platform_init ══════════════════════════════════════════════════════
 *
 * Opens the X11 display, creates a window with an OpenGL context, and
 * initialises the ALSA audio device.  Called ONCE from main() at startup. */
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
        100, 100, GAME_W, GAME_H, 0,
        vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa);

    XStoreName(g_display, g_window, "Asteroids");
    XMapWindow(g_display, g_window);

    /* ── Register WM_DELETE_WINDOW protocol ─────────────────────────────
       Without this, clicking the window X button sends DestroyNotify,
       killing the process and leaving ALSA in an undefined state.
       With WM_DELETE_WINDOW we get a ClientMessage and can clean up.    */
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

    /* Allocate GPU texture storage once.  Pixels are uploaded each frame
       via glTexSubImage2D.
       GL_RGBA byte order: [R][G][B][A] — matches GAME_RGBA exactly.
       *** Using GL_BGRA here would swap red and blue → cyan asteroids! *** */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 GAME_W, GAME_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);

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
        snd_pcm_hw_params_set_format(g_alsa_pcm, hw, SND_PCM_FORMAT_S16_LE);
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
           Without start_threshold=1, ALSA waits for the buffer to fill to
           its default threshold before starting playback → first second is
           silent.  Fix: load defaults, override threshold, apply.         */
        snd_pcm_sw_params_t *sw;
        snd_pcm_sw_params_alloca(&sw);
        snd_pcm_sw_params_current(g_alsa_pcm, sw);
        snd_pcm_sw_params_set_start_threshold(g_alsa_pcm, sw, 1);
        snd_pcm_sw_params(g_alsa_pcm, sw);
    }

    g_is_running = 1;
}

/* ══════ platform_shutdown ════════════════════════════════════════════════
 *
 * Release all OS resources.  Must be called before exit.                 */
void platform_shutdown(void) {
    if (g_alsa_pcm) {
        snd_pcm_drain(g_alsa_pcm);
        snd_pcm_close(g_alsa_pcm);
        g_alsa_pcm = NULL;
    }
    if (g_gl_ctx) {
        glXMakeCurrent(g_display, None, NULL);
        glXDestroyContext(g_display, g_gl_ctx);
        g_gl_ctx = NULL;
    }
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
 *
 * LESSON 06 — Double-buffered input.
 * prepare_input_frame() carries ended_down and zeros half_transition_count.
 * process_events() then fires state changes into the new frame.
 *
 * XkbKeycodeToKeysym(display, keycode, group=0, level=0) returns the
 * logical key name, correct even when keyboard layout remaps keys.
 *
 * half_transition_count records how many press/release transitions
 * occurred this frame — usually 0 or 1, but can be 2+ on rapid tap.    */
static void process_events(GameInput *input) {
    while (XPending(g_display) > 0) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {

            /* ── WM_DELETE_WINDOW: user clicked the window X button ───── */
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
 *
 * Uploads g_pixel_buf[] to the OpenGL texture, computes a centered
 * letterbox viewport, then draws a full-screen textured quad.
 *
 * platform_compute_letterbox() (from platform.h) handles the math:
 *   scale = min(win_w / GAME_W, win_h / GAME_H)
 *   vw = GAME_W * scale, vh = GAME_H * scale
 *   vx = (win_w - vw)/2, vy = (win_h - vh)/2
 *
 * GL_RGBA byte layout must match GAME_RGBA ([R][G][B][A] in memory).
 * *** Using GL_BGRA would swap red and blue → cyan asteroids! ***         */
static void display_backbuffer(void) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0, GAME_W, GAME_H,
                    GL_RGBA, GL_UNSIGNED_BYTE,
                    g_pixel_buf);

    int vx, vy, vw, vh;
    platform_compute_letterbox(GAME_W, GAME_H, g_win_w, g_win_h,
                                &vx, &vy, &vw, &vh);

    /* Clear entire window to black (fills letterbox bars) */
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
 *
 * The game loop:
 *   1. Measure frame time (dt)
 *   2. Prepare input double buffer (carry held state, reset transitions)
 *   3. Process X11 events (keyboard)
 *   4. Update game simulation
 *   5. Render into pixel backbuffer
 *   6. Output audio to ALSA
 *   7. Upload pixels to GPU + swap
 *   8. Repeat                                                             */
int main(void) {
    /* ── Initialise platform (window + audio) ───────────────────────── */
    platform_init();

    /* ── Platform/game props ─────────────────────────────────────────── */
    PlatformGameProps props;
    platform_game_props_init(&props);

    /* Point the backbuffer at our static pixel storage */
    props.backbuffer.pixels          = g_pixel_buf;
    props.backbuffer.width           = GAME_W;
    props.backbuffer.height          = GAME_H;
    props.backbuffer.pitch           = GAME_W * 4;
    props.backbuffer.bytes_per_pixel = 4;

    /* ── Initialise game state on the stack ─────────────────────────── */
    GameState state;
    memset(&state, 0, sizeof(state));
    state.audio.samples_per_second = 44100;
    game_audio_init(&state);
    asteroids_init(&state);

    /* ── Audio output buffer ─────────────────────────────────────────── */
    int16_t audio_samples[4096 * 2];  /* period_size × channels */
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
        if (dt > 0.066) dt = 0.066;
        if (dt < 0.0)   dt = 0.0;

        /* ── Input: prepare double buffer ────────────────────────────── */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);

        /* ── Input: read X11 events into inputs[1] ───────────────────── */
        process_events(&inputs[1]);
        if (inputs[1].quit) break;

        /* ── Update game simulation ───────────────────────────────────── */
        asteroids_update(&state, &inputs[1], (float)dt);

        /* ── Render into pixel backbuffer ────────────────────────────── */
        asteroids_render(&state, &props.backbuffer, props.world_config);

        /* ── Audio ───────────────────────────────────────────────────── */
        if (g_alsa_pcm) {
            game_get_audio_samples(&state, &audio_buf);
            int rc = snd_pcm_writei(g_alsa_pcm, audio_samples,
                                    (snd_pcm_uframes_t)g_alsa_period_size);
            if (rc == -EPIPE) {
                /* Buffer underrun: ALSA ran out of data.  Recover. */
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
