/*
 * main_raylib.c  —  Sugar, Sugar | Raylib Platform Backend
 *
 * Implements the platform.h contract using Raylib 5.x.
 *
 * How the backbuffer reaches the screen:
 *   game_render() writes ARGB pixels into bb.pixels
 *   → R and B channels are swapped in-place (ARGB → ABGR ≡ Raylib RGBA)
 *   → UpdateTexture() uploads them to the GPU
 *   → DrawTextureEx() renders the texture with letterbox scaling
 *   → channels are swapped back so game state is unaffected
 *
 * Audio (AudioStream model):
 *   InitAudioDevice → SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE)
 *   → LoadAudioStream → pre-fill 2×silence → PlayAudioStream
 *   Per-frame: while (IsAudioStreamProcessed) { game_get_audio_samples; UpdateAudioStream }
 *   This is the same pattern Casey Muratori uses: the platform asks the game
 *   for samples rather than the game pushing them.
 *
 * Build (output always goes to build/game):
 *   clang -Wall -Wextra -O0 -g -DDEBUG -fsanitize=address,undefined \
 *         -o build/game \
 *         src/main_raylib.c src/game.c src/levels.c src/audio.c \
 *         -lraylib -lm -lpthread -ldl
 */

#include "raylib.h"

#include <math.h>    /* fminf  */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset       */

#include "platform.h"

/* ===================================================================
 * AUDIO CONSTANTS
 * =================================================================== */
#define SAMPLES_PER_SECOND 44100
/*
 * AUDIO_CHUNK_SIZE is the number of stereo frames (L+R pairs) we generate
 * and push to Raylib in one call.  This value is used for BOTH:
 *   1. SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE) — registers
 *      Raylib's internal ring-buffer segment size
 *   2. UpdateAudioStream(stream, buf, AUDIO_CHUNK_SIZE) — fills that segment
 *
 * CRITICAL: These two values MUST be identical.  A mismatch causes Raylib to
 * misalign its internal ring-buffer write pointer — you hear repeated or
 * skipped audio (stuttering) every second.
 *
 * Why 2048 (≈46 ms) and NOT 735 (44100/60 Hz)?
 *   IsAudioStreamProcessed() fires on hardware-buffer boundaries, not exact
 *   frame intervals.  At 735 frames any OS scheduling jitter causes
 *   consecutive pushes before the previous chunk is consumed — UpdateAudioStream
 *   silently drops the write, causing wavy/echoing audio with a pitch shift.
 *   2048 frames gives 46 ms of headroom that absorbs all normal OS jitter.
 */
#define AUDIO_CHUNK_SIZE  2048

/* ===================================================================
 * PLATFORM GLOBALS
 * ===================================================================
 * g_scale / g_offset_x / g_offset_y are set each frame in
 * platform_display_backbuffer and read in platform_get_input to
 * transform window-space mouse coordinates into canvas-space coords.
 * Without this, clicking anywhere in the letterbox is off by the
 * border pixels and doesn't scale correctly when the window is resized.
 */
static Texture2D g_texture;
static int       g_tex_w;
static int       g_tex_h;

/* Letterbox globals — written by platform_display_backbuffer */
static float g_scale    = 1.0f;
static int   g_offset_x = 0;
static int   g_offset_y = 0;

/* Audio globals */
static AudioStream g_stream;
static int16_t     g_sample_buffer[AUDIO_CHUNK_SIZE * 4]; /* stereo; 4× for headroom */
static int         g_audio_ready = 0;

/* ===================================================================
 * PLATFORM IMPLEMENTATION
 * =================================================================== */

void platform_init(const char *title, int width, int height) {
    SetTraceLogLevel(LOG_WARNING); /* suppress verbose Raylib log spam */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title);

    /* Create a texture with the same dimensions as the canvas.
     * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 = 4 bytes per pixel, RGBA order.
     * We swap R↔B before each UpdateTexture call so our ARGB pixel data
     * lands in the correct channels. */
    Image img = {
        .data    = NULL,
        .width   = width,
        .height  = height,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_texture = LoadTextureFromImage(img);
    g_tex_w   = width;
    g_tex_h   = height;

    SetTargetFPS(60);
}

double platform_get_time(void) {
    return GetTime(); /* Raylib monotonic clock, seconds */
}

