/* =============================================================================
 * utils/audio.h — Audio System Types for Frogger
 * =============================================================================
 *
 * THREE LAYERS OF AUDIO:
 *
 *   1. ONE-SHOT SFX — triggered by game events (hop, death, win, etc.).
 *      Short, sharp sounds that fire once and expire naturally.
 *
 *   2. BACKGROUND MUSIC — a chiptune melody driven by a note sequencer.
 *      game_music_update(state, dt) advances the sequencer each game frame
 *      and triggers individual notes as short SoundInstances.
 *      Separate `music_volume` control; does not compete with SFX.
 *
 *   3. AMBIENT SOUNDS — continuously looping environmental layers:
 *      • SOUND_AMBIENT_TRAFFIC  — low engine/tyre drone from road lanes.
 *      • SOUND_AMBIENT_WATER    — flowing-river white noise from river lanes.
 *      These use `looping = 1` (SoundInstance auto-restarts on expiry) and
 *      `vol_scale` which is updated every frame by game_update_ambients() to
 *      smoothly fade based on the frog's distance from each zone.
 *      Separate `ambient_volume` ceiling; always quieter than SFX.
 *
 * VOLUME HIERARCHY:
 *   final_amp = raw × envelope × def.volume × inst.vol_scale × master_volume
 *   SFX:     inst.vol_scale = sfx_volume    (set when triggered)
 *   Music:   inst.vol_scale = music_volume  (set when note starts)
 *   Ambient: inst.vol_scale = 0..ambient_volume (updated each game frame)
 *
 * STEREO PANNING:
 *   SFX panned by frog_x:
 *     pan = (frog_x / max_tile_x) * 2.0f - 1.0f   [-1=left, 0=centre, +1=right]
 *     gain_l = (pan <= 0) ? 1.0 : (1.0 - pan)
 *     gain_r = (pan >= 0) ? 1.0 : (1.0 + pan)
 *   Ambient traffic panned to where the nearest car is on screen.
 *   Ambient water centred (river lanes span the full width).
 *
 * JS equivalent (Web Audio API):
 *   const sfxGain     = audioCtx.createGain();  sfxGain.gain.value     = 0.7;
 *   const musicGain   = audioCtx.createGain();  musicGain.gain.value   = 0.4;
 *   const ambientGain = audioCtx.createGain();  ambientGain.gain.value = 0.2;
 * =============================================================================
 */

#ifndef FROGGER_AUDIO_H
#define FROGGER_AUDIO_H

#include <stdint.h>

/* ══════ Sound IDs ══════════════════════════════════════════════════════════

   Three categories of sound:
     • One-shot SFX   — triggered by game events, expire on their own.
     • Music channel  — one note at a time, driven by the note sequencer.
     • Ambient loops  — looping background layers (looping=1, vol modulated).

   SOUND_COUNT is always last — used to size the SOUND_DEFS[] table.      */
typedef enum {
    SOUND_NONE = 0,

    /* ── One-shot SFX ──────────────────────────────────────────────────── */
    SOUND_HOP,            /* short tick on every frog hop (up/down/left/right) */
    SOUND_SQUASH,         /* percussive noise burst — car hit on road          */
    SOUND_SPLASH,         /* descending noise burst — water/drowning death     */
    SOUND_HOME_REACHED,   /* ascending tone when one home cell is filled       */
    SOUND_WIN_JINGLE,     /* ascending sweep across an octave — all 5 homes   */
    SOUND_GAME_OVER,      /* slow descending chord — all lives lost            */
    SOUND_LEVEL_START,    /* short ascending arpeggio at game start / restart  */

    /* ── Music channel (frequency overridden per note by sequencer) ────── */
    SOUND_MUSIC,          /* one melodic note; repeated by game_music_update() */

    /* ── Ambient loops (looping=1, vol_scale modulated by game) ────────── */
    SOUND_AMBIENT_TRAFFIC, /* low engine/tyre drone — road lanes 5-8          */
    SOUND_AMBIENT_WATER,   /* flowing river noise — river lanes 1-3           */

    SOUND_COUNT            /* always last — used as array length              */
} SOUND_ID;

/* ══════ Capacity ═══════════════════════════════════════════════════════════
   16 slots: 2 ambient loops + 1 music note + up to 13 simultaneous SFX.
   Frogger rarely fires more than 3 SFX at once, so this is generous.     */
#define MAX_SIMULTANEOUS_SOUNDS 16

/* ══════ SoundInstance — one actively playing sound ════════════════════════

   When game_play_sound() / game_play_looping() is called, a SoundInstance
   slot is activated.  The mixer reads from it each audio frame.

   looping:    if 1, samples_remaining resets to total_samples on expiry
               instead of the slot being freed.  Used for ambient sounds.
   vol_scale:  per-instance final volume multiplier.  Replaces the old
               global sfx_volume application; each instance carries its own
               effective volume so SFX, music, and ambients can be scaled
               independently without extra state.                          */
typedef struct {
    SOUND_ID sound_id;          /* which SoundDef to synthesise            */
    int      samples_remaining; /* audio frames left until next loop/expiry */
    int      total_samples;     /* total samples for one play / loop cycle  */
    float    phase;             /* oscillator phase accumulator [0, 1)      */
    float    freq;              /* current frequency in Hz (can slide)      */
    float    pan_left;          /* left-channel gain  [0, 1]                */
    float    pan_right;         /* right-channel gain [0, 1]                */
    int      looping;           /* 1 = auto-restart; 0 = expire             */
    float    vol_scale;         /* effective volume = def.vol × vol_scale × master */
} SoundInstance;

/* ══════ GameAudioState — all per-game audio data ══════════════════════════

   Embedded inside GameState.  game_init() memsets GameState, so fields that
   must survive a restart are saved before and restored after the memset:
     samples_per_second, master_volume, sfx_volume, music_volume,
     ambient_volume.                                                        */
typedef struct {
    SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
    int           active_sound_count;

    /* Set by platform before game_init() is called. */
    int   samples_per_second;

    /* Volume controls [0, 1].
       master_volume  — final gain applied to the entire mix output.
       sfx_volume     — scale applied to one-shot SFX (vol_scale at trigger).
       music_volume   — scale applied to each music note (vol_scale at trigger).
       ambient_volume — ceiling for ambient vol_scale (updated each frame). */
    float master_volume;
    float sfx_volume;
    float music_volume;
    float ambient_volume;

    /* Music sequencer state — advanced by game_music_update(state, dt).
       Note index wraps around MUSIC_NOTE_COUNT when it reaches the end.   */
    int   music_note_idx;    /* index into MUSIC_NOTES[]                   */
    float music_note_timer;  /* time remaining on the current note (s)     */
    int   music_playing;     /* 1 = sequencer running; 0 = paused          */
} GameAudioState;

/* ══════ AudioOutputBuffer — passed from platform to game ══════════════════

   The platform allocates a fixed-size PCM buffer and passes it to
   game_get_audio_samples().  The game fills it; the platform sends it to
   ALSA (X11) or the Raylib AudioStream.

   STEREO LAYOUT: interleaved as [L0, R0, L1, R1, …]
   sample_count   = number of stereo frames to fill.
   Total int16_t  = sample_count * 2.
   int16_t range  = −32768 … +32767 (signed 16-bit, CD quality)           */
typedef struct {
    int16_t *samples;            /* output: interleaved stereo int16_t     */
    int      sample_count;       /* stereo frames to fill                  */
    int      samples_per_second; /* sample rate (44100 Hz typical)         */
} AudioOutputBuffer;

#endif /* FROGGER_AUDIO_H */
