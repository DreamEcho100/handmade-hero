#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include "platform.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* GLX extension for VSync */
typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform State
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  Display *display;
  Window window;
  GLXContext gl_context;
  GLuint texture_id;
  int screen;
  int width;
  int height;
  bool vsync_enabled;

  /* Audio (ALSA) */
  snd_pcm_t *pcm_handle;
  int16_t *sample_buffer;
  int sample_buffer_size; /* Max samples we can generate */
  int samples_per_second;
  snd_pcm_uframes_t hw_buffer_size; /* Hardware buffer size in frames */
} X11State;

static X11State g_x11 = {0};
static double g_start_time = 0.0;
static double g_last_frame_time = 0.0;
static const double g_target_frame_time = 1.0 / 60.0;

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Initialization (Casey's Latency Model)
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_audio_init(PlatformAudioConfig *config) {
  int err;

  config->samples_per_second = 48000;

  /* Casey's latency calculations using config values */
  config->samples_per_frame = config->samples_per_second / config->hz;
  config->latency_samples =
      config->samples_per_frame * config->frames_of_latency;
  config->safety_samples = config->samples_per_frame / 3;
  config->running_sample_index = 0;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 ALSA AUDIO (Casey's Latency Model)\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:     %d Hz\n", config->samples_per_second);
  printf("  Game update:     %d Hz\n", config->hz);
  printf("  Samples/frame:   %d (%.1f ms)\n", config->samples_per_frame,
         (float)config->samples_per_frame / config->samples_per_second *
             1000.0f);
  printf("  Target latency:  %d samples (%.1f ms, %d frames)\n",
         config->latency_samples,
         (float)config->latency_samples / config->samples_per_second * 1000.0f,
         config->frames_of_latency);
  printf("  Safety margin:   %d samples (%.1f ms)\n", config->safety_samples,
         (float)config->safety_samples / config->samples_per_second * 1000.0f);

  /* Open default PCM device */
  err = snd_pcm_open(&g_x11.pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    fprintf(stderr, "ALSA: Cannot open audio device: %s\n", snd_strerror(err));
    config->is_initialized = 0;
    return 1;
  }

  /* Set hardware parameters */
  snd_pcm_hw_params_t *hw_params;
  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_hw_params_any(g_x11.pcm_handle, hw_params);

  snd_pcm_hw_params_set_access(g_x11.pcm_handle, hw_params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(g_x11.pcm_handle, hw_params,
                               SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(g_x11.pcm_handle, hw_params, 2);

  unsigned int sample_rate = config->samples_per_second;
  snd_pcm_hw_params_set_rate_near(g_x11.pcm_handle, hw_params, &sample_rate, 0);
  config->samples_per_second = sample_rate;

  /* Buffer size: enough for latency + safety + extra headroom */
  snd_pcm_uframes_t buffer_frames =
      config->latency_samples + config->safety_samples;
  buffer_frames *= 2; /* Double for headroom */
  snd_pcm_hw_params_set_buffer_size_near(g_x11.pcm_handle, hw_params,
                                         &buffer_frames);
  g_x11.hw_buffer_size = buffer_frames;

  /* Period size: ~1/4 of buffer */
  snd_pcm_uframes_t period_frames = buffer_frames / 4;
  snd_pcm_hw_params_set_period_size_near(g_x11.pcm_handle, hw_params,
                                         &period_frames, 0);

  err = snd_pcm_hw_params(g_x11.pcm_handle, hw_params);
  if (err < 0) {
    fprintf(stderr, "ALSA: Cannot set hardware parameters: %s\n",
            snd_strerror(err));
    snd_pcm_close(g_x11.pcm_handle);
    config->is_initialized = 0;
    return 1;
  }

  /* Query actual values */
  snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_frames);
  snd_pcm_hw_params_get_period_size(hw_params, &period_frames, 0);

  config->buffer_size_samples = buffer_frames;
  g_x11.hw_buffer_size = buffer_frames;

  printf("  HW buffer:       %lu frames (%.1f ms)\n", buffer_frames,
         (float)buffer_frames / config->samples_per_second * 1000.0f);
  printf("  HW period:       %lu frames (%.1f ms)\n", period_frames,
         (float)period_frames / config->samples_per_second * 1000.0f);

  /* Allocate sample buffer (enough for several frames) */
  g_x11.sample_buffer_size = config->samples_per_frame * 4;
  g_x11.samples_per_second = config->samples_per_second;
  g_x11.sample_buffer =
      (int16_t *)malloc(g_x11.sample_buffer_size * 2 * sizeof(int16_t));

  if (!g_x11.sample_buffer) {
    fprintf(stderr, "ALSA: Cannot allocate sample buffer\n");
    snd_pcm_close(g_x11.pcm_handle);
    config->is_initialized = 0;
    return 1;
  }

  /* Pre-fill buffer with silence to start playback */
  memset(g_x11.sample_buffer, 0, config->latency_samples * 2 * sizeof(int16_t));
  snd_pcm_writei(g_x11.pcm_handle, g_x11.sample_buffer,
                 config->latency_samples);

  snd_pcm_prepare(g_x11.pcm_handle);

  config->is_initialized = 1;
  printf("═══════════════════════════════════════════════════════════\n\n");

  return 0;
}

void platform_audio_shutdown(void) {
  if (g_x11.sample_buffer) {
    free(g_x11.sample_buffer);
    g_x11.sample_buffer = NULL;
  }
  if (g_x11.pcm_handle) {
    snd_pcm_drain(g_x11.pcm_handle);
    snd_pcm_close(g_x11.pcm_handle);
    g_x11.pcm_handle = NULL;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Casey's Audio Latency Model
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int platform_audio_get_samples_to_write(PlatformAudioConfig *config) {
  if (!config->is_initialized || !g_x11.pcm_handle)
    return 0;

  /* Get delay: how many frames are queued in ALSA buffer */
  snd_pcm_sframes_t delay_frames = 0;
  int err = snd_pcm_delay(g_x11.pcm_handle, &delay_frames);

  if (err < 0) {
    if (err == -EPIPE) {
      fprintf(stderr, "ALSA: Underrun! Recovering...\n");
      snd_pcm_prepare(g_x11.pcm_handle);
      delay_frames = 0;
    } else {
      return 0;
    }
  }

  /* Check available space */
  snd_pcm_sframes_t avail_frames = snd_pcm_avail_update(g_x11.pcm_handle);
  if (avail_frames < 0) {
    if (avail_frames == -EPIPE) {
      snd_pcm_prepare(g_x11.pcm_handle);
      avail_frames = g_x11.hw_buffer_size;
      delay_frames = 0;
    } else {
      return 0;
    }
  }

  /* Casey's calculation */
  int target_buffered = config->latency_samples + config->safety_samples;
  int samples_to_write = target_buffered - (int)delay_frames;

  /* Clamp to available space */
  if (samples_to_write > (int)avail_frames) {
    samples_to_write = (int)avail_frames;
  }

  /* Don't write tiny amounts */
  if (samples_to_write < config->samples_per_frame / 4) {
    samples_to_write = 0;
  }

  /* Clamp to buffer capacity */
  if (samples_to_write > g_x11.sample_buffer_size) {
    samples_to_write = g_x11.sample_buffer_size;
  }

  return samples_to_write;
}

static void platform_audio_update(GameState *game_state,
                                  PlatformAudioConfig *config) {
  if (!config->is_initialized || !g_x11.pcm_handle)
    return;

  int samples_to_write = platform_audio_get_samples_to_write(config);

  if (samples_to_write <= 0)
    return;

  AudioOutputBuffer buffer = {.samples_buffer = g_x11.sample_buffer,
                              .samples_per_second = g_x11.samples_per_second,
                              .sample_count = samples_to_write};

  game_get_audio_samples(game_state, &buffer);

  snd_pcm_sframes_t frames_written =
      snd_pcm_writei(g_x11.pcm_handle, g_x11.sample_buffer, samples_to_write);

  if (frames_written < 0) {
    frames_written = snd_pcm_recover(g_x11.pcm_handle, frames_written, 0);
    if (frames_written < 0) {
      fprintf(stderr, "ALSA: Write error: %s\n", snd_strerror(frames_written));
      return;
    }
  }

  config->running_sample_index += frames_written;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing
 * ═══════════════════════════════════════════════════════════════════════════
 */

static double get_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double now = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
  if (g_start_time == 0.0)
    g_start_time = now;
  return now - g_start_time;
}

static void sleep_ms(int ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VSync Setup
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void setup_vsync(void) {
  const char *extensions =
      glXQueryExtensionsString(g_x11.display, g_x11.screen);

  if (extensions && strstr(extensions, "GLX_EXT_swap_control")) {
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT =
        (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalEXT");
    if (glXSwapIntervalEXT) {
      glXSwapIntervalEXT(g_x11.display, g_x11.window, 1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (GLX_EXT_swap_control)\n");
      return;
    }
  }

  if (extensions && strstr(extensions, "GLX_MESA_swap_control")) {
    PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA =
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalMESA");
    if (glXSwapIntervalMESA) {
      glXSwapIntervalMESA(1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (GLX_MESA_swap_control)\n");
      return;
    }
  }

  g_x11.vsync_enabled = false;
  printf("⚠ VSync not available\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Init
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_init(PlatformGameProps *platform_game_props) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "Failed to open display\n");
    return 1;
  }

  Bool supported;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);
  printf("✓ Auto-repeat detection: %s\n",
         supported ? "enabled" : "not supported");

  g_x11.screen = DefaultScreen(g_x11.display);
  g_x11.width = platform_game_props->width;
  g_x11.height = platform_game_props->height;

  int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual =
      glXChooseVisual(g_x11.display, g_x11.screen, visual_attribs);
  if (!visual) {
    fprintf(stderr, "Failed to choose GLX visual\n");
    return 1;
  }

  Colormap colormap =
      XCreateColormap(g_x11.display, RootWindow(g_x11.display, g_x11.screen),
                      visual->visual, AllocNone);

  XSetWindowAttributes attrs;
  attrs.colormap = colormap;
  attrs.event_mask =
      ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask;

  g_x11.window = XCreateWindow(
      g_x11.display, RootWindow(g_x11.display, g_x11.screen), 100, 100,
      platform_game_props->width, platform_game_props->height, 0, visual->depth,
      InputOutput, visual->visual, CWColormap | CWEventMask, &attrs);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  if (!g_x11.gl_context) {
    fprintf(stderr, "Failed to create GLX context\n");
    XFree(visual);
    return 1;
  }

  XFree(visual);
  XStoreName(g_x11.display, g_x11.window, platform_game_props->title);
  XMapWindow(g_x11.display, g_x11.window);
  XFlush(g_x11.display);

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  glViewport(0, 0, g_x11.width, g_x11.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, platform_game_props->width, platform_game_props->height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  setup_vsync();

  printf("✓ OpenGL initialized: %s\n", glGetString(GL_VERSION));

  if (platform_audio_init(&platform_game_props->audio) != 0) {
    fprintf(stderr,
            "Warning: Audio initialization failed, continuing without audio\n");
  }
  return 0;
}

void platform_shutdown(void) {
  platform_audio_shutdown();
  if (g_x11.texture_id)
    glDeleteTextures(1, &g_x11.texture_id);
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)
    XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display)
    XCloseDisplay(g_x11.display);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input Handling
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void platform_get_input(GameState *game_state, GameInput *input,
                               PlatformGameProps *platform_game_props) {
  XEvent event;

  while (XPending(g_x11.display) > 0) {
    XNextEvent(g_x11.display, &event);

    switch (event.type) {
    case KeyPress: {
      KeySym key = XLookupKeysym(&event.xkey, 0);
      switch (key) {
      case XK_q:
      case XK_Q:
      case XK_Escape:
        platform_game_props->is_running = false;
        game_state->should_quit = 1;
        break;
      case XK_r:
      case XK_R:
        game_state->should_restart = 1;
        break;
      case XK_x:
      case XK_X:
        UPDATE_BUTTON(input->rotate_x, 1);
        game_state->current_piece.next_rotate_x_value =
            TETROMINO_ROTATE_X_GO_RIGHT;
        break;
      case XK_z:
      case XK_Z:
        UPDATE_BUTTON(input->rotate_x, 1);
        game_state->current_piece.next_rotate_x_value =
            TETROMINO_ROTATE_X_GO_LEFT;
        break;
      case XK_Left:
      case XK_A:
      case XK_a:
        UPDATE_BUTTON(input->move_left, 1);
        break;
      case XK_Right:
      case XK_D:
      case XK_d:
        UPDATE_BUTTON(input->move_right, 1);
        break;
      case XK_Down:
      case XK_S:
      case XK_s:
        UPDATE_BUTTON(input->move_down, 1);
        break;
      }
      break;
    }

    case KeyRelease: {
      KeySym key = XLookupKeysym(&event.xkey, 0);
      switch (key) {
      case XK_r:
      case XK_R:
        game_state->should_restart = 0;
        break;
      case XK_x:
      case XK_X:
      case XK_z:
      case XK_Z:
        UPDATE_BUTTON(input->rotate_x, 0);
        game_state->current_piece.next_rotate_x_value = TETROMINO_ROTATE_X_NONE;
        break;
      case XK_Left:
      case XK_A:
      case XK_a:
        UPDATE_BUTTON(input->move_left, 0);
        break;
      case XK_Right:
      case XK_D:
      case XK_d:
        UPDATE_BUTTON(input->move_right, 0);
        break;
      case XK_Down:
      case XK_S:
      case XK_s:
        UPDATE_BUTTON(input->move_down, 0);
        break;
      }
      break;
    }

    case ClientMessage:
      platform_game_props->is_running = false;
      game_state->should_quit = 1;
      break;

    case ConfigureNotify:
      if (event.xconfigure.width != g_x11.width ||
          event.xconfigure.height != g_x11.height) {
        g_x11.width = event.xconfigure.width;
        g_x11.height = event.xconfigure.height;
        glViewport(0, 0, g_x11.width, g_x11.height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, g_x11.width, g_x11.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
      }
      break;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Display Backbuffer
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void platform_display_backbuffer(Backbuffer *bb) {
  // glClear(GL_COLOR_BUFFER_BIT);

  // glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bb->width, bb->height, 0, GL_RGBA,
  //              GL_UNSIGNED_BYTE, bb->pixels);

  // glBegin(GL_QUADS);
  // glTexCoord2f(0.0f, 0.0f);
  // glVertex2f(0, 0);
  // glTexCoord2f(1.0f, 0.0f);
  // glVertex2f(bb->width, 0);
  // glTexCoord2f(1.0f, 1.0f);
  // glVertex2f(bb->width, bb->height);
  // glTexCoord2f(0.0f, 1.0f);
  // glVertex2f(0, bb->height);
  // glEnd();

  // glXSwapBuffers(g_x11.display, g_x11.window);

  glClear(GL_COLOR_BUFFER_BIT);

  // Center backbuffer in window
  int offset_x = (g_x11.width - bb->width) / 2;
  int offset_y = (g_x11.height - bb->height) / 2;
  if (offset_x < 0)
    offset_x = 0;
  if (offset_y < 0)
    offset_y = 0;
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bb->width, bb->height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, bb->pixels);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(offset_x, offset_y);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(offset_x + bb->width, offset_y);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(offset_x + bb->width, offset_y + bb->height);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(offset_x, offset_y + bb->height);
  glEnd();
  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  PlatformGameProps platform_game_props = {0};
  if (platform_game_props_init(&platform_game_props) != 0) {
    return 1;
  }

  if (platform_init(&platform_game_props) != 0) {
    return 1;
  }

  GameInput inputs[2] = {0};
  GameInput *current_input = &inputs[0];
  GameInput *old_input = &inputs[1];

  GameState game_state = {0};
  game_init(&game_state);

  if (platform_game_props.audio.is_initialized) {
    game_audio_init(&game_state.audio,
                    platform_game_props.audio.samples_per_second);
    game_music_play(&game_state.audio);
  }

  g_last_frame_time = get_time();

  while (platform_game_props.is_running) {
    double current_time = get_time();
    float delta_time = (float)(current_time - g_last_frame_time);
    g_last_frame_time = current_time;

    prepare_input_frame(old_input, current_input);
    platform_get_input(&game_state, current_input, &platform_game_props);

    if (current_input->move_left.ended_down) {
      printf("Move Left: %d\n", current_input->move_left.half_transition_count);
    }

    if (game_state.should_quit) {
      break;
    }

    game_update(&game_state, current_input, delta_time);
    platform_audio_update(&game_state, &platform_game_props.audio);
    game_render(&platform_game_props.backbuffer, &game_state);
    platform_display_backbuffer(&platform_game_props.backbuffer);

    if (!g_x11.vsync_enabled) {
      double frame_time = get_time() - current_time;
      if (frame_time < g_target_frame_time) {
        sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
      }
    }

    platform_swap_input_buffers(old_input, current_input);
  }

  platform_game_props_free(&platform_game_props);
  platform_shutdown();
  return 0;
}