void platform_get_input(GameInput *input) {
    /* Mouse position — transform from window coords to canvas coords.
     *
     * GetMousePosition() returns window-space coords.  When the window
     * is resized the canvas is letterboxed: scaled to fit with black
     * borders.  We must undo that scale + offset to get canvas coords.
     *
     * g_scale and g_offset_* are updated in platform_display_backbuffer
     * (which runs before platform_get_input in the main loop).
     */
    Vector2 pos = GetMousePosition();
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;

    /* Map window → canvas.  Clamp so out-of-canvas positions stay on edge. */
    int cx = (int)((pos.x - (float)g_offset_x) / g_scale);
    int cy = (int)((pos.y - (float)g_offset_y) / g_scale);
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx >= CANVAS_W) cx = CANVAS_W - 1;
    if (cy >= CANVAS_H) cy = CANVAS_H - 1;
    input->mouse.x = cx;
    input->mouse.y = cy;

    /* Left mouse button */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        UPDATE_BUTTON(input->mouse.left, 1);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        UPDATE_BUTTON(input->mouse.left, 0);

    /* Keys */
    if (IsKeyPressed(KEY_ESCAPE))   UPDATE_BUTTON(input->escape,  1);
    if (IsKeyReleased(KEY_ESCAPE))  UPDATE_BUTTON(input->escape,  0);
    if (IsKeyPressed(KEY_R))        UPDATE_BUTTON(input->reset,   1);
    if (IsKeyReleased(KEY_R))       UPDATE_BUTTON(input->reset,   0);
    if (IsKeyPressed(KEY_G))        UPDATE_BUTTON(input->gravity, 1);
    if (IsKeyReleased(KEY_G))       UPDATE_BUTTON(input->gravity, 0);

    /* Enter / Space — confirm or advance */
    if (IsKeyPressed(KEY_ENTER)  || IsKeyPressed(KEY_SPACE))
        UPDATE_BUTTON(input->enter, 1);
    if (IsKeyReleased(KEY_ENTER) || IsKeyReleased(KEY_SPACE))
        UPDATE_BUTTON(input->enter, 0);
}

void platform_display_backbuffer(const GameBackbuffer *backbuffer) {
    /*
     * Pixel format fix: our pixels are 0xAARRGGBB (ARGB), stored
     * little-endian as bytes [B, G, R, A].  Raylib's RGBA format reads
     * those same bytes as [R=B, G=G, B=R, A=A] — swapping red and blue.
     *
     * We fix this with an in-place R↔B swap before the GPU upload and
     * a matching swap-back afterward so game state remains unaffected.
     */
    uint32_t *px    = backbuffer->pixels;
    int        total = backbuffer->width * backbuffer->height;

    for (int i = 0; i < total; i++) {
        uint32_t c = px[i];
        px[i] = (c & 0xFF00FF00u)
              | ((c & 0x00FF0000u) >> 16)
              | ((c & 0x000000FFu) << 16);
    }

    UpdateTexture(g_texture, backbuffer->pixels);

    for (int i = 0; i < total; i++) {
        uint32_t c = px[i];
        px[i] = (c & 0xFF00FF00u)
              | ((c & 0x00FF0000u) >> 16)
              | ((c & 0x000000FFu) << 16);
    }

    /* Letterbox: scale the 640×480 canvas to fit the current window while
     * preserving aspect ratio, centering it with black borders.
     * Store scale/offset in globals so platform_get_input can transform
     * mouse coordinates correctly. */
    int   win_w  = GetScreenWidth();
    int   win_h  = GetScreenHeight();
    float scaleX = (float)win_w / (float)CANVAS_W;
    float scaleY = (float)win_h / (float)CANVAS_H;
    g_scale    = (scaleX < scaleY) ? scaleX : scaleY; /* fminf without <math.h> */
    int dst_w  = (int)((float)CANVAS_W * g_scale);
    int dst_h  = (int)((float)CANVAS_H * g_scale);
    g_offset_x = (win_w - dst_w) / 2;
    g_offset_y = (win_h - dst_h) / 2;

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTextureEx(g_texture,
                  (Vector2){(float)g_offset_x, (float)g_offset_y},
                  0.0f,     /* rotation */
                  g_scale,
                  WHITE);
    EndDrawing();
}

