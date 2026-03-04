/* =============================================================================
 * main_x11.c — Linux Platform Layer (X11 + GLX + ALSA)
 * =============================================================================
 *
 * This file implements the four platform functions declared in platform.h:
 *   platform_init()      — open X11 window + GL context + ALSA device
 *   platform_get_time()  — monotonic clock in seconds
 *   platform_get_input() — read X11 keyboard events → GameInput
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
 *   Any game logic — it only calls game_init/update/render/audio via
 *   the platform.h contract.  Game code never #includes X11.h.
 *
 * COURSE NOTES (bugs fixed vs reference frogger source):
 *   1. XkbKeycodeToKeysym instead of XLookupKeysym (correct key names)
 *   2. XkbSetDetectableAutoRepeat to suppress fake key-repeat events
 *   3. WM_DELETE_WINDOW protocol so window X button triggers clean exit
 *   4. ALSA snd_pcm_sw_params + start_threshold=1 to prevent silent start
 *   5. GL_RGBA (not GL_BGRA) to match GAME_RGBA byte layout [R][G][B][A]
 *   6. inputs[2] double-buffer + prepare_input_frame for "just pressed"
 *   7. platform_shutdown() for clean teardown (no hanging terminal)
 *   8. Letterbox centering on ConfigureNotify (window resize) via two
 *      glViewport calls: clear the whole window black, then draw the
 *      game canvas in a centred viewport.
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alloca.h>  /* snd_pcm_hw_params_alloca expands to alloca() */

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
   Game state must NEVER be global (use GameState on the stack in main).  */
static Display     *g_display    = NULL;
static Window       g_window     = 0;
static GLXContext   g_gl_ctx     = NULL;
static Atom         g_wm_delete  = 0;       /* WM_DELETE_WINDOW atom */
static int          g_win_w      = SCREEN_W;
static int          g_win_h      = SCREEN_H;

static snd_pcm_t   *g_alsa_pcm  = NULL;
static int          g_alsa_period_size = 1024; /* ALSA write size in frames */

/* Pixel backbuffer storage — game renders here; display_backbuffer() blits it */
static uint32_t     g_pixel_buf[SCREEN_W * SCREEN_H];
static GLuint       g_texture   = 0;

/* ══════ platform_init ══════════════════════════════════════════════════════

   Opens the X11 display, creates a window with an OpenGL context, and
   initialises the ALSA audio device for 44100 Hz / 16-bit / stereo output. */
