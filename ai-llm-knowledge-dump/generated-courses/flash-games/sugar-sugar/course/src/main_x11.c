/*
 * main_x11.c  —  Sugar, Sugar | X11 Platform Backend
 *
 * Implements the platform.h contract using:
 *   - Xlib        (window creation, event polling)
 *   - XImage      (software pixel blit to display)
 *   - clock_gettime (monotonic timer)
 *
 * The window is kept at a fixed 640×480 — X11 has no built-in scaling for
 * XImage blits, so we lock the size via XSizeHints rather than attempting
 * software scaling.  The Raylib backend supports letterbox scaling.
 *
 * Build:
 *   clang -Wall -Wextra -O2 -o sugar_x11 \
 *         src/main_x11.c src/game.c src/levels.c -lX11 -lm
 *
 * Debug build:
 *   clang -Wall -Wextra -O0 -g -fsanitize=address,undefined \
 *         -o sugar_x11_dbg src/main_x11.c src/game.c src/levels.c -lX11 -lm
 */

/* Expose POSIX extensions: clock_gettime, nanosleep, struct timespec */
#define _POSIX_C_SOURCE 199309L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <time.h>    /* clock_gettime  */
#include <stdlib.h>  /* malloc, free   */
#include <string.h>  /* memset         */

#include "platform.h"

/* ===================================================================
 * PLATFORM GLOBALS
 * These live in this file only — game.c never sees them.
 * =================================================================== */
static Display *g_display;
static Window   g_window;
static GC       g_gc;
static XImage  *g_ximage;
static int      g_screen;
static int      g_should_quit;

/* We keep a pointer to the shared backbuffer pixel array so XImage
 * can reference it without a copy. */
static uint32_t *g_pixel_data;
static int       g_pixel_w;
static int       g_pixel_h;

/* ===================================================================
 * PLATFORM IMPLEMENTATION
 * =================================================================== */

void platform_init(const char *title, int width, int height) {
    g_display = XOpenDisplay(NULL); /* connect to the X server */
    if (!g_display) {
        /* If XOpenDisplay fails, we can't render anything — hard exit. */
        return;
    }
    g_screen = DefaultScreen(g_display);

    /* Create a simple window */
    g_window = XCreateSimpleWindow(
        g_display,
        RootWindow(g_display, g_screen),
        0, 0,              /* x, y — window manager will reposition */
        (unsigned)width,
        (unsigned)height,
        1,                 /* border width */
        BlackPixel(g_display, g_screen),
        WhitePixel(g_display, g_screen)
    );

    /* Lock window size so the user cannot resize it.
     * X11 does not provide a built-in way to scale an XImage blit,
     * so a fixed-size window is the simplest correct approach.
     * The Raylib backend supports letterbox scaling instead. */
    XSizeHints *hints = XAllocSizeHints();
    if (hints) {
        hints->flags      = PMinSize | PMaxSize;
        hints->min_width  = hints->max_width  = width;
        hints->min_height = hints->max_height = height;
        XSetWMNormalHints(g_display, g_window, hints);
        XFree(hints);
    }

    /* Register for the events we care about */
    XSelectInput(g_display, g_window,
                 ExposureMask        |  /* window needs a repaint        */
                 KeyPressMask        |  /* key pressed                   */
                 KeyReleaseMask      |  /* key released                  */
                 ButtonPressMask     |  /* mouse button pressed          */
                 ButtonReleaseMask   |  /* mouse button released         */
                 PointerMotionMask   |  /* mouse moved while button held */
                 Button1MotionMask   |  /* motion with left button held  */
                 StructureNotifyMask);  /* ConfigureNotify for size changes */

    /* Tell the window manager we want window-close events */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &wm_delete, 1);

    /* Set the window title */
    XStoreName(g_display, g_window, title);

    /* Show the window */
    XMapWindow(g_display, g_window);
    XFlush(g_display);

    /* Create a graphics context for XPutImage */
    g_gc = XCreateGC(g_display, g_window, 0, NULL);
}