/* -----------------------------------------------------------------------
 * Audio (AudioStream model)
 *
 * The platform asks the game for samples — game_get_audio_samples() is our
 * "AudioWorklet process()" equivalent.  We never call PlaySound/LoadSound;
 * those hand control to Raylib's scheduler, bypassing our PCM mixer.
 *
 * Key rules (from games/tetris/src/main_raylib.c reference):
 *   1. SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE) BEFORE LoadAudioStream
 *   2. UpdateAudioStream must always use the SAME count: AUDIO_CHUNK_SIZE
 *   3. Pre-fill 2 silent chunks before PlayAudioStream (prevents startup click)
 *   4. In the frame loop, fill ALL available buffers (max 4 per frame) so we
 *      never fall behind even after a frame spike
 * ----------------------------------------------------------------------- */
void platform_audio_init(GameState *state, int samples_per_second) {
    /* Wire sample rate into the game-side audio state FIRST */
    game_audio_init(&state->audio, samples_per_second);

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        g_audio_ready = 0;
        return;
    }

    /* CRITICAL: set buffer size BEFORE LoadAudioStream.
     * This is the size Raylib allocates per internal ring-buffer segment.
     * It MUST equal the count passed to UpdateAudioStream — otherwise the
     * ring-buffer write pointer drifts and you hear stutter/repeats. */
    SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);

    g_stream = LoadAudioStream(samples_per_second, 16 /* bits */, 2 /* stereo */);
    if (!IsAudioStreamValid(g_stream)) {
        CloseAudioDevice();
        g_audio_ready = 0;
        return;
    }

    /* Pre-fill 2 silent chunks before starting playback.
     * Raylib is double-buffered — this ensures both buffers have data so
     * the audio thread never starves on the very first frame. */
    memset(g_sample_buffer, 0, sizeof(g_sample_buffer));
    UpdateAudioStream(g_stream, g_sample_buffer, AUDIO_CHUNK_SIZE);
    UpdateAudioStream(g_stream, g_sample_buffer, AUDIO_CHUNK_SIZE);

    PlayAudioStream(g_stream);
    g_audio_ready = 1;
}

void platform_audio_update(GameState *state) {
    if (!g_audio_ready) return;

    /* Fill ALL consumed buffers this frame.
     * Raylib is double-buffered; after a frame spike we may need to fill
     * up to 4 chunks.  Capping at 4 prevents a spiral if audio lags badly. */
    int filled = 0;
    while (filled < 4 && IsAudioStreamProcessed(g_stream)) {
        AudioOutputBuffer buf = {
            .samples            = g_sample_buffer,
            .samples_per_second = SAMPLES_PER_SECOND,
            .sample_count       = AUDIO_CHUNK_SIZE,
        };
        game_get_audio_samples(state, &buf);
        /* MUST pass AUDIO_CHUNK_SIZE — never a different value */
        UpdateAudioStream(g_stream, g_sample_buffer, AUDIO_CHUNK_SIZE);
        filled++;
    }
}

void platform_audio_shutdown(void) {
    if (!g_audio_ready) return;
    StopAudioStream(g_stream);
    UnloadAudioStream(g_stream);
    CloseAudioDevice();
    g_audio_ready = 0;
}

/* ===================================================================
 * MAIN LOOP
 * =================================================================== */

int main(void) {
    static GameState      state;
    static GameBackbuffer bb;

    /* Allocate the pixel buffer (640 × 480 × 4 bytes ≈ 1.2 MB) */
    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1;

    platform_init("Sugar, Sugar", CANVAS_W, CANVAS_H);
    game_init(&state, &bb);
    platform_audio_init(&state, SAMPLES_PER_SECOND);
    /* game_audio_init (called inside platform_audio_init) resets audio state
     * with memset — it wipes the is_playing flag that game_init set via
     * change_phase(PHASE_TITLE).  Retrigger title music here after audio is
     * fully initialised so the sequencer starts immediately on launch. */
    game_music_play_title(&state.audio);

    GameInput input;
    memset(&input, 0, sizeof(input));

    double prev_time = platform_get_time();

    while (!WindowShouldClose() && !state.should_quit) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;
        if (delta_time > 0.1f) delta_time = 0.1f;

        platform_audio_update(&state);

        prepare_input_frame(&input);
        platform_get_input(&input);
        game_update(&state, &input, delta_time);
        game_render(&state, &bb);
        platform_display_backbuffer(&bb);
    }

    platform_audio_shutdown();
    UnloadTexture(g_texture);
    CloseWindow();
    free(bb.pixels);
    return 0;
}

