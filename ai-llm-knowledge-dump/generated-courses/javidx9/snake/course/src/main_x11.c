/* ═══════════════════════════════════════════════════════════════════════════
 * main_x11.c  —  Snake Course  —  X11 + GLX + ALSA Platform Backend
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * The Linux/X11 platform backend.  It handles:
 *   • Window creation and GL context setup (X11 + GLX)
 *   • Keyboard input (XEvent polling with auto-repeat fix)
 *   • Audio output (ALSA PCM, per-frame latency model)
 *   • Pixel display (glTexImage2D → fullscreen quad)
 *   • Timing (clock_gettime CLOCK_MONOTONIC)
 *
 * THIS FILE NEVER CALLS game.c functions directly — it goes through the
 * contract in platform.h (init/get_time/get_input/shutdown).
 *
 * BUILD (via build-dev.sh):
 *   ./build-dev.sh --backend=x11           # release
 *   ./build-dev.sh --backend=x11 --debug   # debug build with ASSERT enabled
 *
 * COMPILE DEPS (handled by build-dev.sh):
 *   -lX11 -lGL -lasound
 *
 * READING ORDER
 * ─────────────
 * 1. Globals (window, GL context, ALSA handle, backbuffer)
 * 2. GL helpers (setup_gl, platform_display_backbuffer)
 * 3. ALSA helpers (setup_alsa, fill_audio)
 * 4. Platform API: platform_init / get_time / get_input / shutdown
 * 5. main() — the game loop
 *
 * COURSE NOTE: X11 is verbose but every line is intentional.  We annotate
 * each group of calls to explain what it does.  See Lesson 03 for an
 * overview of the X11 event model.
 */

#include <X11/Xlib.h>          /* XOpenDisplay, XCreateWindow, XEvent, … */
#include <X11/keysym.h>        /* XK_Left, XK_Right, XK_r, XK_q, …       */
#include <X11/XKBlib.h>        /* XkbSetDetectableAutoRepeat              */
#include <GL/gl.h>             /* glTexImage2D, glDrawArrays, …           */
#include <GL/glx.h>            /* GLXContext, glXCreateContext, …         */
#include <alsa/asoundlib.h>    /* snd_pcm_open, snd_pcm_writei, …        */
#include <time.h>              /* clock_gettime, CLOCK_MONOTONIC          */
#include <stdlib.h>            /* malloc, free                            */
#include <string.h>            /* memset                                  */
#include <stdio.h>             /* fprintf, stderr                         */

#include "platform.h"  /* contract: platform_init / get_time / get_input / shutdown */
#include "game.h"      /* snake_init, snake_update, snake_render, game_get_audio_samples */

/* ══════ Globals ═════════════════════════════════════════════════════════════
 *
 * WHY GLOBALS (not a struct)?
 *   Platform state (window, GL context, ALSA handle) must persist across
 *   the four platform_* calls.  Using globals avoids passing a platform
 *   context pointer through every function — the platform layer is a singleton.
 *   Game state (GameState) is NOT a global — it lives on the stack in main().
 */
static Display        *g_display           = NULL;  /* X11 server connection       */
static Window          g_window            = 0;     /* The application window      */
static GLXContext      g_gl_context        = NULL;  /* OpenGL context              */
static Atom            g_wm_delete_window  = 0;     /* WM_DELETE_WINDOW atom       */
static snd_pcm_t      *g_alsa_handle       = NULL;  /* ALSA PCM handle             */
static SnakeBackbuffer g_backbuffer;                 /* CPU-side pixel array        */
static GLuint          g_texture_id        = 0;     /* GPU texture for the backbuf */
static int             g_is_running        = 1;     /* 0 → exit the game loop      */
static int16_t        *g_audio_buf         = NULL;  /* Heap buffer for ALSA writes */
static int             g_samples_per_frame = 0;

/* ══════ GL Setup ════════════════════════════════════════════════════════════
 *
 * We use the SIMPLEST possible GL path: one RGB texture + one fullscreen quad.
 * No shaders — compatibility profile, fixed-function pipeline.
 *
 * WHY OPENGL INSTEAD OF XPutImage?
 *   XPutImage copies pixels through Xlib's connection to the X server.
 *   It's correct but slow for large images (1200×460 = ~2MB per frame).
 *   glTexImage2D uploads to GPU VRAM once; glDrawArrays then just renders
 *   the already-uploaded texture — much faster, uses GPU compositing.
 *
 * GL_TEXTURE_RECTANGLE_ARB vs GL_TEXTURE_2D:
 *   GL_TEXTURE_2D requires power-of-two dimensions.  Our window (1200×460)
 *   is not a power of two.  We use GL_TEXTURE_RECTANGLE_ARB which accepts
 *   arbitrary sizes and uses unnormalized texcoords (pixel values, not 0..1).
 *   This avoids the extra complexity of a pow-of-two framebuffer.
 */
