#ifndef DE100_PLATFORM_COMMON_AUDIO_H
#define DE100_PLATFORM_COMMON_AUDIO_H

#include <stdint.h>
#include "../../_common/base.h"

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM AUDIO CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════
// This structure is shared across all platform backends.
// Each backend fills in the values during initialization.
//
// Casey's DirectSound equivalent: win32_audio_output
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    // ─────────────────────────────────────────────────────────────────────
    // INITIALIZATION STATE
    // ─────────────────────────────────────────────────────────────────────
    bool is_initialized;
    
    // ─────────────────────────────────────────────────────────────────────
    // HARDWARE CONFIGURATION (set once at init)
    // ─────────────────────────────────────────────────────────────────────
    int32 samples_per_second;     // e.g., 48000 Hz
    int32 bytes_per_sample;       // e.g., 4 (16-bit stereo = 2 * 2)
    int32 secondary_buffer_size;  // Total buffer size in bytes
    
    // ─────────────────────────────────────────────────────────────────────
    // TIMING STATE (updated each frame)
    // ─────────────────────────────────────────────────────────────────────
    int64 running_sample_index;   // Total samples written (ever)
    
    // ─────────────────────────────────────────────────────────────────────
    // LATENCY CONFIGURATION
    // ─────────────────────────────────────────────────────────────────────
    int32 latency_sample_count;   // Buffer size in samples
    int32 safety_sample_count;    // Safety margin (1/3 frame)
    
    // ─────────────────────────────────────────────────────────────────────
    // GAME TIMING
    // ─────────────────────────────────────────────────────────────────────
    int32 game_update_hz;         // Game logic rate (e.g., 30 Hz)
    
} PlatformAudioConfig;

#endif // DE100_PLATFORM_COMMON_AUDIO_H
