#ifndef DE100_HOOKS_AUDIO_H
#define DE100_HOOKS_AUDIO_H

#include "../../../_common/base.h"

// ═══════════════════════════════════════════════════════════════════════
// PLATFORM HOOKS - Audio
//
// Platform-agnostic audio control that game code can call without
// knowing which backend is running. Mirrors the utils.h pattern.
//
// Each backend implements these in platforms/[backend]/hooks/audio.c
// ═══════════════════════════════════════════════════════════════════════

/** Get estimated audio output latency in milliseconds. */
f32 de100_audio_get_latency_ms(void);

/** Get number of audio callback underruns since last reset. */
i32 de100_audio_get_underrun_count(void);

/** Set platform-level master volume (0.0 to 1.0). */
void de100_audio_set_master_volume(f32 volume);

/** Pause audio output (e.g., on focus loss). */
void de100_audio_pause(void);

/** Resume audio output (e.g., on focus gain). */
void de100_audio_resume(void);

#endif // DE100_HOOKS_AUDIO_H
