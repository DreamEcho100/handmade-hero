/* =============================================================================
 * platforms/raylib/main.c — Raylib Platform Backend
 * =============================================================================
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * This file implements the Raylib platform layer.  It is responsible for:
 *   • Opening a window and running the game loop
 *   • Mapping Raylib keyboard events to our GameInput abstraction
 *   • Uploading the game's pixel backbuffer to the GPU each frame
 *   • Streaming PCM audio via Raylib's AudioStream API
 *
 * COMPARED TO THE X11 BACKEND:
 *   Raylib:  ~130 lines — Raylib does all the heavy lifting
 *   X11:     ~280 lines — manual X11 + GLX + ALSA
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
 *   We keep a CPU-side g_pixel_buf[] that the game writes into each frame.
 *   After rendering we call UpdateTexture() to upload it to the GPU, then
 *   DrawTexturePro() to blit it letterboxed into the window.
 *
 * PIXEL FORMAT:
 *   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 matches GAME_RGBA byte layout.
 *   Memory: [R][G][B][A] per pixel — exactly what Raylib expects.
 *
 * PLATFORM CONTRACT (platform.h):
 *   The game calls asteroids_render(state, bb, world_config).
 *   This file owns the backbuffer storage and the world_config origin.
 *   PlatformGameProps bundles them so the platform can pass both at once.
 * =============================================================================
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "raylib.h"
#include "../../platform.h"

/* ══════ Global Platform State ════════════════════════════════════════════ */
static Texture2D   g_texture;
static AudioStream g_audio_stream;

/* Number of stereo frames per audio push.
   2048 (≈ 46 ms) with the IsAudioStreamProcessed gate prevents ring-buffer
   drift: we only generate + push when miniaudio has consumed one sub-buffer,
   so there is always room and no data is ever dropped.                      */
#define SAMPLES_PER_FRAME 2048

/* Pixel storage (the game writes here; we upload to GPU each frame).
   Size: GAME_W × GAME_H × 4 bytes per pixel (RGBA).                       */
static uint32_t g_pixel_buf[GAME_W * GAME_H];

/* ══════ platform_init ══════════════════════════════════════════════════════
 *
 * Opens the Raylib window, creates the pixel-upload texture, and starts the
 * audio stream.  Called ONCE from main() before the game loop.            */
void platform_init(void) {
    /* FLAG_WINDOW_RESIZABLE lets users drag to any size; the game canvas is
       always centered and letterboxed inside it (see main loop below).     */
    InitWindow(GAME_W, GAME_H, "Asteroids");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    /* Create a Raylib texture from our pixel buffer.
       Image is a CPU-side struct; Texture2D lives on the GPU.              */
    Image img = {
        .data    = g_pixel_buf,
        .width   = GAME_W,
        .height  = GAME_H,
        .mipmaps = 1,
        /* PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: bytes are [R][G][B][A]
           — matches GAME_RGBA macro exactly.                                */
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    g_texture = LoadTextureFromImage(img);
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);

    /* Initialise audio device */
    InitAudioDevice();

    /* Size the audio stream buffer to exactly SAMPLES_PER_FRAME frames.
       miniaudio double-buffers internally: actual ring buffer = 2 × N.
       MUST be called BEFORE LoadAudioStream.                               */
    SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME);

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
 *
 * LESSON 06 — Double-buffered input.
 * prepare_input_frame() carries ended_down from last frame and zeros
 * half_transition_count; then we detect state changes and bump the counter.
 *
 * MAP_KEY detects the transition by comparing old held state with current:
 *   • Newly pressed  → ended_down 0→1, half_transition_count++
 *   • Newly released → ended_down 1→0, half_transition_count++
 *   • No change      → nothing recorded                                    */
static void process_input(const GameInput *old, GameInput *current) {
    prepare_input_frame(old, current);

    #define MAP_KEY(BTN, KEY1, KEY2)                               \
        do {                                                        \
            int held = IsKeyDown(KEY1) || IsKeyDown(KEY2);         \
            if (held != (BTN).ended_down) {                        \
                (BTN).ended_down = held;                           \
                (BTN).half_transition_count++;                     \
            }                                                       \
        } while(0)

    MAP_KEY(current->left,  KEY_LEFT,   KEY_A);
    MAP_KEY(current->right, KEY_RIGHT,  KEY_D);
    MAP_KEY(current->up,    KEY_UP,     KEY_W);
    MAP_KEY(current->fire,  KEY_SPACE,  KEY_ENTER);

    #undef MAP_KEY

    if (WindowShouldClose() ||
        IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
        current->quit = 1;
    }
}

/* ══════ main ═══════════════════════════════════════════════════════════════
 *
 * The Raylib game loop.  Raylib manages the window event pump internally;
 * we call WindowShouldClose() to check for close events.                  */
int main(void) {
    /* ── Initialise platform ─────────────────────────────────────────── */
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

    /* ── Initialise game state ───────────────────────────────────────── */
    GameState state;
    memset(&state, 0, sizeof(state));
    state.audio.samples_per_second = 44100;
    game_audio_init(&state);
    asteroids_init(&state);

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
        /* Clamp dt: if the window was minimised or the system was slow,
           a huge dt would make objects teleport.  Cap at ~4 frames.     */
        if (dt > 0.066f) dt = 0.066f;
        if (dt < 0.0f)   dt = 0.0f;

        /* ── Input ────────────────────────────────────────────────────── */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        process_input(&inputs[0], &inputs[1]);
        if (inputs[1].quit) break;

        /* ── Update ───────────────────────────────────────────────────── */
        asteroids_update(&state, &inputs[1], dt);

        /* ── Render ──────────────────────────────────────────────────── */
        asteroids_render(&state, &props.backbuffer, props.world_config);

        /* ── Audio: generate + push only when the stream has room ───────
           IsAudioStreamProcessed returns true when at least one of the two
           internal sub-buffers has been fully consumed by miniaudio.
           We ONLY call game_get_audio_samples when we are about to push so
           that sound-instance counters (samples_remaining, phase, freq)
           advance only when audio is actually generated.
           Do NOT call it unconditionally: that causes ring-buffer drift
           → "wavy/echoing" artifact with a slight pitch shift.            */
        if (IsAudioStreamProcessed(g_audio_stream)) {
            game_get_audio_samples(&state, &audio_buf);
            UpdateAudioStream(g_audio_stream, audio_samples, SAMPLES_PER_FRAME);
        }

        /* ── Display: upload pixels to GPU, letterboxed into window ──────
           Scale the fixed GAME_W × GAME_H canvas to fit the (possibly
           resized) window while preserving aspect ratio.  Black bars fill
           unused space (pillarboxing or letterboxing).                    */
        UpdateTexture(g_texture, g_pixel_buf);

        {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            int vx, vy, vw, vh;
            platform_compute_letterbox(GAME_W, GAME_H, sw, sh,
                                       &vx, &vy, &vw, &vh);

            BeginDrawing();
                ClearBackground(BLACK);
                DrawTexturePro(g_texture,
                    (Rectangle){0, 0, (float)GAME_W, (float)GAME_H},
                    (Rectangle){(float)vx, (float)vy, (float)vw, (float)vh},
                    (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            EndDrawing();
        }
    }
    /* ══════ End of Game Loop ══════════════════════════════════════════ */

    platform_shutdown();
    return 0;
}
