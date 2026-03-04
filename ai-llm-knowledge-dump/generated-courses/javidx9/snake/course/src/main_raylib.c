/* ═══════════════════════════════════════════════════════════════════════════
 * main_raylib.c  —  Snake Course  —  Raylib Platform Backend
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * The Raylib platform backend.  Raylib handles window creation, input,
 * audio streaming, and pixel upload in a cross-platform way.
 *
 * COMPARED TO main_x11.c:
 *   • Window + GL: InitWindow / BeginDrawing / EndDrawing — no manual GLX.
 *   • Input: IsKeyPressed / IsKeyDown — poll-based, not event-based.
 *   • Audio: AudioStream with IsAudioStreamProcessed — per-frame fill model.
 *   • Timing: GetTime() — same interface as clock_gettime behind the scenes.
 *   • Pixels: UpdateTexture per frame.
 *
 * BUILD:
 *   ./build-dev.sh --backend=raylib           # release
 *   ./build-dev.sh --backend=raylib --debug   # with ASSERT
 *
 * COMPILE DEPS (handled by build-dev.sh):
 *   -lraylib -lm -lpthread -ldl (Linux); Raylib's flags on macOS/Windows.
 *
 * COURSE NOTE:
 *   Comparing main_x11.c and main_raylib.c side-by-side teaches the key
 *   principle: the SAME game.c runs on both.  Only the platform glue changes.
 *   See Lesson 13 (optional) for a detailed comparison.
 */

#include "raylib.h"    /* InitWindow, BeginDrawing, AudioStream, IsKeyDown, … */
#include <string.h>    /* memset */
#include <stdlib.h>    /* malloc, free */

#include "platform.h"  /* platform_init / get_time / get_input / shutdown */
#include "game.h"      /* snake_init, snake_update, snake_render, … */

/* ══════ Globals ══════════════════════════════════════════════════════════════
 *
 * Same rationale as main_x11.c: platform state persists across platform_* calls.
 * Game state stays on the stack in main().
 */
static Texture2D       g_texture;           /* GPU texture for the backbuffer   */
static SnakeBackbuffer g_backbuffer;        /* CPU pixel array                  */
static AudioStream     g_audio_stream;      /* Raylib streaming audio handle     */
static int             g_samples_per_frame = 735;  /* 44100/60 ≈ 735 stereo frames */
static int16_t        *g_audio_buf         = NULL;
static int             g_is_running        = 1;

/* ══════ Platform API Implementation ══════════════════════════════════════════ */

/* platform_init — open window, set up image/texture, init audio stream.
 *
 * PIXEL FORMAT NOTE:
 *   GAME_RGB stores pixels as 0xAABBGGRR uint32 → bytes in memory: [R][G][B][A].
 *   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 reads byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A.
 *   These match exactly → colours are correct on Raylib without any channel swap.
 *   (The old layout 0xAARRGGBB had [B][G][R][A] in memory, swapping R and B.)
 */
void platform_init(const char *title, int width, int height) {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(width, height, title);
    SetTargetFPS(60);

    /* Allocate CPU pixel array */
    g_backbuffer.width  = width;
    g_backbuffer.height = height;
    g_backbuffer.pitch  = width * 4;
    g_backbuffer.pixels = (uint32_t *)malloc((size_t)(width * height) * sizeof(uint32_t));
    memset(g_backbuffer.pixels, 0, (size_t)(width * height) * sizeof(uint32_t));

    /* Create GPU texture in R8G8B8A8 format — matches our backbuffer byte order */
    {
        Image img  = {0};
        img.data   = g_backbuffer.pixels;
        img.width  = width;
        img.height = height;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        g_texture  = LoadTextureFromImage(img);
    }
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);

    /* Audio stream: 44100 Hz, 16-bit signed, 2 channels (stereo).
     * We use the per-frame IsAudioStreamProcessed model (not a callback):
     *   Each frame, check if the stream needs more data → generate → update.
     * This keeps audio generation on the main thread — no threading concerns.
     * g_samples_per_frame = 44100/60 ≈ 735 stereo frames per game frame. */
    InitAudioDevice();
    g_audio_stream = LoadAudioStream(44100, 16, 2);
    PlayAudioStream(g_audio_stream);
    g_audio_buf = (int16_t *)malloc((size_t)g_samples_per_frame * 2 * sizeof(int16_t));
}

