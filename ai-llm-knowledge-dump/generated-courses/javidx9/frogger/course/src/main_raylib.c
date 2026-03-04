/* =============================================================================
 * main_raylib.c — Cross-Platform Backend (Raylib)
 * =============================================================================
 *
 * This file implements the four platform functions declared in platform.h
 * using Raylib — a simple game library that handles windowing, input,
 * rendering, and audio through a unified high-level API.
 *
 * COMPARED TO THE X11 BACKEND:
 *   Raylib:  ~130 lines — Raylib does all the heavy lifting
 *   X11:     ~280 lines — manual X11 + GLX + ALSA
 *   Same game logic, vastly different implementation complexity.
 *
 * AUDIO MODEL (streaming, not callback):
 *   1. Create an AudioStream at 44100 Hz, 16-bit, stereo.
 *   2. PlayAudioStream() immediately — streams silence until fed data.
 *   3. Each frame: if IsAudioStreamProcessed(), generate + push one block.
 *      UpdateAudioStream() third argument = stereo FRAMES (not samples).
 *
 *   WHY SAMPLES_PER_FRAME = 2048 (not 735):
 *     Raylib's miniaudio ring buffer is 2 × SAMPLES_PER_FRAME frames.
 *     At 60 fps the game loop calls UpdateAudioStream ~60× per second, but
 *     miniaudio's audio thread consumes 44100 frames/s = ~735 frames/frame.
 *     If we push 735 frames every game frame and the ring buffer is still
 *     full (both sub-buffers occupied) miniaudio silently drops the push.
 *     The write-pointer drifts ahead of the read-pointer → old and new data
 *     get mixed → "wavy/echoing" artifact with a slight pitch shift.
 *
 *     Using 2048 frames (≈ 46 ms) with an IsAudioStreamProcessed gate fixes
 *     this: we only generate + push when at least one sub-buffer has been
 *     fully consumed, so there is always room and no data is ever dropped.
 *
 * PIXEL FORMAT:
 *   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 matches GAME_RGBA byte layout.
 *   Memory layout: [R][G][B][A] per pixel — exactly what Raylib expects.
 *
 * COURSE NOTES (improvements over reference frogger source):
 *   1. Per-frame streaming audio — no callbacks, no race conditions
 *   2. FLAG_WINDOW_RESIZABLE + DrawTexturePro letterbox (from Lesson 01)
 *   3. inputs[2] double-buffer for correct "just pressed" detection
 *   4. platform_shutdown() for clean teardown
 *   5. PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 matches GAME_RGBA exactly
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

/* Number of stereo frames per audio push (see rationale above).
   Do NOT reduce this to 735 — it causes the "wavy/echoing" audio bug.   */
#define SAMPLES_PER_FRAME 2048

/* Pixel storage — game renders here; we upload to g_texture each frame   */
static uint32_t g_pixel_buf[SCREEN_W * SCREEN_H];

/* ══════ platform_init ══════════════════════════════════════════════════════ */
int platform_init(PlatformGameProps *props) {
    /* Resizable window — game canvas is always letterboxed inside it.    */
    InitWindow(props->width, props->height, props->title);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    /* CPU-side image → GPU texture.
       PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: bytes in memory = [R][G][B][A]
       — matches GAME_RGBA(r,g,b,a) = (a<<24)|(b<<16)|(g<<8)|r exactly.  */
    Image img = {
        .data    = g_pixel_buf,
        .width   = SCREEN_W,
        .height  = SCREEN_H,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    g_texture = LoadTextureFromImage(img);
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);  /* crisp pixels   */

    /* Audio */
    InitAudioDevice();
    /* Must be called BEFORE LoadAudioStream — sets internal ring buffer size */
    SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME);
    g_audio_stream = LoadAudioStream(44100, 16, 2);
    PlayAudioStream(g_audio_stream);

    /* Backbuffer pointer */
    props->backbuffer.pixels = g_pixel_buf;
    props->backbuffer.width  = SCREEN_W;
    props->backbuffer.height = SCREEN_H;
    props->backbuffer.pitch  = SCREEN_W * 4;
    props->is_running        = 1;

    return 1;
}

/* ══════ platform_get_time ══════════════════════════════════════════════════ */
double platform_get_time(void) {
    return GetTime();
}

/* ══════ platform_get_input ═════════════════════════════════════════════════

   Maps Raylib key state to our GameButtonState.  IsKeyDown detects held keys;
   we compare against last frame's ended_down to record transitions.       */
void platform_get_input(GameInput *input, PlatformGameProps *props) {
    /* Helper: detect a state change and record one transition */
    #define MAP_KEY(BTN, KEY1, KEY2)                             \
        do {                                                      \
            int held = IsKeyDown(KEY1) || IsKeyDown(KEY2);       \
            if (held != (BTN).ended_down) {                      \
                (BTN).ended_down = held;                         \
                (BTN).half_transition_count++;                   \
            }                                                     \
        } while(0)

    MAP_KEY(input->move_up,    KEY_UP,    KEY_W);
    MAP_KEY(input->move_down,  KEY_DOWN,  KEY_S);
    MAP_KEY(input->move_left,  KEY_LEFT,  KEY_A);
    MAP_KEY(input->move_right, KEY_RIGHT, KEY_D);

    if (IsKeyPressed(KEY_R)) input->restart = 1;

    #undef MAP_KEY

    if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
        input->quit = 1;
        props->is_running = 0;
    }
}

/* ══════ platform_shutdown ══════════════════════════════════════════════════ */
void platform_shutdown(void) {
    StopAudioStream(g_audio_stream);
    UnloadAudioStream(g_audio_stream);
    CloseAudioDevice();
    UnloadTexture(g_texture);
    CloseWindow();
}

/* ══════ main ═══════════════════════════════════════════════════════════════

   The Raylib game loop.  Raylib's internal event pump runs inside
   WindowShouldClose() — we just check the result each frame.             */
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
        if (dt > 0.066f) dt = 0.066f;
        if (dt < 0.0f)   dt = 0.0f;

        /* ── Input ────────────────────────────────────────────────────── */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);

        if (inputs[1].quit || !props.is_running) break;

        /* ── Update ───────────────────────────────────────────────────── */
        game_update(&state, &inputs[1], dt);

        /* ── Render ──────────────────────────────────────────────────── */
        game_render(&props.backbuffer, &state);

        /* ── Audio: generate + push only when the stream has consumed a
           sub-buffer (IsAudioStreamProcessed gate).
           Do NOT call game_get_audio_samples unconditionally — that causes
           the ring-buffer write-pointer to drift, producing "wavy/echoing"
           audio with a slight pitch shift (see file header for details).  */
        if (IsAudioStreamProcessed(g_audio_stream)) {
            game_get_audio_samples(&state, &audio_buf);
            UpdateAudioStream(g_audio_stream, audio_samples, SAMPLES_PER_FRAME);
        }

        /* ── Display: upload pixels, scale + centre via DrawTexturePro ──
           Letterbox algorithm:
             sx = window_w / SCREEN_W,  sy = window_h / SCREEN_H
             sc = min(sx, sy)           — whichever axis is tighter
             viewport = sc * game canvas, centred in window               */
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
                    (Rectangle){(float)vx, (float)vy, (float)vw, (float)vh},
                    (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            EndDrawing();
        }
    }
    /* ══════ End of Game Loop ══════════════════════════════════════════ */

    platform_shutdown();
    return 0;
}