double platform_get_time(void) {
    /* CLOCK_MONOTONIC never jumps backward (unlike gettimeofday).
     * This is the correct clock to use for delta-time calculations. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void platform_get_input(GameInput *input) {
    /* XPending returns the number of events in the queue without blocking. */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);

    /* ---- Save start-of-frame mouse position BEFORE processing events ----
     *
     * X11 can queue MANY MotionNotify events between frames (e.g. 10+
     * events when the mouse moves fast).  If we update prev_x inside the
     * event loop, by the time game_update runs, prev_x/y only holds the
     * position from the second-to-last event — all earlier segments of the
     * mouse path are lost.  draw_brush_line(prev_x, prev_y, x, y) then
     * draws only the tiny last segment, leaving visible gaps.
     *
     * Fix: snapshot the position once here, before the event loop.
     * After all events are consumed, x/y = end-of-frame position and
     * prev_x/y = start-of-frame position.  Bresenham in draw_brush_line
     * interpolates the full path in one call, covering every pixel the
     * mouse crossed regardless of how many events were queued. */
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;

    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {

        case MotionNotify:
            /* Update current position only — prev is already saved above. */
            input->mouse.x = event.xmotion.x;
            input->mouse.y = event.xmotion.y;
            break;

        case ButtonPress:
            if (event.xbutton.button == Button1) {
                /* Also update position so prev matches current on click */
                input->mouse.prev_x = event.xbutton.x;
                input->mouse.prev_y = event.xbutton.y;
                input->mouse.x      = event.xbutton.x;
                input->mouse.y      = event.xbutton.y;
                UPDATE_BUTTON(input->mouse.left, 1);
            }
            break;

        case ButtonRelease:
            if (event.xbutton.button == Button1)
                UPDATE_BUTTON(input->mouse.left, 0);
            break;

        case KeyPress: {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            if (sym == XK_Escape)                        UPDATE_BUTTON(input->escape,  1);
            if (sym == XK_r || sym == XK_R)              UPDATE_BUTTON(input->reset,   1);
            if (sym == XK_g || sym == XK_G)              UPDATE_BUTTON(input->gravity, 1);
            if (sym == XK_Return || sym == XK_space)     UPDATE_BUTTON(input->enter,   1);
            break;
        }

        case KeyRelease: {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            if (sym == XK_Escape)                        UPDATE_BUTTON(input->escape,  0);
            if (sym == XK_r || sym == XK_R)              UPDATE_BUTTON(input->reset,   0);
            if (sym == XK_g || sym == XK_G)              UPDATE_BUTTON(input->gravity, 0);
            if (sym == XK_Return || sym == XK_space)     UPDATE_BUTTON(input->enter,   0);
            break;
        }

        case ClientMessage:
            /* Window close button (the ✕) — set quit flag */
            if ((Atom)event.xclient.data.l[0] == wm_delete)
                g_should_quit = 1;
            break;

        case Expose:
            /* The window was uncovered — we'll redraw next frame anyway */
            break;

        case ConfigureNotify:
            /* Window size/position changed.  Because we set PMinSize == PMaxSize
             * the window manager should not resize us, but we handle the event
             * to keep the queue clean. */
            break;
        }
    }
}

void platform_display_backbuffer(const GameBackbuffer *backbuffer) {
    /*
     * On the first call, create the XImage that wraps our pixel buffer.
     * XCreateImage does NOT copy the data — it references g_pixel_data
     * directly.  So every frame, XPutImage reads from the same memory
     * that game_render() just wrote to.
     */
    if (!g_ximage) {
        g_pixel_data = backbuffer->pixels;
        g_pixel_w    = backbuffer->width;
        g_pixel_h    = backbuffer->height;

        /* Create the XImage.  The pixel format 0xAARRGGBB stored
         * little-endian as [B,G,R,A] maps correctly to X11's default
         * TrueColor visual on x86 Linux (blue_mask = 0xFF, red_mask = 0xFF0000). */
        g_ximage = XCreateImage(
            g_display,
            DefaultVisual(g_display, g_screen),
            (unsigned)DefaultDepth(g_display, g_screen),
            ZPixmap,
            0,                        /* offset */
            (char *)backbuffer->pixels,
            (unsigned)backbuffer->width,
            (unsigned)backbuffer->height,
            32,                       /* bitmap_pad: 32-bit aligned */
            backbuffer->pitch         /* bytes per row */
        );
        /* Explicitly declare byte order so X11 doesn't misinterpret
         * our 0xAARRGGBB pixels on big-endian hosts or unusual displays. */
        if (g_ximage) g_ximage->byte_order = LSBFirst;
    }

    /* Upload our pixel buffer to the X11 window at 1:1 scale.
     * The window is locked to CANVAS_W × CANVAS_H so no scaling is needed. */
    XPutImage(g_display, g_window, g_gc, g_ximage,
              0, 0,  /* src x, y */
              0, 0,  /* dst x, y */
              (unsigned)backbuffer->width,
              (unsigned)backbuffer->height);
    XFlush(g_display);
}