int platform_init(PlatformGameProps *props) {

    /* ── Open X11 display ────────────────────────────────────────────── */
    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        fprintf(stderr, "Error: cannot open X11 display\n");
        return 0;
    }

    /* ── Choose a GLX visual ─────────────────────────────────────────── */
    int attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE,   8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE,  8,
        None
    };
    int screen = DefaultScreen(g_display);
    XVisualInfo *vi = glXChooseVisual(g_display, screen, attribs);
    if (!vi) {
        fprintf(stderr, "Error: no suitable GLX visual\n");
        return 0;
    }

    /* ── Create window ───────────────────────────────────────────────── */
    XSetWindowAttributes swa;
    swa.colormap   = XCreateColormap(g_display,
                                     RootWindow(g_display, vi->screen),
                                     vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask
                   | StructureNotifyMask;  /* for ConfigureNotify resize */
    g_window = XCreateWindow(
        g_display,
        RootWindow(g_display, vi->screen),
        0, 0, (unsigned)props->width, (unsigned)props->height, 0,
        vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa);

    XStoreName(g_display, g_window, props->title);

    /* Register WM_DELETE_WINDOW so clicking X triggers ClientMessage */
    g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);

    /* Suppress X11 auto-repeat key events (they produce spurious KeyRelease
       + KeyPress pairs at the keyboard repeat rate, corrupting half_transition_count) */
    XkbSetDetectableAutoRepeat(g_display, True, NULL);

    XMapWindow(g_display, g_window);
    XFlush(g_display);

    /* ── Create GL context ───────────────────────────────────────────── */
    g_gl_ctx = glXCreateContext(g_display, vi, NULL, GL_TRUE);
    glXMakeCurrent(g_display, g_window, g_gl_ctx);
    XFree(vi);

    /* ── Allocate and configure the backbuffer texture ───────────────── */
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    /* No mipmaps; NEAREST filter to keep pixel art crisp */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    /* Allocate GPU storage for the full game canvas */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 SCREEN_W, SCREEN_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 g_pixel_buf);

    /* ── ALSA audio setup ─────────────────────────────────────────────── */
    {
        unsigned int rate = 44100;
        int err = snd_pcm_open(&g_alsa_pcm, "default",
                               SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            fprintf(stderr, "Warning: ALSA open failed: %s (audio disabled)\n",
                    snd_strerror(err));
            g_alsa_pcm = NULL;
        } else {
            snd_pcm_hw_params_t *hw;
            snd_pcm_hw_params_alloca(&hw);
            snd_pcm_hw_params_any(g_alsa_pcm, hw);
            snd_pcm_hw_params_set_access(g_alsa_pcm, hw,
                                         SND_PCM_ACCESS_RW_INTERLEAVED);
            snd_pcm_hw_params_set_format(g_alsa_pcm, hw,
                                         SND_PCM_FORMAT_S16_LE);
            snd_pcm_hw_params_set_rate_near(g_alsa_pcm, hw, &rate, NULL);
            snd_pcm_hw_params_set_channels(g_alsa_pcm, hw, 2);

            snd_pcm_uframes_t buf_size = (snd_pcm_uframes_t)(g_alsa_period_size * 4);
            snd_pcm_hw_params_set_buffer_size_near(g_alsa_pcm, hw, &buf_size);
            snd_pcm_uframes_t period = (snd_pcm_uframes_t)g_alsa_period_size;
            snd_pcm_hw_params_set_period_size_near(g_alsa_pcm, hw, &period, NULL);
            g_alsa_period_size = (int)period;
            snd_pcm_hw_params(g_alsa_pcm, hw);

            /* start_threshold = 1: ALSA starts playback as soon as the first
               frame is written, not after the buffer is full.  Without this
               the audio device is silent for one buffer period (~92 ms).   */
            snd_pcm_sw_params_t *sw;
            snd_pcm_sw_params_alloca(&sw);
            snd_pcm_sw_params_current(g_alsa_pcm, sw);
            snd_pcm_sw_params_set_start_threshold(g_alsa_pcm, sw, 1);
            snd_pcm_sw_params(g_alsa_pcm, sw);
        }
    }

    /* ── Fill in backbuffer pointer for the game ─────────────────────── */
    props->backbuffer.pixels = g_pixel_buf;
    props->backbuffer.width  = SCREEN_W;
    props->backbuffer.height = SCREEN_H;
    props->backbuffer.pitch  = SCREEN_W * 4;
    props->is_running        = 1;

    return 1;
}

/* ══════ platform_get_time ══════════════════════════════════════════════════ */
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ══════ platform_get_input ═════════════════════════════════════════════════

   Drains all pending X11 events into *input.

   KEY HANDLING:
     XkbKeycodeToKeysym returns the logical key name from a hardware keycode —
     correct even when the keyboard layout remaps keys.

     half_transition_count counts transitions this frame.  Normally 0 or 1;
     can be 2+ for rapid tap-fire input at high frame rates.              */