/* platform_get_time — Raylib's monotonic timer (seconds). */
double platform_get_time(void) {
    return (double)GetTime();
}

/* platform_get_input — poll Raylib key state and map to GameInput.
 *
 * Raylib is POLL-BASED (unlike X11's event queue).
 * IsKeyPressed() fires once per physical key-press cycle (edge-triggered).
 * IsKeyDown()    fires every frame while the key is held (level-triggered).
 *
 * For turns we want single-press semantics: use UPDATE_BUTTON with IsKeyDown
 * so that ended_down tracks held state, and half_transition_count is incremented
 * on state changes.  The "just pressed" check in snake_update
 * (ended_down && half_transition_count > 0) fires correctly on the first frame.
 */
void platform_get_input(GameInput *input) {
    /* Turn left: Left arrow or A */
    UPDATE_BUTTON(input->turn_left,
                  (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) ? 1 : 0);
    /* Turn right: Right arrow or D */
    UPDATE_BUTTON(input->turn_right,
                  (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) ? 1 : 0);

    /* Restart: R or Space — one-shot */
    if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_SPACE))
        input->restart = 1;

    /* Quit: Q or Escape, or window close button */
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE) || WindowShouldClose()) {
        input->quit  = 1;
        g_is_running = 0;
    }
}

/* platform_shutdown — clean up Raylib resources. */
void platform_shutdown(void) {
    StopAudioStream(g_audio_stream);
    UnloadAudioStream(g_audio_stream);
    CloseAudioDevice();
    UnloadTexture(g_texture);
    free(g_backbuffer.pixels);
    g_backbuffer.pixels = NULL;
    free(g_audio_buf);
    g_audio_buf = NULL;
    CloseWindow();
}

/* ══════ main ════════════════════════════════════════════════════════════════
 *
 * Identical structure to main_x11.c — same loop, same call order.
 * Differences:
 *   • Audio: IsAudioStreamProcessed check → UpdateAudioStream (not ALSA writei).
 *   • Pixels: UpdateTexture + DrawTexture (not glTexImage2D + glXSwapBuffers).
 *   • BeginDrawing / EndDrawing wraps the render phase.
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
    while (g_is_running && !WindowShouldClose()) {
        /* 1. Delta time */
        current_time = platform_get_time();
        delta_time   = (float)(current_time - last_time);
        if (delta_time > 0.05f) delta_time = 0.05f;
        last_time    = current_time;

        /* 2. Swap & prepare input */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);

        /* 3. Poll input */
        platform_get_input(&inputs[1]);
        if (inputs[1].quit) break;

        /* 4. Game update */
        snake_update(&state, &inputs[1], delta_time);

        /* 5. Render — fill backbuffer */
        snake_render(&state, &g_backbuffer);

        /* 6. Audio — fill stream when Raylib has consumed the previous buffer.
         * IsAudioStreamProcessed returns true when the stream's internal buffer
         * is empty and needs refilling.  We generate exactly g_samples_per_frame
         * stereo frames and hand them to Raylib via UpdateAudioStream.          */
        if (IsAudioStreamProcessed(g_audio_stream) && g_audio_buf) {
            AudioOutputBuffer ab;
            ab.samples            = g_audio_buf;
            ab.samples_per_second = 44100;
            ab.sample_count       = g_samples_per_frame;
            game_get_audio_samples(&state, &ab);
            UpdateAudioStream(g_audio_stream, g_audio_buf,
                              (unsigned int)g_samples_per_frame);
        }

        /* 7. Upload backbuffer pixels to GPU texture and draw fullscreen */
        UpdateTexture(g_texture, g_backbuffer.pixels);
        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexture(g_texture, 0, 0, WHITE);
        EndDrawing();
    }

    /* ── Cleanup ─────────────────────────────────────────────────────────── */
    platform_shutdown();
    return 0;
}