/* -----------------------------------------------------------------------
 * Audio (ALSA) — Casey's Latency Model
 *
 * Reference: games/tetris/src/main_x11.c — same architecture, adapted
 * for Sugar Sugar's simpler platform API (no PlatformAudioConfig).
 *
 * Why snd_pcm_hw_params (not snd_pcm_set_params)?
 *   snd_pcm_set_params is a convenience wrapper but it can fail silently
 *   on many ALSA configs and doesn't expose hw_buffer_size — which we need
 *   for Casey's latency model.  The hw_params API is explicit and reliable.
 *
 * Casey's model: each frame, query how many samples are already queued.
 *   samples_to_write = (target_latency - queued_samples).
 *   This keeps the hardware buffer filled just ahead of playback — never
 *   too empty (underruns = silence/glitches), never too full (audio lag).
 *
 * Build: -DALSA_AVAILABLE -lasound (both in build-dev.sh for x11 backend)
 * ----------------------------------------------------------------------- */
#ifdef ALSA_AVAILABLE
#include <alloca.h>           /* required for snd_pcm_*_alloca macros with clang */
#include <alsa/asoundlib.h>

#define ALSA_SAMPLES_PER_SEC  44100
#define ALSA_GAME_HZ          60
#define ALSA_FRAMES_PER_FRAME (ALSA_SAMPLES_PER_SEC / ALSA_GAME_HZ)   /* 735 */
/* 4 frames of latency (~66 ms) — more headroom than 2 frames reduces underruns
 * that produce audible clicks on systems with variable frame timing. */
#define ALSA_LATENCY_FRAMES   (ALSA_FRAMES_PER_FRAME * 4)             /* ~4 frames */
#define ALSA_SAFETY_FRAMES    (ALSA_FRAMES_PER_FRAME / 2)
#define ALSA_SAMPLE_BUF_SIZE  (ALSA_FRAMES_PER_FRAME * 8)             /* 8× headroom */

static snd_pcm_t   *g_pcm          = NULL;
static int16_t     *g_alsa_buf     = NULL;   /* malloc'd stereo sample buffer */
static int          g_alsa_ready   = 0;
static snd_pcm_uframes_t g_hw_buf_size = 0;  /* actual ALSA hardware buffer size */

