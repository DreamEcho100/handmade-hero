#include "../../_common/hooks/audio.h"
#include "../audio.h"

f32 de100_audio_get_latency_ms(void) {
  if (!g_linux_audio_output.pcm_handle) return 0.0f;
  snd_pcm_sframes_t delay = 0;
  SndPcmDelay(g_linux_audio_output.pcm_handle, &delay);
  // Note: g_linux_audio_output doesn't store samples_per_second directly,
  // but we can get it from the latency calculation
  // delay is in frames, convert to ms assuming 48kHz
  return (f32)delay / 48000.0f * 1000.0f;
}

i32 de100_audio_get_underrun_count(void) {
  // X11/ALSA doesn't track underruns with an atomic counter like Raylib.
  // Return 0 for now -- ALSA underruns are handled via snd_pcm_recover.
  return 0;
}

void de100_audio_set_master_volume(f32 volume) {
  linux_set_master_volume(volume);
}

void de100_audio_pause(void) {
  // Note: linux_pause_audio requires LinuxAudioConfig pointer.
  // The hook can't access it directly. For now, this is a no-op.
  // Focus-based pause is handled in backend.c which has access to the config.
  (void)0;
}

void de100_audio_resume(void) {
  (void)0;
}
