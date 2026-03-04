/* =============================================================================
 * main_raylib.c — Cross-Platform Backend (Raylib)
 * =============================================================================
 *
 * This file implements the platform layer using Raylib, a simple game
 * programming library that handles windowing, input, rendering, and audio
 * via a single high-level API.
 *
 * COMPARED TO THE X11 BACKEND:
 *   Raylib:  ~120 lines — Raylib does all the heavy lifting
 *   X11:     ~270 lines — manual X11 + GLX + ALSA
 *   Same game logic, vastly different implementation complexity.
 *
 * AUDIO MODEL:
 *   Raylib uses a streaming audio model rather than a callback:
 *   1. Create an AudioStream at 44100 Hz, 16-bit, 2 channels.
 *   2. Play it immediately (starts looping silence until fed data).
 *   3. Each frame: if IsAudioStreamProcessed(), call UpdateAudioStream().
 *      UpdateAudioStream's third argument is the number of stereo FRAMES
 *      (NOT individual samples) — pass sample_count, not sample_count*2.
 *
 * TEXTURE MODEL:
 *   Unlike X11 (which uses glTexSubImage2D directly), Raylib exposes:
 *   LoadImageFromMemory() → CreateTextureFromImage() → UpdateTexture() per frame.
 *   OR LoadRenderTexture → UpdateTexture for efficiency.
 *   We use the simpler Image→Texture approach.
 *
 * PIXEL FORMAT:
 *   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 matches our GAME_RGBA byte layout.
 *   Memory: [R][G][B][A] per pixel — exactly what Raylib expects.
 *
 * COURSE NOTES (improvements over reference):
 *   1. Per-frame streaming audio (no callbacks) — simpler and correct
 *   2. inputs[2] double-buffer for "just pressed" detection
 *   3. platform_shutdown() for clean teardown
 *   4. PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 matches GAME_RGBA exactly
 * =============================================================================
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "raylib.h"
#include "platform.h"

/* ══════ Global Platform State ════════════════════════════════════════════ */
static Texture2D   g_texture;
static AudioStream g_audio_stream;

/* Number of stereo frames per audio push.
   735 (= 44100/60) was the previous value, but it caused timing races:
   Raylib's miniaudio ring buffer is 2×N frames (double-buffered).  When
   the game renders slightly faster than 60 fps the stream's sub-buffers
   are sometimes both still full, so UpdateAudioStream silently skips the
   write.  Skipped frames drift the ring-buffer write-pointer, mixing old
   and new data → "wavy / echoing" artifact with a slight pitch shift.

   Using 2048 frames (≈ 46 ms) with an IsAudioStreamProcessed gate fixes
   this: we only generate + push when the stream has genuinely consumed one
   sub-buffer, so there is always room and no data is ever dropped.
   Sound-instance state (samples_remaining) still advances correctly
   because game_get_audio_samples is only called when a push is due.     */
#define SAMPLES_PER_FRAME 2048

/* Pixel storage (Raylib image updates from this) */
static uint32_t g_pixel_buf[SCREEN_W * SCREEN_H];

/* ══════ platform_init ══════════════════════════════════════════════════════ */
void platform_init(void) {
    /* Initialise window — FLAG_WINDOW_RESIZABLE lets users drag to any size;
       the game canvas is always centered and letterboxed inside it.        */
    InitWindow(SCREEN_W, SCREEN_H, "Asteroids");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    /* Create a Raylib texture from our pixel buffer.
       Image is a CPU-side image; Texture2D lives on the GPU.             */
    Image img = {
        .data    = g_pixel_buf,
        .width   = SCREEN_W,
        .height  = SCREEN_H,
        .mipmaps = 1,
        /* PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 means:
           bytes in memory are [R][G][B][A] — matches GAME_RGBA.          */
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    g_texture = LoadTextureFromImage(img);
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);

    /* Initialise audio device */
    InitAudioDevice();

    /* Size the audio stream buffer to exactly SAMPLES_PER_FRAME frames.
       miniaudio double-buffers internally, so the actual ring buffer holds
       2 × SAMPLES_PER_FRAME frames (≈ 93 ms total).
       This MUST be called BEFORE LoadAudioStream.                        */
    SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME);

    /* Create a streaming audio channel: 44100 Hz, 16-bit, stereo */
    g_audio_stream = LoadAudioStream(44100, 16, 2);
    PlayAudioStream(g_audio_stream);
}

/* ══════ platform_shutdown ══════════════════════════════════════════════════ */
void platform_shutdown(void) {
    StopAudioStream(g_audio_stream);
    UnloadAudioStream(g_audio_stream);
    CloseAudioDevice();
    UnloadTexture(g_texture);
    CloseWindow();
}

/* ══════ process_input — map Raylib keys to GameInput ══════════════════════

   Raylib provides IsKeyDown (held) and IsKeyPressed (just pressed this frame).
   We map these to our GameButtonState fields.

   COURSE NOTE: We simulate half_transition_count=1 when a key is just
   pressed or released this frame (IsKeyPressed / IsKeyReleased).         */