void platform_audio_init(GameState *state, int samples_per_second) {
    (void)samples_per_second; /* we use ALSA_SAMPLES_PER_SEC */
    game_audio_init(&state->audio, ALSA_SAMPLES_PER_SEC);

    int err = snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) { g_pcm = NULL; g_alsa_ready = 0; return; }

    /* ── Hardware parameters ────────────────────────────────────────── */
    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(g_pcm, hw);

    snd_pcm_hw_params_set_access(g_pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(g_pcm, hw, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(g_pcm, hw, 2);

    unsigned int rate = ALSA_SAMPLES_PER_SEC;
    snd_pcm_hw_params_set_rate_near(g_pcm, hw, &rate, 0);

    /* Buffer: latency + safety + double headroom */
    snd_pcm_uframes_t buf_frames = (ALSA_LATENCY_FRAMES + ALSA_SAFETY_FRAMES) * 2;
    snd_pcm_hw_params_set_buffer_size_near(g_pcm, hw, &buf_frames);
    g_hw_buf_size = buf_frames;

    /* Period: ¼ of buffer */
    snd_pcm_uframes_t period_frames = buf_frames / 4;
    snd_pcm_hw_params_set_period_size_near(g_pcm, hw, &period_frames, 0);

    err = snd_pcm_hw_params(g_pcm, hw);
    if (err < 0) { snd_pcm_close(g_pcm); g_pcm = NULL; g_alsa_ready = 0; return; }

    /* Read back actual buffer size after ALSA may have adjusted it */
    snd_pcm_hw_params_get_buffer_size(hw, &buf_frames);
    g_hw_buf_size = buf_frames;

    /* ── Software parameters: start immediately on first write ─────── */
    {
        snd_pcm_sw_params_t *sw;
        snd_pcm_sw_params_alloca(&sw);
        snd_pcm_sw_params_current(g_pcm, sw);         /* load defaults FIRST */
        snd_pcm_sw_params_set_start_threshold(g_pcm, sw, 1);
        snd_pcm_sw_params(g_pcm, sw);
    }

    /* ── Sample buffer ─────────────────────────────────────────────── */
    g_alsa_buf = (int16_t *)malloc(ALSA_SAMPLE_BUF_SIZE * 2 * sizeof(int16_t));
    if (!g_alsa_buf) { snd_pcm_close(g_pcm); g_pcm = NULL; g_alsa_ready = 0; return; }

    /* Pre-fill with silence to start playback cleanly */
    memset(g_alsa_buf, 0, ALSA_LATENCY_FRAMES * 2 * sizeof(int16_t));
    snd_pcm_writei(g_pcm, g_alsa_buf, ALSA_LATENCY_FRAMES);
    snd_pcm_prepare(g_pcm);

    g_alsa_ready = 1;
}

/* Casey's write-amount calculation:
 *   Ask ALSA how many frames are currently queued in the hardware buffer
 *   (snd_pcm_delay gives the precise playhead position), then write just
 *   enough to reach our latency target.  This keeps latency stable without
 *   over-filling or under-filling. */
static int alsa_samples_to_write(void) {
    if (!g_pcm) return 0;

    snd_pcm_sframes_t delay = 0;
    int err = snd_pcm_delay(g_pcm, &delay);
    if (err < 0) {
        if (err == -EPIPE) { snd_pcm_prepare(g_pcm); delay = 0; }
        else return 0;
    }

    snd_pcm_sframes_t avail = snd_pcm_avail_update(g_pcm);
    if (avail < 0) {
        if (avail == -EPIPE) { snd_pcm_prepare(g_pcm); avail = (snd_pcm_sframes_t)g_hw_buf_size; delay = 0; }
        else return 0;
    }

    int target   = ALSA_LATENCY_FRAMES + ALSA_SAFETY_FRAMES;
    int to_write = target - (int)delay;
    if (to_write > (int)avail)          to_write = (int)avail;
    if (to_write < ALSA_FRAMES_PER_FRAME / 4) to_write = 0; /* skip tiny writes */
    if (to_write > ALSA_SAMPLE_BUF_SIZE) to_write = ALSA_SAMPLE_BUF_SIZE;
    return to_write;
}

void platform_audio_update(GameState *state) {
    if (!g_alsa_ready || !g_pcm || !g_alsa_buf) return;

    int n = alsa_samples_to_write();
    if (n <= 0) return;

    AudioOutputBuffer buf = {
        .samples            = g_alsa_buf,
        .samples_per_second = ALSA_SAMPLES_PER_SEC,
        .sample_count       = n,
    };
    game_get_audio_samples(state, &buf);

    snd_pcm_sframes_t written = snd_pcm_writei(g_pcm, g_alsa_buf, (snd_pcm_uframes_t)n);
    if (written < 0) {
        /* Underrun or suspend — recover and re-prime with silence so the
         * device doesn't click when playback resumes from a dry buffer.
         * snd_pcm_recover handles EPIPE (underrun) and ESTRPIPE (suspend). */
        int rerr = snd_pcm_recover(g_pcm, (int)written, 0);
        if (rerr >= 0) {
            /* Re-fill with silence up to the latency target to absorb the gap */
            int prefill = ALSA_LATENCY_FRAMES;
            if (prefill > ALSA_SAMPLE_BUF_SIZE) prefill = ALSA_SAMPLE_BUF_SIZE;
            memset(g_alsa_buf, 0, (size_t)prefill * 2 * sizeof(int16_t));
            snd_pcm_writei(g_pcm, g_alsa_buf, (snd_pcm_uframes_t)prefill);
        }
    }
}

void platform_audio_shutdown(void) {
    if (g_alsa_buf) { free(g_alsa_buf); g_alsa_buf = NULL; }
    if (!g_pcm) return;
    snd_pcm_drain(g_pcm);
    snd_pcm_close(g_pcm);
    g_pcm = NULL; g_alsa_ready = 0;
}

#else /* !ALSA_AVAILABLE — silent stubs so the build always succeeds */

void platform_audio_init(GameState *state, int samples_per_second) {
    game_audio_init(&state->audio, samples_per_second);
}
void platform_audio_update(GameState *state) { (void)state; }
void platform_audio_shutdown(void) {}

#endif /* ALSA_AVAILABLE */

/* ===================================================================
 * MAIN LOOP
 * =================================================================== */

int main(void) {
    /* Declare as static so the ~460 KB of arrays in GameState and
     * the 2.5 MB backbuffer pixel array are placed in the BSS segment
     * (zero-initialised at startup) rather than on the stack.
     *
     * JS analogy: this is like module-level variables — they exist for
     * the lifetime of the program, not just one function call. */
    static GameState      state;
    static GameBackbuffer bb;

    /* Allocate the pixel buffer on the heap.
     * 640 × 480 × 4 bytes = 1,228,800 bytes ≈ 1.2 MB. */
    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1; /* out of memory */

    /* Open the window (must happen before game_init so bb dimensions work) */
    platform_init("Sugar, Sugar", CANVAS_W, CANVAS_H);

    /* Initialise the game: zero state, load level 0, set backbuffer dims */
    game_init(&state, &bb);

    /* Open audio device and wire sample rate into game audio state.
     * IMPORTANT: platform_audio_init calls game_audio_init which memset-zeros
     * the entire GameAudioState — including the is_playing flag set by
     * game_init via change_phase(PHASE_TITLE).  Retrigger title music here
     * after audio is fully initialised so music starts on launch. */
    platform_audio_init(&state, 44100);
    game_music_play_title(&state.audio);

    GameInput input;
    memset(&input, 0, sizeof(input));

    /* Delta-time game loop */
    double prev_time = platform_get_time();

    while (!g_should_quit && !state.should_quit) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;

        /* Cap delta-time: if we were paused in a debugger for 10 seconds,
         * we don't want the physics to explode. */
        if (delta_time > 0.1f) delta_time = 0.1f;

        /* Step 1: clear per-frame input counters */
        prepare_input_frame(&input);

        /* Step 2: read OS events → fill input struct */
        platform_get_input(&input);

        /* Step 3: advance the simulation */
        game_update(&state, &input, delta_time);

        /* Step 3b: feed audio hardware if it needs more data */
        platform_audio_update(&state);

        /* Step 4: draw the current frame into the pixel buffer */
        game_render(&state, &bb);

        /* Step 5: send the pixel buffer to the X11 window */
        platform_display_backbuffer(&bb);

        /* ---- Frame rate cap: target 60 fps ----
         * Without a cap the game runs at thousands of fps.  dt is then
         * microscopic (< 0.0001 s), grains move < 0.005 px per frame, and
         * the displacement-based settle check immediately marks every grain
         * as "still" — causing them to bake and vanish before they are even
         * rendered once.
         *
         * nanosleep sleeps for the remainder of the ~16.7 ms budget.
         * If the frame took longer than 16.7 ms we skip the sleep. */
        {
            double frame_end = platform_get_time();
            double elapsed   = frame_end - curr_time;
            double target    = 1.0 / 60.0;
            if (elapsed < target) {
                double sleep_s = target - elapsed;
                struct timespec ts;
                ts.tv_sec  = (time_t)sleep_s;
                ts.tv_nsec = (long)((sleep_s - (double)ts.tv_sec) * 1e9);
                nanosleep(&ts, NULL);
            }
        }
    }

    /* Cleanup */
    platform_audio_shutdown();
    if (g_ximage) {
        /* Prevent XDestroyImage from freeing our pixel data
         * (we'll free it ourselves below). */
        g_ximage->data = NULL;
        XDestroyImage(g_ximage);
    }
    if (g_gc)      XFreeGC(g_display, g_gc);
    if (g_window)  XDestroyWindow(g_display, g_window);
    if (g_display) XCloseDisplay(g_display);
    free(bb.pixels);

    return 0;
}
