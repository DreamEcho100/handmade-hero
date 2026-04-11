/* =============================================================================
 * utils/audio.h — Audio System Types for Asteroids
 * =============================================================================
 *
 * WHAT'S IN HERE?
 *   Types and constants for a software sound mixer.  The mixer supports
 *   up to MAX_SIMULTANEOUS_SOUNDS sounds playing concurrently (important for
 *   Asteroids where several asteroid explosions can happen in the same frame).
 *
 * HOW IT WORKS (overview):
 *   1. Game logic calls game_play_sound() / game_play_sound_panned() with a
 *      SOUND_ID to request a sound.  The request is added to active_sounds[].
 *   2. Each audio frame, game_get_audio_samples() iterates active_sounds[],
 *      generates PCM samples for each using additive synthesis, sums them,
 *      and writes the result to the platform's AudioOutputBuffer.
 *   3. The platform writes that buffer to ALSA (X11) or AudioStream (Raylib).
 *
 * WHY ADDITIVE SYNTHESIS (no WAV files)?
 * ───────────────────────────────────────
 * Keeping all sounds procedurally generated means:
 *   • No asset pipeline — nothing to load, no file paths to manage.
 *   • Tiny binary — the "assets" are a handful of floats in the source.
 *   • Educational — you can see exactly how each sound is constructed.
 *   • Portable — works without a file system (useful for bare-metal/WebAssembly).
 *
 * JS equivalent approach (Web Audio API):
 *   const osc = audioCtx.createOscillator();
 *   osc.frequency.setValueAtTime(440, audioCtx.currentTime);
 *   osc.type = 'square';
 *   osc.connect(audioCtx.destination);
 *   osc.start(); osc.stop(audioCtx.currentTime + 0.2);
 *
 * STEREO PANNING:
 *   Asteroid explosions are panned left or right based on the asteroid's
 *   X position in game units: pan = (asteroid.x / GAME_UNITS_W) * 2 - 1,
 *   range [-1, +1].
 *   A pan of -1 = full left, 0 = centre, +1 = full right.
 * =============================================================================
 */

#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H

#include <stdint.h>

/* ══════ Sound IDs ══════════════════════════════════════════════════════════
   An enum gives each sound a human-readable name and a stable integer index.
   JS equivalent: enum SoundID { NONE, THRUST, FIRE, … }  (TypeScript enum)

   SOUND_COUNT is always the last entry; its numeric value equals the number
   of real sound types.  We use it to size lookup tables (SoundDef array).  */
typedef enum {
    SOUND_NONE = 0,
    SOUND_THRUST,          /* low rumble while thrusting — looped by game.c */
    SOUND_FIRE,            /* short ping on bullet fire                      */
    SOUND_EXPLODE_LARGE,   /* deep boom when a large asteroid is split       */
    SOUND_EXPLODE_MEDIUM,  /* mid crack when a medium asteroid is split      */
    SOUND_EXPLODE_SMALL,   /* high snap when a small asteroid is destroyed   */
    SOUND_SHIP_DEATH,      /* descending whomp when player is killed         */
    SOUND_ROTATE,          /* subtle whir while rotating — looped by game.c */
    SOUND_MUSIC_GAMEPLAY,  /* background drone during play — looped by game.c */
    SOUND_MUSIC_RESTART,   /* ascending jingle on game restart              */
    SOUND_COUNT            /* always last — used as array length             */
} SOUND_ID;

/* ══════ Capacity ═══════════════════════════════════════════════════════════
   Asteroids can have many simultaneous explosions (e.g. a bullet hits a large
   asteroid that splits into two mediums, which both get hit by the same burst
   of fire, generating 5 sounds in one frame).  16 slots is safe head-room.  */
#define MAX_SIMULTANEOUS_SOUNDS 16

/* ══════ SoundInstance — one actively playing sound ════════════════════════

   When game_play_sound() is called, a SoundInstance is added to the
   active_sounds[] pool.  The mixer reads from it each frame until
   samples_remaining reaches 0 (or active is cleared).                    */
typedef struct {
    SOUND_ID sound_id;         /* which sound definition to synthesise      */
    int      samples_remaining;/* frames of audio left to generate          */
    int      total_samples;    /* total samples at start (for envelope calc) */
    float    phase;            /* oscillator phase accumulator [0, 1)        */
    float    freq;             /* current frequency (can slide over time)    */
    float    pan_left;         /* left-channel volume  [0, 1]               */
    float    pan_right;        /* right-channel volume [0, 1]               */
} SoundInstance;

/* ══════ GameAudioState — all per-game audio data ══════════════════════════

   Embedded directly inside GameState so audio state moves with the game.
   Fields that must survive asteroids_init()'s memset are saved/restored
   by the caller: samples_per_second, master_volume, sfx_volume.          */
typedef struct {
    SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
    int           active_sound_count;

    /* Platform tells the game how many samples/sec the hardware expects.
       Set by main() before asteroids_init() is called.                   */
    int   samples_per_second;

    /* Volume controls — both in range [0, 1].
       master_volume scales the final mixed output.
       sfx_volume scales each individual SFX before the master.           */
    float master_volume;
    float sfx_volume;
} GameAudioState;

/* ══════ AudioOutputBuffer — buffer passed from platform to game ════════════

   The platform allocates a fixed-size PCM buffer and hands a pointer to
   game_get_audio_samples().  The game fills it; the platform sends it to
   the audio device.

   STEREO LAYOUT: samples are interleaved: [L0, R0, L1, R1, L2, R2, ...]
   sample_count is the number of stereo FRAMES, so the total int16_t count
   is sample_count * 2.

   int16_t range: -32768 … +32767 (signed 16-bit, standard CD quality)   */
typedef struct {
    int16_t *samples;           /* output: interleaved stereo int16_t       */
    int      sample_count;      /* number of stereo frames to fill          */
    int      samples_per_second;/* sample rate (44100 Hz typical)           */
} AudioOutputBuffer;

/* ══════ Helper: clamp a float sample to int16 range ═══════════════════════

   Additive mixing can push values outside [-32767, +32767].  Clamping
   prevents undefined wrap-around behaviour on overflow.                  */
static inline int16_t clamp_sample(float s) {
    if (s >  32767.0f) return  32767;
    if (s < -32768.0f) return -32768;
    return (int16_t)s;
}

/* ══════ Helper: convert centre pan value to L/R gains ════════════════════

   pan: -1.0 = hard left, 0.0 = centre, +1.0 = hard right
   Uses a simple linear pan law:
     left_gain  = (1 - pan) / 2   → 1.0 at pan=-1, 0.5 at pan=0, 0 at pan=1
     right_gain = (1 + pan) / 2   → 0   at pan=-1, 0.5 at pan=0, 1 at pan=1

   JS equivalent: const leftGain = (1 - pan) / 2;  StereoPannerNode uses same */
static inline void calculate_pan_volumes(float pan, float *left, float *right) {
    if (pan < -1.0f) pan = -1.0f;
    if (pan >  1.0f) pan =  1.0f;
    *left  = (1.0f - pan) * 0.5f;
    *right = (1.0f + pan) * 0.5f;
}

#endif /* UTILS_AUDIO_H */
