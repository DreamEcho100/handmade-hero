#ifndef PROJECT_ENGINE_PLATFORM_COMMON_AUDIO_H
#define PROJECT_ENGINE_PLATFORM_COMMON_AUDIO_H

#include <stdint.h>
#include "../../_common/base.h"

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM AUDIO CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════
// This structure is shared across all platform backends.
// Each backend fills in the values during initialization.
//
// Casey's DirectSound equivalent: win32_sound_output
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    // ─────────────────────────────────────────────────────────────────────
    // INITIALIZATION STATE
    // ─────────────────────────────────────────────────────────────────────
    bool is_initialized;
    
    // ─────────────────────────────────────────────────────────────────────
    // HARDWARE CONFIGURATION (set once at init)
    // ─────────────────────────────────────────────────────────────────────
    int32_t samples_per_second;     // e.g., 48000 Hz
    int32_t bytes_per_sample;       // e.g., 4 (16-bit stereo = 2 * 2)
    int32_t secondary_buffer_size;  // Total buffer size in bytes
    
    // ─────────────────────────────────────────────────────────────────────
    // TIMING STATE (updated each frame)
    // ─────────────────────────────────────────────────────────────────────
    int64_t running_sample_index;   // Total samples written (ever)
    
    // ─────────────────────────────────────────────────────────────────────
    // LATENCY CONFIGURATION
    // ─────────────────────────────────────────────────────────────────────
    int32_t latency_sample_count;   // Buffer size in samples
    int32_t safety_sample_count;    // Safety margin (1/3 frame)
    
    // ─────────────────────────────────────────────────────────────────────
    // GAME TIMING
    // ─────────────────────────────────────────────────────────────────────
    int32_t game_update_hz;         // Game logic rate (e.g., 30 Hz)
    
} PlatformAudioConfig;

#endif // PROJECT_ENGINE_PLATFORM_COMMON_AUDIO_H