static void setup_gl(void) {
    glGenTextures(1, &g_texture_id);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, g_texture_id);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);

    /* Orthographic projection: maps world coords 1:1 to pixel coords */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* platform_display_backbuffer — upload CPU pixels to GPU and draw a quad.
 *
 * This is called once per frame after snake_render() fills the backbuffer.
 *
 * glTexImage2D — upload backbuffer.pixels to the GPU texture.
 *   Format: GL_BGRA (note: SnakeBackbuffer stores 0xAARRGGBB as uint32,
 *   which is BGRA in memory on little-endian x86).
 *
 * Fullscreen quad with GL_TEXTURE_RECTANGLE_ARB texcoords:
 *   In non-ARB GL_TEXTURE_2D, texcoords are 0..1.
 *   In GL_TEXTURE_RECTANGLE_ARB, texcoords are 0..WIDTH and 0..HEIGHT.
 *   This makes it easier to map pixel-for-pixel.
 */
static void platform_display_backbuffer(void) {
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, g_texture_id);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8,
                 g_backbuffer.width, g_backbuffer.height, 0,
                 GL_RGBA,            /* pixels are [R][G][B][A] in memory — matches GAME_RGB layout */
                 GL_UNSIGNED_BYTE, g_backbuffer.pixels);

    glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2i(0,                    0);                     glVertex2i(0,             0);
        glTexCoord2i(g_backbuffer.width,   0);                     glVertex2i(WINDOW_WIDTH,  0);
        glTexCoord2i(0,                    g_backbuffer.height);   glVertex2i(0,             WINDOW_HEIGHT);
        glTexCoord2i(g_backbuffer.width,   g_backbuffer.height);   glVertex2i(WINDOW_WIDTH,  WINDOW_HEIGHT);
    glEnd();

    glXSwapBuffers(g_display, g_window);
}

/* ══════ ALSA Setup ══════════════════════════════════════════════════════════
 *
 * ALSA (Advanced Linux Sound Architecture) gives us direct access to the
 * sound card PCM interface.  We configure it for:
 *   - Stereo, 16-bit signed PCM (matches our int16_t buffer layout)
 *   - 44100 Hz sample rate
 *   - A small buffer (~2 frames of audio) for low latency
 *
 * LATENCY MODEL (Casey Muratori style):
 *   We don't use a callback (like many audio APIs do).  Instead, we query
 *   how many samples ALSA still has in its hardware buffer (snd_pcm_avail_update),
 *   and write just enough NEW samples to stay ahead by `latency_samples`.
 *   This gives us explicit control over latency and avoids underruns.
 *
 *   latency_samples = samples_per_second / 30 ≈ 1470 samples ≈ 33ms @ 44100 Hz
 *   (Two frames of audio at 60 FPS)
 */