static void process_input(const GameInput *old, GameInput *current) {
    /* Carry last frame's held state and reset transition count */
    prepare_input_frame(old, current);

    /* Helper to update one button from a Raylib key */
    #define MAP_KEY(BTN, KEY1, KEY2)                                  \
        do {                                                           \
            int held = IsKeyDown(KEY1) || IsKeyDown(KEY2);            \
            if (held != (BTN).ended_down) {                           \
                (BTN).ended_down = held;                              \
                (BTN).half_transition_count++;                        \
            }                                                          \
        } while(0)

    MAP_KEY(current->left,  KEY_LEFT,   KEY_A);
    MAP_KEY(current->right, KEY_RIGHT,  KEY_D);
    MAP_KEY(current->up,    KEY_UP,     KEY_W);
    MAP_KEY(current->fire,  KEY_SPACE,  KEY_ENTER);

    #undef MAP_KEY

    /* Quit */
    if (WindowShouldClose() ||
        IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
        current->quit = 1;
    }
}

/* ══════ main ═══════════════════════════════════════════════════════════════

   The Raylib game loop.  Raylib manages the window event pump internally;
   we just need to call WindowShouldClose() to check for close events.    */
int main(void) {
    /* ── Initialise platform ─────────────────────────────────────────── */
    platform_init();

    /* ── Initialise game state ───────────────────────────────────────── */
    GameState state;
    memset(&state, 0, sizeof(state));
    state.audio.samples_per_second = 44100;
    game_audio_init(&state);
    asteroids_init(&state);

    /* ── Pixel backbuffer ─────────────────────────────────────────────── */
    AsteroidsBackbuffer bb;
    bb.pixels = g_pixel_buf;
    bb.width  = SCREEN_W;
    bb.height = SCREEN_H;
    bb.pitch  = SCREEN_W * 4;

    /* ── Audio output buffer ─────────────────────────────────────────── */
    int16_t audio_samples[SAMPLES_PER_FRAME * 2];  /* stereo interleaved  */
    AudioOutputBuffer audio_buf;
    audio_buf.samples            = audio_samples;
    audio_buf.samples_per_second = 44100;
    audio_buf.sample_count       = SAMPLES_PER_FRAME;

    /* ── Input double-buffer ─────────────────────────────────────────── */
    GameInput inputs[2];
    memset(inputs, 0, sizeof(inputs));

    /* ══════ Main Game Loop ════════════════════════════════════════════ */
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        /* Clamp dt to prevent huge jumps after pause/focus loss */
        if (dt > 0.066f) dt = 0.066f;
        if (dt < 0.0f)   dt = 0.0f;

        /* ── Input ────────────────────────────────────────────────────── */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        process_input(&inputs[0], &inputs[1]);

        if (inputs[1].quit) break;

        /* ── Update ───────────────────────────────────────────────────── */
        asteroids_update(&state, &inputs[1], dt);

        /* ── Render ──────────────────────────────────────────────────── */
        asteroids_render(&state, &bb);

        /* ── Audio: generate + push only when the stream has room ───────
           IsAudioStreamProcessed returns true when at least one of the
           two internal sub-buffers (each SAMPLES_PER_FRAME frames) has
           been fully consumed by miniaudio's audio thread.

           We ONLY call game_get_audio_samples when we are about to push.
           This is intentional: sound-instance counters (samples_remaining,
           phase, freq) advance only when audio is actually generated, so
           the mixer state stays perfectly aligned with what the hardware
           is playing.  Because SAMPLES_PER_FRAME = 2048 (≈ 46 ms) the
           gate fires roughly every 2-3 game frames — game logic updates
           every frame as normal; only audio is batched.

           Do NOT call game_get_audio_samples unconditionally every frame:
           that causes the ring-buffer write-pointer to drift ahead of the
           read-pointer, mixing stale and fresh data → "wavy/echoing" with
           a slight pitch shift.                                            */
        if (IsAudioStreamProcessed(g_audio_stream)) {
            game_get_audio_samples(&state, &audio_buf);
            UpdateAudioStream(g_audio_stream, audio_samples, SAMPLES_PER_FRAME);
        }

        /* ── Display: upload pixels to GPU texture, centered + letterboxed ─
           Scale the fixed-size game canvas to fit the (possibly resized)
           window while keeping the original aspect ratio.  Black bars fill
           the unused area on either side (pillarbox) or top/bottom
           (letterbox).                                                      */
        UpdateTexture(g_texture, g_pixel_buf);

        {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float sx = (float)sw / (float)SCREEN_W;
            float sy = (float)sh / (float)SCREEN_H;
            float sc = (sx < sy) ? sx : sy;
            int vw = (int)((float)SCREEN_W * sc);
            int vh = (int)((float)SCREEN_H * sc);
            int vx = (sw - vw) / 2;
            int vy = (sh - vh) / 2;

            BeginDrawing();
                ClearBackground(BLACK);
                DrawTexturePro(g_texture,
                               (Rectangle){0, 0, (float)SCREEN_W, (float)SCREEN_H},
                               (Rectangle){(float)vx, (float)vy,
                                           (float)vw,  (float)vh},
                               (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            EndDrawing();
        }
    }
    /* ══════ End of Game Loop ══════════════════════════════════════════ */

    platform_shutdown();
    return 0;
}
