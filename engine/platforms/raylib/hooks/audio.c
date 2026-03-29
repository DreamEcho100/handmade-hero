#include "../../_common/hooks/audio.h"
#include "../audio.h"
#include <stdatomic.h>

f32 de100_audio_get_latency_ms(void) {
  u32 bpf = g_raylib_audio_output.bytes_per_frame;
  if (bpf == 0) return 0.0f;
  u32 capacity = g_raylib_audio_output.ring_buffer_size_bytes;
  u32 wp = atomic_load(&g_raylib_audio_output.ring_write_pos);
  u32 rp = atomic_load(&g_raylib_audio_output.ring_read_pos);
  u32 buffered = ((wp - rp + capacity) % capacity) / bpf;
  return (f32)buffered / (f32)g_raylib_audio_output.samples_per_second * 1000.0f;
}

i32 de100_audio_get_underrun_count(void) {
  return atomic_load(&g_raylib_audio_output.callback_underruns);
}

void de100_audio_set_master_volume(f32 volume) {
  raylib_set_master_volume(volume);
}

void de100_audio_pause(void) {
  raylib_pause_audio();
}

void de100_audio_resume(void) {
  raylib_resume_audio();
}