static void setup_alsa(int samples_per_second) {
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    unsigned int         rate         = (unsigned int)samples_per_second;
    int                  dir          = 0;
    snd_pcm_uframes_t    buffer_size  = 4096;
    snd_pcm_uframes_t    period_size  = 1024;

    if (snd_pcm_open(&g_alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "[ALSA] snd_pcm_open failed\n");
        return;
    }

    /* Hardware params — set format, rate, buffer/period sizes */
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(g_alsa_handle, hw_params);
    snd_pcm_hw_params_set_access(g_alsa_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(g_alsa_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(g_alsa_handle, hw_params, 2);  /* stereo */
    snd_pcm_hw_params_set_rate_near(g_alsa_handle, hw_params, &rate, &dir);
    snd_pcm_hw_params_set_buffer_size_near(g_alsa_handle, hw_params, &buffer_size);
    snd_pcm_hw_params_set_period_size_near(g_alsa_handle, hw_params, &period_size, &dir);
    snd_pcm_hw_params(g_alsa_handle, hw_params);

    /* Software params — CRITICAL FIX:
     * Default start_threshold = buffer_size.  This means ALSA waits until the
     * entire ring buffer is full before starting playback.  With our per-frame
     * write model (~1024 samples/frame into a 4096-sample buffer), the buffer
     * never fills → playback never starts → no audio.
     *
     * Setting start_threshold = 1 starts playback as soon as any data is written. */
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(g_alsa_handle, sw_params);
    snd_pcm_sw_params_set_start_threshold(g_alsa_handle, sw_params, 1);
    snd_pcm_sw_params(g_alsa_handle, sw_params);

    snd_pcm_prepare(g_alsa_handle);

    /* Write one period per frame (period_size frames after hw_params_set_period_size_near) */
    g_samples_per_frame = (int)period_size;
    g_audio_buf = (int16_t *)malloc((size_t)g_samples_per_frame * 2 * sizeof(int16_t));
}

/* fill_audio — write pending samples to ALSA.
 *
 * snd_pcm_avail_update(): samples the hardware has consumed since last write.
 * avail > 0 means ALSA is hungry — it needs more data.
 *
 * We write min(avail, g_samples_per_frame) samples per frame.
 * If avail == 0, ALSA still has plenty — skip writing this frame.
 *
 * snd_pcm_recover() handles buffer underruns (EPIPE) gracefully.
 */
static void fill_audio(GameState *state) {
    snd_pcm_sframes_t avail;

    if (!g_alsa_handle || !g_audio_buf) return;

    avail = snd_pcm_avail_update(g_alsa_handle);
    if (avail < 0) {
        snd_pcm_recover(g_alsa_handle, (int)avail, 0);
        return;
    }
    /* Only write when ALSA has room for a full period — prevents partial writes */
    if (avail < g_samples_per_frame) return;

    {
        AudioOutputBuffer ab;
        ab.samples            = g_audio_buf;
        ab.samples_per_second = 44100;
        ab.sample_count       = g_samples_per_frame;
        game_get_audio_samples(state, &ab);
    }

    {
        snd_pcm_sframes_t written = snd_pcm_writei(g_alsa_handle, g_audio_buf,
                                                    (snd_pcm_uframes_t)g_samples_per_frame);
        if (written < 0) snd_pcm_recover(g_alsa_handle, (int)written, 0);
    }
}

/* ══════ Platform API Implementation ═════════════════════════════════════════
 *
 * These four functions implement the contract declared in platform.h.
 * They are the ONLY entry points from main() into platform-specific code.
 */

/* platform_init — create window, GL context, backbuffer, and audio device. */
void platform_init(const char *title, int width, int height) {
    XSetWindowAttributes wa;
    int                  glx_attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, None };
    XVisualInfo         *vi;
    Colormap             cmap;

    /* Connect to X server */
    g_display = XOpenDisplay(NULL);
    if (!g_display) { fprintf(stderr, "[X11] XOpenDisplay failed\n"); return; }

    /* Choose a visual with RGBA + double-buffer */
    vi = glXChooseVisual(g_display, DefaultScreen(g_display), glx_attribs);
    if (!vi) { fprintf(stderr, "[GLX] glXChooseVisual failed\n"); return; }

    /* Create a colormap for the chosen visual */
    cmap = XCreateColormap(g_display, DefaultRootWindow(g_display), vi->visual, AllocNone);

    /* Create the window */
    wa.colormap   = cmap;
    wa.event_mask = KeyPressMask | KeyReleaseMask;
    g_window = XCreateWindow(g_display, DefaultRootWindow(g_display),
                             0, 0, (unsigned)width, (unsigned)height, 0,
                             vi->depth, InputOutput, vi->visual,
                             CWColormap | CWEventMask, &wa);
    XStoreName(g_display, g_window, title);
    XMapWindow(g_display, g_window);

    /* Register WM_DELETE_WINDOW protocol so clicking the window's X button
     * sends a ClientMessage event instead of forcibly destroying the window.
     * Without this, clicking X kills the window while the game loop still runs,
     * causing glXSwapBuffers to block forever → the terminal hangs.          */
    g_wm_delete_window = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete_window, 1);

    /* Fix X11 auto-repeat: without this, holding a key fires rapid fake
     * release+press events — confusing the input system. */
    XkbSetDetectableAutoRepeat(g_display, True, NULL);

    /* Create and activate GL context */
    g_gl_context = glXCreateContext(g_display, vi, NULL, GL_TRUE);
    glXMakeCurrent(g_display, g_window, g_gl_context);
    XFree(vi);

    /* Allocate and zero the CPU-side backbuffer */
    g_backbuffer.width  = width;
    g_backbuffer.height = height;
    g_backbuffer.pitch  = width * 4;  /* bytes per row */
    g_backbuffer.pixels = (uint32_t *)malloc((size_t)(width * height) * sizeof(uint32_t));
    memset(g_backbuffer.pixels, 0, (size_t)(width * height) * sizeof(uint32_t));

    setup_gl();
    setup_alsa(44100);
}

/* platform_get_time — monotonic time in seconds (nanosecond resolution). */
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* platform_get_input — drain the X11 event queue and map keys to GameInput.
 *
 * IMPORTANT: called AFTER prepare_input_frame() each frame.
 * UPDATE_BUTTON fills half_transition_count automatically on state changes.
 *
 * KEY MAPPINGS:
 *   Left/A  → turn_left
 *   Right/D → turn_right
 *   R/Space → restart (one-shot)
 *   Q/Escape → quit (one-shot)
 */
