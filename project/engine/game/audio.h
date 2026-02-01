#ifndef DE100_GAME_AUDIO_H
#define DE100_GAME_AUDIO_H

#include "../_common/base.h"
#include "../_common/memory.h"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// SOUND SOURCE (Individual audio generator)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    real32 phase;            // Phase accumulator (0 to 2π) - PERSISTS between calls!
    real32 frequency;        // Current frequency (Hz)
    real32 target_frequency; // For smooth frequency transitions (future)
    real32 volume;           // 0.0 to 1.0
    real32 pan_position;     // -1.0 (left) to 1.0 (right)
    bool is_playing;
} SoundSource;

// ═══════════════════════════════════════════════════════════════════════════
// GAME AUDIO STATE (Lives in game memory)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    SoundSource tone;       // Simple oscillator
    real32 master_volume;   // Global volume multiplier
    
    // Future expansion:
    // SoundSource music;
    // SoundSource sfx[8];
} GameAudioState;

// ═══════════════════════════════════════════════════════════════════════════
// SOUND OUTPUT BUFFER (Platform → Game → Platform)
// ═══════════════════════════════════════════════════════════════════════════
// Platform allocates memory, tells game how many samples to generate,
// game fills buffer, platform sends to hardware.
//
// Casey's equivalent: game_audio_output_buffer
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    int32 samples_per_second;  // Sample rate (e.g., 48000)
    int32 sample_count;        // How many samples to generate THIS call
    
    // Two ways to access the buffer (platform chooses which to use):
    // int16 *samples;            // Simple pointer (preferred for game code)
    void *samples;            // Simple pointer (preferred for game code)
} GameAudioOutputBuffer;

#endif // DE100_GAME_AUDIO_H