void platform_get_input(GameInput *input, PlatformGameProps *props) {
    while (XPending(g_display) > 0) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {

            /* ── WM_DELETE_WINDOW ────────────────────────────────────── */
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == g_wm_delete) {
                    input->quit = 1;
                    props->is_running = 0;
                }
                break;

            /* ── Window resize → update letterbox geometry ───────────── */
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
                        btn = &input->move_left;   break;
                    case XK_Right: case XK_d: case XK_D:
                        btn = &input->move_right;  break;
                    case XK_Up:    case XK_w: case XK_W:
                        btn = &input->move_up;     break;
                    case XK_Down:  case XK_s: case XK_S:
                        btn = &input->move_down;   break;
                    case XK_r:    case XK_R:
                        if (event.type == KeyPress) input->restart = 1;
                        break;
                    case XK_Escape: case XK_q: case XK_Q:
                        input->quit = 1;
                        props->is_running = 0;
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

/* ══════ platform_shutdown ══════════════════════════════════════════════════ */
void platform_shutdown(void) {
    if (g_alsa_pcm) {
        snd_pcm_drain(g_alsa_pcm);
        snd_pcm_close(g_alsa_pcm);
        g_alsa_pcm = NULL;
    }
    if (g_texture) {
        glDeleteTextures(1, &g_texture);
        g_texture = 0;
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

/* ══════ display_backbuffer — upload pixels to GPU and blit ════════════════

   Uploads the software pixel array to the OpenGL texture, then draws a
   textured quad.  When the window is resized the game canvas is scaled
   uniformly and centred with black letterbox bars.

   GL_RGBA matches GAME_RGBA byte layout [R][G][B][A]:
     memory byte 0 = R, 1 = G, 2 = B, 3 = A — an exact match for GL_RGBA.
   Using GL_BGRA would swap red and blue, making all sprites the wrong colour.

   LETTERBOX ALGORITHM:
     scale_x = win_w / SCREEN_W      (how much we'd scale to fill width)
     scale_y = win_h / SCREEN_H      (how much we'd scale to fill height)
     scale   = min(scale_x, scale_y) (uniform scale: whichever axis is tighter)
     viewport = scale * game canvas, centred in window                    */
static void display_backbuffer(void) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0, SCREEN_W, SCREEN_H,
                    GL_RGBA, GL_UNSIGNED_BYTE,
                    g_pixel_buf);

    float scale_x = (float)g_win_w / (float)SCREEN_W;
    float scale_y = (float)g_win_h / (float)SCREEN_H;
    float scale   = (scale_x < scale_y) ? scale_x : scale_y;
    int   vw = (int)((float)SCREEN_W * scale);
    int   vh = (int)((float)SCREEN_H * scale);
    int   vx = (g_win_w - vw) / 2;
    int   vy = (g_win_h - vh) / 2;

    /* Clear entire window (fills letterbox bars) */
    glViewport(0, 0, g_win_w, g_win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Draw game canvas centred */
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
     2. Prepare input double-buffer
     3. Poll X11 events (keyboard)
     4. Update game simulation
     5. Render into pixel backbuffer
     6. Output audio via ALSA
     7. Upload pixels to GPU + swap
     8. Repeat                                                             */
int main(void) {
    /* ── Initialise platform ─────────────────────────────────────────── */
    PlatformGameProps props;
    memset(&props, 0, sizeof(props));
    props.title  = "Frogger";
    props.width  = SCREEN_W;
    props.height = SCREEN_H;

    if (!platform_init(&props)) return 1;

    /* ── Initialise game state ───────────────────────────────────────── */
    GameState state;
    memset(&state, 0, sizeof(state));
    state.audio.samples_per_second = 44100;
    state.audio.master_volume      = 1.0f;
    state.audio.sfx_volume         = 1.0f;
    state.audio.music_volume       = 0.40f;
    state.audio.ambient_volume     = 0.30f;
    game_init(&state, ASSETS_DIR);

    /* ── Audio output buffer ─────────────────────────────────────────── */
    int16_t audio_samples[4096 * 2];  /* > max period_size * 2 channels   */
    AudioOutputBuffer audio_buf;
    audio_buf.samples            = audio_samples;
    audio_buf.samples_per_second = 44100;
    audio_buf.sample_count       = g_alsa_period_size;

    /* ── Input double-buffer ─────────────────────────────────────────── */
    GameInput inputs[2];
    memset(inputs, 0, sizeof(inputs));

    /* ── Frame timing ────────────────────────────────────────────────── */
    double t_last = platform_get_time();

    /* ══════ Main Game Loop ════════════════════════════════════════════ */
    while (props.is_running) {
        /* ── Delta time ──────────────────────────────────────────────── */
        double t_now = platform_get_time();
        double dt    = t_now - t_last;
        t_last       = t_now;
        /* Clamp: if window was minimised a huge dt would teleport the frog */
        if (dt > 0.066) dt = 0.066;
        if (dt < 0.0)   dt = 0.0;

        /* ── Input: swap + prepare + read ────────────────────────────── */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);

        if (inputs[1].quit) break;

        /* ── Update game simulation ───────────────────────────────────── */
        game_update(&state, &inputs[1], (float)dt);

        /* ── Render into pixel backbuffer ────────────────────────────── */
        game_render(&props.backbuffer, &state);

        /* ── Audio: generate and push one period to ALSA ─────────────── */
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