void platform_get_input(GameInput *input) {
    while (XPending(g_display)) {
        XEvent ev;
        XNextEvent(g_display, &ev);

        if (ev.type == KeyPress || ev.type == KeyRelease) {
            int    is_down = (ev.type == KeyPress) ? 1 : 0;
            /* XkbKeycodeToKeysym with group=0, level=0 always returns the base
             * (unshifted, no-modifier) keysym — reliable regardless of caps lock
             * or shift state.  More robust than XLookupKeysym(&ev.xkey, 0) when
             * XKB auto-repeat detection is active.                              */
            KeySym ks = XkbKeycodeToKeysym(g_display, ev.xkey.keycode, 0, 0);

            switch (ks) {
                case XK_Left:   case XK_a: UPDATE_BUTTON(input->turn_left,  is_down); break;
                case XK_Right:  case XK_d: UPDATE_BUTTON(input->turn_right, is_down); break;
                case XK_r:      case XK_space:  if (is_down) input->restart = 1; break;
                case XK_q:      case XK_Escape: if (is_down) { input->quit = 1; g_is_running = 0; } break;
                default: break;
            }

        } else if (ev.type == ClientMessage) {
            /* Window manager sent WM_DELETE_WINDOW — user clicked the X button.
             * Without handling this, the WM would destroy the window while the
             * game loop still calls glXSwapBuffers → blocks forever → hung terminal. */
            if ((Atom)ev.xclient.data.l[0] == g_wm_delete_window) {
                input->quit  = 1;
                g_is_running = 0;
            }
        }
    }
}

/* platform_shutdown — free all platform resources. */
void platform_shutdown(void) {
    if (g_alsa_handle) {
        snd_pcm_drain(g_alsa_handle);
        snd_pcm_close(g_alsa_handle);
        g_alsa_handle = NULL;
    }
    free(g_audio_buf);
    g_audio_buf = NULL;

    free(g_backbuffer.pixels);
    g_backbuffer.pixels = NULL;

    if (g_gl_context) {
        glXMakeCurrent(g_display, None, NULL);
        glXDestroyContext(g_display, g_gl_context);
    }
    if (g_display) {
        XDestroyWindow(g_display, g_window);
        XCloseDisplay(g_display);
    }
}

/* ══════ main ════════════════════════════════════════════════════════════════
 *
 * THE GAME LOOP — the heartbeat of the program.
 *
 * LOOP STRUCTURE (every frame):
 *   1. Compute delta_time  (time since last frame)
 *   2. Swap & prepare input buffers
 *   3. Poll OS events → fill current input
 *   4. Game update (logic, movement, collision, scoring)
 *   5. Game render (pixels into backbuffer)
 *   6. Audio output (fill ALSA buffer)
 *   7. Display backbuffer (GPU upload + swap)
 *
 * DELTA-TIME:
 *   dt = current_time - last_time
 *   Clamped to [0, 0.05] to prevent "bullet through paper" on large spikes
 *   (e.g., the window was covered and unblocked — one huge dt would advance
 *   the snake 10 cells at once and look wrong).
 *
 * DOUBLE-BUFFERED INPUT: inputs[0] = previous frame, inputs[1] = current frame.
 *   platform_swap_input_buffers swaps contents each frame.
 *   prepare_input_frame copies ended_down and clears transition counts.
 *   platform_get_input fills new events into inputs[1].
 *
 * The game loop runs until g_is_running = 0 (set by quit input or window close).
 */
int main(void) {
    GameState  state;
    GameInput  inputs[2];
    double     last_time, current_time;
    float      delta_time;

    /* ── Initialise ──────────────────────────────────────────────────────── */
    platform_init("Snake", WINDOW_WIDTH, WINDOW_HEIGHT);

    memset(&state, 0, sizeof(state));
    state.audio.samples_per_second = 44100;
    game_audio_init(&state);
    snake_init(&state);

    memset(inputs, 0, sizeof(inputs));
    last_time = platform_get_time();

    /* ── Game Loop ───────────────────────────────────────────────────────── */
    while (g_is_running) {
        /* 1. Delta time */
        current_time = platform_get_time();
        delta_time   = (float)(current_time - last_time);
        if (delta_time > 0.05f) delta_time = 0.05f;  /* cap at 50ms */
        last_time    = current_time;

        /* 2. Swap & prepare input */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);

        /* 3. Poll OS events */
        platform_get_input(&inputs[1]);
        if (inputs[1].quit) break;

        /* 4. Game update */
        snake_update(&state, &inputs[1], delta_time);

        /* 5. Render */
        snake_render(&state, &g_backbuffer);

        /* 6. Audio */
        fill_audio(&state);

        /* 7. Display */
        platform_display_backbuffer();
    }

    /* ── Cleanup ─────────────────────────────────────────────────────────── */
    platform_shutdown();
    return 0;
}
