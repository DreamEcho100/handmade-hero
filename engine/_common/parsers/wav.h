#ifndef DE100_COMMON_PARSERS_WAV_H
#define DE100_COMMON_PARSERS_WAV_H

#include "../base.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// WAV/RIFF PARSER
//
// Pure byte-parsing utility. No file I/O, no audio knowledge, no game
// knowledge. Caller provides raw file bytes, parser returns PCM data
// pointers.
//
// Supports: PCM format (format tag 1), 8/16/24/32-bit, 1-N channels,
//           any sample rate.
//
// Usage:
//   De100DebugDe100FileReadResult file = de100_debug_read_entire_file("sound.wav");
//   De100WavResult wav = de100_wav_parse(file.memory.base, file.size);
//   if (wav.is_valid) {
//     // wav.samples points into the original file data (no copy)
//     // wav.sample_count, wav.channel_count, wav.sample_rate are set
//   }
//
// Casey's equivalent: handmade_wav.cpp + handmade_riff.cpp
// ═══════════════════════════════════════════════════════════════════════════

// RIFF chunk IDs (little-endian ASCII)
#define DE100_RIFF_ID(a, b, c, d)                                              \
  (((u32)(a)) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

#define DE100_WAV_RIFF DE100_RIFF_ID('R', 'I', 'F', 'F')
#define DE100_WAV_WAVE DE100_RIFF_ID('W', 'A', 'V', 'E')
#define DE100_WAV_FMT  DE100_RIFF_ID('f', 'm', 't', ' ')
#define DE100_WAV_DATA DE100_RIFF_ID('d', 'a', 't', 'a')

// WAV format tags
#define DE100_WAV_FORMAT_PCM 1

// ─────────────────────────────────────────────────────────────────────────
// Internal RIFF structures (packed, match file layout)
// ─────────────────────────────────────────────────────────────────────────

#pragma pack(push, 1)

typedef struct {
  u32 riff_id;       // 'RIFF'
  u32 file_size;     // File size minus 8
  u32 wave_id;       // 'WAVE'
} De100WavHeader;

typedef struct {
  u32 chunk_id;
  u32 chunk_size;
} De100RiffChunkHeader;

typedef struct {
  u16 format_tag;       // 1 = PCM
  u16 channel_count;
  u32 sample_rate;
  u32 avg_bytes_per_sec;
  u16 block_align;
  u16 bits_per_sample;
} De100WavFmtChunk;

#pragma pack(pop)

// ─────────────────────────────────────────────────────────────────────────
// Result
// ─────────────────────────────────────────────────────────────────────────

typedef struct {
  void *samples;         // Points into original file data (no allocation)
  u32 sample_count;      // Total frames (not individual channel samples)
  u32 channel_count;     // 1 = mono, 2 = stereo
  u32 sample_rate;       // Hz (e.g., 48000)
  u16 bits_per_sample;   // 8, 16, 24, or 32
  u32 data_size_bytes;   // Raw PCM data size
  bool is_valid;
  const char *error;     // NULL if valid, error message if not
} De100WavResult;

// ─────────────────────────────────────────────────────────────────────────
// Parser
// ─────────────────────────────────────────────────────────────────────────

static inline De100WavResult de100_wav_parse(void *file_data,
                                             size_t file_size) {
  De100WavResult result = {0};

  if (!file_data || file_size < sizeof(De100WavHeader)) {
    result.error = "File too small or NULL";
    return result;
  }

  // ─── Validate RIFF/WAVE header ───
  De100WavHeader *header = (De100WavHeader *)file_data;

  if (header->riff_id != DE100_WAV_RIFF) {
    result.error = "Not a RIFF file";
    return result;
  }

  if (header->wave_id != DE100_WAV_WAVE) {
    result.error = "Not a WAVE file";
    return result;
  }

  // ─── Iterate chunks to find fmt and data ───
  u8 *cursor = (u8 *)file_data + sizeof(De100WavHeader);
  u8 *end = (u8 *)file_data + file_size;

  De100WavFmtChunk *fmt = NULL;
  void *data_ptr = NULL;
  u32 data_size = 0;

  while (cursor + sizeof(De100RiffChunkHeader) <= end) {
    De100RiffChunkHeader *chunk = (De100RiffChunkHeader *)cursor;
    u8 *chunk_data = cursor + sizeof(De100RiffChunkHeader);

    // Bounds check: chunk data must fit within file
    if (chunk_data + chunk->chunk_size > end) {
      break;
    }

    if (chunk->chunk_id == DE100_WAV_FMT) {
      if (chunk->chunk_size >= sizeof(De100WavFmtChunk)) {
        fmt = (De100WavFmtChunk *)chunk_data;
      }
    } else if (chunk->chunk_id == DE100_WAV_DATA) {
      data_ptr = chunk_data;
      data_size = chunk->chunk_size;
    }

    // Advance to next chunk (chunks are 2-byte aligned)
    u32 advance = sizeof(De100RiffChunkHeader) + chunk->chunk_size;
    if (advance & 1)
      advance++; // Pad to even boundary
    cursor += advance;
  }

  // ─── Validate we found both chunks ───
  if (!fmt) {
    result.error = "No fmt chunk found";
    return result;
  }

  if (!data_ptr) {
    result.error = "No data chunk found";
    return result;
  }

  if (fmt->format_tag != DE100_WAV_FORMAT_PCM) {
    result.error = "Not PCM format (only PCM supported)";
    return result;
  }

  if (fmt->channel_count == 0 || fmt->sample_rate == 0 ||
      fmt->bits_per_sample == 0) {
    result.error = "Invalid format parameters";
    return result;
  }

  // ─── Calculate sample count ───
  u32 bytes_per_frame =
      (u32)fmt->channel_count * ((u32)fmt->bits_per_sample / 8);
  if (bytes_per_frame == 0) {
    result.error = "Invalid bytes per frame";
    return result;
  }

  u32 sample_count = data_size / bytes_per_frame;

  // ─── Fill result ───
  result.samples = data_ptr;
  result.sample_count = sample_count;
  result.channel_count = fmt->channel_count;
  result.sample_rate = fmt->sample_rate;
  result.bits_per_sample = fmt->bits_per_sample;
  result.data_size_bytes = data_size;
  result.is_valid = true;
  result.error = NULL;

  return result;
}

#endif // DE100_COMMON_PARSERS_WAV_H
