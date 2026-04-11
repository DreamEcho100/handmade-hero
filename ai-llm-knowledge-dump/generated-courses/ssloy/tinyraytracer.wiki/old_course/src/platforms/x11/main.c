#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/main.c — TinyRaytracer Course
 * (Adapted from Platform Foundation Course — audio stripped, raytracer keys)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "./base.h"
#include "../../platform.h"
#include "../../game/base.h"
#include "../../game/main.h"
#include "../../game/render.h"
#include "../../game/gpu_shader.h"
#include "../../game/gpu_upload.h"
#include "../../game/settings.h"
#include "../../utils/math.h"

typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

/* ═══════════════════════════════════════════════════════════════════════════
 * L23 GPU rendering — OpenGL 2.0+ shader function pointers
 * ═══════════════════════════════════════════════════════════════════════════
 * The X11 backend gets a legacy compat context via glXCreateContext.
 * Most Mesa/NVIDIA drivers still expose GL 2.0+ shader functions in compat
 * mode.  We load them at runtime via glXGetProcAddressARB.
 *
 * EDUCATIONAL NOTES:
 * • glCreateShader / glShaderSource / glCompileShader → compile GLSL text
 * • glCreateProgram / glAttachShader / glLinkProgram  → link into pipeline
 * • glUseProgram   → bind the program for subsequent draw calls
 * • glUniform*     → set per-frame data (resolution, time, mouse)
 * • glGenVertexArrays / glBindVertexArray → empty VAO for gl_VertexID trick
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef GLuint (*PFN_glCreateShader)(GLenum);
typedef void   (*PFN_glShaderSource)(GLuint, GLsizei, const char **, const GLint *);
typedef void   (*PFN_glCompileShader)(GLuint);
typedef void   (*PFN_glGetShaderiv)(GLuint, GLenum, GLint *);
typedef void   (*PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei *, char *);
typedef GLuint (*PFN_glCreateProgram)(void);
typedef void   (*PFN_glAttachShader)(GLuint, GLuint);
typedef void   (*PFN_glLinkProgram)(GLuint);
typedef void   (*PFN_glGetProgramiv)(GLuint, GLenum, GLint *);
typedef void   (*PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei *, char *);
typedef void   (*PFN_glUseProgram)(GLuint);
typedef GLint  (*PFN_glGetUniformLocation)(GLuint, const char *);
typedef void   (*PFN_glUniform1f)(GLint, GLfloat);
typedef void   (*PFN_glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (*PFN_glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (*PFN_glDeleteShader)(GLuint);
typedef void   (*PFN_glDeleteProgram)(GLuint);
typedef void   (*PFN_glGenVertexArrays)(GLsizei, GLuint *);
typedef void   (*PFN_glBindVertexArray)(GLuint);
typedef void   (*PFN_glDeleteVertexArrays)(GLsizei, const GLuint *);
/* L24+ scene upload function pointers */
typedef void   (*PFN_glUniform1i)(GLint, GLint);
typedef void   (*PFN_glUniform1iv)(GLint, GLsizei, const GLint *);
typedef void   (*PFN_glUniform1fv)(GLint, GLsizei, const GLfloat *);
typedef void   (*PFN_glUniform3fv)(GLint, GLsizei, const GLfloat *);
typedef void   (*PFN_glUniform4fv)(GLint, GLsizei, const GLfloat *);
typedef void   (*PFN_glUniform1uiv)(GLint, GLsizei, const GLuint *);
typedef void   (*PFN_glActiveTexture)(GLenum);

typedef struct {
  int ready;              /* 1 if shader compiled + linked OK */
  GLuint program;
  GLuint vao;             /* empty VAO for gl_VertexID fullscreen tri */
  /* Uniform locations */
  GLint u_resolution;
  GLint u_time;
  GLint u_mouse;
  GLint u_cam_origin;
  GLint u_cam_forward;
  GLint u_cam_right;
  GLint u_cam_up;
  GLint u_cam_fov;
  /* Function pointers */
  PFN_glCreateShader       CreateShader;
  PFN_glShaderSource       ShaderSource;
  PFN_glCompileShader      CompileShader;
  PFN_glGetShaderiv        GetShaderiv;
  PFN_glGetShaderInfoLog   GetShaderInfoLog;
  PFN_glCreateProgram      CreateProgram;
  PFN_glAttachShader       AttachShader;
  PFN_glLinkProgram        LinkProgram;
  PFN_glGetProgramiv       GetProgramiv;
  PFN_glGetProgramInfoLog  GetProgramInfoLog;
  PFN_glUseProgram         UseProgram;
  PFN_glGetUniformLocation GetUniformLocation;
  PFN_glUniform1f          Uniform1f;
  PFN_glUniform3f          Uniform3f;
  PFN_glUniform4f          Uniform4f;
  PFN_glDeleteShader       DeleteShader;
  PFN_glDeleteProgram      DeleteProgram;
  PFN_glGenVertexArrays    GenVertexArrays;
  PFN_glBindVertexArray    BindVertexArray;
  PFN_glDeleteVertexArrays DeleteVertexArrays;
  /* L24+ scene upload */
  PFN_glUniform1i         Uniform1i;
  PFN_glUniform1iv        Uniform1iv;
  PFN_glUniform1fv        Uniform1fv;
  PFN_glUniform3fv        Uniform3fv;
  PFN_glUniform4fv        Uniform4fv;
  PFN_glUniform1uiv       Uniform1uiv;
  PFN_glActiveTexture     ActiveTexture;
  /* Scene uniform locations */
  GLint u_mat_count, u_mat_color, u_mat_albedo, u_mat_spec, u_mat_ior;
  GLint u_sphere_count, u_sphere, u_sphere_mat;
  GLint u_light_count, u_light;
  GLint u_box_count, u_box_center, u_box_half, u_box_mat;
  GLint u_vox_count, u_vox_pos, u_vox_scale, u_vox_mat;
  GLint u_vox_bbox_center, u_vox_bbox_half, u_vox_solid_count, u_vox_bitfield;
  GLint u_mesh_count, u_mesh_pos, u_mesh_bbox_center, u_mesh_bbox_half;
  GLint u_mesh_mat, u_mesh_tri_count, u_mesh_data_off, u_mesh_tri_tex;
  GLint u_env_mode, u_env_equirect, u_env_cube;
  GLint u_features;
  /* Texture objects */
  GLuint tex_mesh_tri;
  GLuint tex_env_equirect;
  GLuint tex_env_cube;
  int    scene_uploaded;
} GpuState;

static GpuState g_gpu = {0};

/* ═══════════════════════════════════════════════════════════════════════════
 * Frame timing
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  struct timespec frame_start;
  struct timespec work_end;
  struct timespec frame_end;
  float work_seconds;
  float total_seconds;
  float sleep_seconds;
} FrameTiming;

#ifdef DEBUG
typedef struct {
  unsigned int frame_count;
  unsigned int missed_frames;
  float min_frame_ms;
  float max_frame_ms;
  float total_frame_ms;
} FrameStats;
static FrameStats g_stats = {0};
#endif

static FrameTiming g_timing = {0};
static const float TARGET_SECONDS = 1.0f / TARGET_FPS;

static inline double timespec_to_seconds(const struct timespec *ts) {
  return (double)ts->tv_sec + (double)ts->tv_nsec * 1e-9;
}

static inline float timespec_diff_seconds(const struct timespec *a,
                                           const struct timespec *b) {
  return (float)(timespec_to_seconds(b) - timespec_to_seconds(a));
}

static inline void get_timespec(struct timespec *ts) {
  clock_gettime(CLOCK_MONOTONIC, ts);
}

static inline void timing_begin(void) {
  get_timespec(&g_timing.frame_start);
}

static inline void timing_mark_work_done(void) {
  get_timespec(&g_timing.work_end);
  g_timing.work_seconds = timespec_diff_seconds(&g_timing.frame_start,
                                                  &g_timing.work_end);
}

static void timing_sleep_until_target(void) {
  float elapsed = g_timing.work_seconds;
  if (elapsed < TARGET_SECONDS) {
    float threshold = TARGET_SECONDS - 0.003f;
    while (elapsed < threshold) {
      struct timespec sleep_ts = { .tv_sec = 0, .tv_nsec = 1000000L };
      nanosleep(&sleep_ts, NULL);
      struct timespec now;
      get_timespec(&now);
      elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }
    while (elapsed < TARGET_SECONDS) {
      struct timespec now;
      get_timespec(&now);
      elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }
  }
}

static inline void timing_end(void) {
  get_timespec(&g_timing.frame_end);
  g_timing.total_seconds = timespec_diff_seconds(&g_timing.frame_start,
                                                   &g_timing.frame_end);
  g_timing.sleep_seconds = g_timing.total_seconds - g_timing.work_seconds;
#ifdef DEBUG
  float ms = g_timing.total_seconds * 1000.0f;
  g_stats.frame_count++;
  if (g_stats.frame_count == 1 || ms < g_stats.min_frame_ms)
    g_stats.min_frame_ms = ms;
  if (ms > g_stats.max_frame_ms)
    g_stats.max_frame_ms = ms;
  g_stats.total_frame_ms += ms;
  if (g_timing.total_seconds > TARGET_SECONDS + 0.002f)
    g_stats.missed_frames++;
#endif
}

#ifdef DEBUG
static void print_frame_stats(void) {
  if (g_stats.frame_count == 0) return;
  printf("\nFrame Stats: %u frames, %u missed, %.2f-%.2f ms (avg %.2f)\n",
         g_stats.frame_count, g_stats.missed_frames,
         g_stats.min_frame_ms, g_stats.max_frame_ms,
         g_stats.total_frame_ms / (float)g_stats.frame_count);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * VSync
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void setup_vsync(void) {
  const char *ext = glXQueryExtensionsString(g_x11.display, g_x11.screen);
  if (ext && strstr(ext, "GLX_EXT_swap_control")) {
    PFNGLXSWAPINTERVALEXTPROC fn =
        (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalEXT");
    if (fn) {
      fn(g_x11.display, g_x11.window, 1);
      g_x11.vsync_enabled = true;
      return;
    }
  }
  if (ext && strstr(ext, "GLX_MESA_swap_control")) {
    PFNGLXSWAPINTERVALMESAPROC fn =
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalMESA");
    if (fn) { fn(1); g_x11.vsync_enabled = true; return; }
  }
  g_x11.vsync_enabled = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Window + GL init
 * ═══════════════════════════════════════════════════════════════════════════
 */
static int init_window(void) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) { fprintf(stderr, "XOpenDisplay failed\n"); return -1; }

  Bool ok;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &ok);
  g_x11.screen   = DefaultScreen(g_x11.display);
  g_x11.window_w = GAME_W;
  g_x11.window_h = GAME_H;

  int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) { fprintf(stderr, "glXChooseVisual failed\n"); return -1; }

  Colormap cmap = XCreateColormap(g_x11.display,
                                   RootWindow(g_x11.display, g_x11.screen),
                                   visual->visual, AllocNone);
  XSetWindowAttributes wa = {
    .colormap   = cmap,
    .event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                  ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                  StructureNotifyMask,
  };

  g_x11.window = XCreateWindow(
      g_x11.display, RootWindow(g_x11.display, g_x11.screen),
      100, 100, GAME_W, GAME_H, 0,
      visual->depth, InputOutput, visual->visual,
      CWColormap | CWEventMask, &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);
  if (!g_x11.gl_context) { fprintf(stderr, "glXCreateContext failed\n"); return -1; }

  XStoreName(g_x11.display, g_x11.window, TITLE);
  XMapWindow(g_x11.display, g_x11.window);

  g_x11.wm_delete_window = XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  glViewport(0, 0, GAME_W, GAME_H);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  glOrtho(0, GAME_W, GAME_H, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GAME_W, GAME_H, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  return 0;
}

static void shutdown_window(void) {
  if (g_x11.texture_id) glDeleteTextures(1, &g_x11.texture_id);
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)  XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display) XCloseDisplay(g_x11.display);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input — raytracer key bindings
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void handle_key(GameInput *curr, KeySym sym, int is_down) {
  switch (sym) {
  case XK_Escape: case XK_q: case XK_Q:
    UPDATE_BUTTON(&curr->buttons.quit, is_down); break;
  /* Navigation — arrows + WASD */
  case XK_Left:  case XK_a: UPDATE_BUTTON(&curr->buttons.camera_left, is_down); break;
  case XK_Right: case XK_d: UPDATE_BUTTON(&curr->buttons.camera_right, is_down); break;
  case XK_Up:    UPDATE_BUTTON(&curr->buttons.camera_up, is_down); break;
  case XK_Down:  UPDATE_BUTTON(&curr->buttons.camera_down, is_down); break;
  case XK_w: case XK_W: UPDATE_BUTTON(&curr->buttons.camera_forward, is_down); break;
  case XK_s:             UPDATE_BUTTON(&curr->buttons.camera_backward, is_down); break;
  /* Exports (uppercase for S to avoid WASD conflict) */
  case XK_p: case XK_P: UPDATE_BUTTON(&curr->buttons.export_ppm, is_down); break;
  case XK_A:             UPDATE_BUTTON(&curr->buttons.export_anaglyph, is_down); break;
  case XK_S:             UPDATE_BUTTON(&curr->buttons.export_sidebyside, is_down); break;
  case XK_g: UPDATE_BUTTON(&curr->buttons.export_stereogram, is_down); break;
  case XK_G: UPDATE_BUTTON(&curr->buttons.export_stereogram_cross, is_down); break;
  case XK_L: UPDATE_BUTTON(&curr->buttons.export_glsl, is_down); break;
  /* Feature toggles */
  case XK_v: case XK_V: UPDATE_BUTTON(&curr->buttons.toggle_voxels, is_down); break;
  case XK_f: case XK_F: UPDATE_BUTTON(&curr->buttons.toggle_floor, is_down); break;
  case XK_b: case XK_B: UPDATE_BUTTON(&curr->buttons.toggle_boxes, is_down); break;
  case XK_m: case XK_M: UPDATE_BUTTON(&curr->buttons.toggle_meshes, is_down); break;
  case XK_r: case XK_R: UPDATE_BUTTON(&curr->buttons.toggle_reflections, is_down); break;
  case XK_t: case XK_T: UPDATE_BUTTON(&curr->buttons.toggle_refractions, is_down); break;
  case XK_h: case XK_H: UPDATE_BUTTON(&curr->buttons.toggle_shadows, is_down); break;
  case XK_e: case XK_E: UPDATE_BUTTON(&curr->buttons.toggle_envmap, is_down); break;
  case XK_x: case XK_X: UPDATE_BUTTON(&curr->buttons.toggle_aa, is_down); break;
  case XK_c: case XK_C: UPDATE_BUTTON(&curr->buttons.cycle_envmap_mode, is_down); break;
  /* Render scale */
  case XK_Tab: UPDATE_BUTTON(&curr->buttons.scale_cycle, is_down); break;
  /* Render mode (L22/L23) */
  case XK_n: case XK_N: UPDATE_BUTTON(&curr->buttons.cycle_render_mode, is_down); break;
  /* HUD toggle */
  case XK_F1: UPDATE_BUTTON(&curr->buttons.toggle_hud, is_down); break;
  }
}

static float g_prev_mouse_x = 0.0f;
static float g_prev_mouse_y = 0.0f;

static void process_events(GameInput *curr, int *is_running) {
  /* Reset mouse delta per frame (accumulated from events below). */
  curr->mouse.dx     = 0.0f;
  curr->mouse.dy     = 0.0f;
  curr->mouse.scroll = 0.0f;

  while (XPending(g_x11.display) > 0) {
    XEvent ev;
    XNextEvent(g_x11.display, &ev);
    switch (ev.type) {
    case KeyPress: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      handle_key(curr, sym, 1);
      if (sym == XK_Escape || sym == XK_q || sym == XK_Q)
        *is_running = 0;
      break;
    }
    case KeyRelease: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      handle_key(curr, sym, 0);
      break;
    }
    /* ── Mouse buttons ────────────────────────────────────────────────── */
    case ButtonPress:
      if (ev.xbutton.button == 1) curr->mouse.left_down   = 1;
      if (ev.xbutton.button == 2) curr->mouse.middle_down = 1;
      if (ev.xbutton.button == 3) curr->mouse.right_down  = 1;
      if (ev.xbutton.button == 4) curr->mouse.scroll += 1.0f; /* wheel up */
      if (ev.xbutton.button == 5) curr->mouse.scroll -= 1.0f; /* wheel down */
      break;
    case ButtonRelease:
      if (ev.xbutton.button == 1) curr->mouse.left_down   = 0;
      if (ev.xbutton.button == 2) curr->mouse.middle_down = 0;
      if (ev.xbutton.button == 3) curr->mouse.right_down  = 0;
      break;
    /* ── Mouse motion ─────────────────────────────────────────────────── */
    case MotionNotify:
      curr->mouse.x  = (float)ev.xmotion.x;
      curr->mouse.y  = (float)ev.xmotion.y;
      curr->mouse.dx = curr->mouse.x - g_prev_mouse_x;
      curr->mouse.dy = curr->mouse.y - g_prev_mouse_y;
      g_prev_mouse_x = curr->mouse.x;
      g_prev_mouse_y = curr->mouse.y;
      break;
    case ClientMessage:
      if ((Atom)ev.xclient.data.l[0] == g_x11.wm_delete_window) {
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
      }
      break;
    case ConfigureNotify:
      g_x11.window_w = ev.xconfigure.width;
      g_x11.window_h = ev.xconfigure.height;
      break;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Display backbuffer
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void compute_letterbox(int win_w, int win_h, int canvas_w, int canvas_h,
                              float *scale, int *off_x, int *off_y) {
  float sx = (float)win_w / (float)canvas_w;
  float sy = (float)win_h / (float)canvas_h;
  *scale = (sx < sy) ? sx : sy;
  *off_x = (int)((win_w - canvas_w * *scale) * 0.5f);
  *off_y = (int)((win_h - canvas_h * *scale) * 0.5f);
}

static void display_backbuffer(Backbuffer *bb) {
  float scale; int off_x, off_y;
  compute_letterbox(g_x11.window_w, g_x11.window_h,
                    bb->width, bb->height, &scale, &off_x, &off_y);
  int dst_w = (int)(bb->width  * scale);
  int dst_h = (int)(bb->height * scale);

  glViewport(0, 0, g_x11.window_w, g_x11.window_h);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  glOrtho(0, g_x11.window_w, g_x11.window_h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bb->width, bb->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

  float x0 = (float)off_x, y0 = (float)off_y;
  float x1 = (float)(off_x + dst_w), y1 = (float)(off_y + dst_h);

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);
  glEnd();

  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * L23 GPU mode — init / render / cleanup
 * ═══════════════════════════════════════════════════════════════════════════
 */
#define LOAD_GL(name) \
  g_gpu.name = (PFN_gl##name)glXGetProcAddressARB((const GLubyte*)"gl" #name)

static int gpu_init(RaytracerState *state) {
  /* Load all GL 2.0+ function pointers. */
  LOAD_GL(CreateShader);     LOAD_GL(ShaderSource);
  LOAD_GL(CompileShader);    LOAD_GL(GetShaderiv);
  LOAD_GL(GetShaderInfoLog); LOAD_GL(CreateProgram);
  LOAD_GL(AttachShader);     LOAD_GL(LinkProgram);
  LOAD_GL(GetProgramiv);     LOAD_GL(GetProgramInfoLog);
  LOAD_GL(UseProgram);       LOAD_GL(GetUniformLocation);
  LOAD_GL(Uniform1f);        LOAD_GL(Uniform3f);
  LOAD_GL(Uniform4f);        LOAD_GL(DeleteShader);
  LOAD_GL(DeleteProgram);    LOAD_GL(GenVertexArrays);
  LOAD_GL(BindVertexArray);  LOAD_GL(DeleteVertexArrays);
  /* L24+ scene upload */
  LOAD_GL(Uniform1i);        LOAD_GL(Uniform1iv);
  LOAD_GL(Uniform1fv);       LOAD_GL(Uniform3fv);
  LOAD_GL(Uniform4fv);       LOAD_GL(Uniform1uiv);
  LOAD_GL(ActiveTexture);

  if (!g_gpu.CreateShader || !g_gpu.GenVertexArrays) {
    fprintf(stderr, "[GPU] GL 2.0+/3.0 functions not available\n");
    return -1;
  }

  double t0 = (double)clock() / (double)CLOCKS_PER_SEC;

  /* ── Vertex shader ──────────────────────────────────────────────────── */
  const char *vert_src = GPU_VERT_SHADER;
  GLuint vert = g_gpu.CreateShader(0x8B31 /* GL_VERTEX_SHADER */);
  g_gpu.ShaderSource(vert, 1, &vert_src, NULL);
  g_gpu.CompileShader(vert);
  {
    GLint ok = 0;
    g_gpu.GetShaderiv(vert, 0x8B81 /* GL_COMPILE_STATUS */, &ok);
    if (!ok) {
      char log[512];
      g_gpu.GetShaderInfoLog(vert, sizeof(log), NULL, log);
      fprintf(stderr, "[GPU] Vertex shader error:\n%s\n", log);
      g_gpu.DeleteShader(vert);
      return -1;
    }
  }

  /* ── Fragment shader (L24: data-driven scene shader) ────────────────── */
  char frag_src[65536];
  if (gpu_build_scene_fragment_source(frag_src, sizeof(frag_src)) != 0) {
    g_gpu.DeleteShader(vert);
    return -1;
  }
  const char *frag_src_ptr = frag_src;
  GLuint frag = g_gpu.CreateShader(0x8B30 /* GL_FRAGMENT_SHADER */);
  g_gpu.ShaderSource(frag, 1, &frag_src_ptr, NULL);
  g_gpu.CompileShader(frag);
  {
    GLint ok = 0;
    g_gpu.GetShaderiv(frag, 0x8B81, &ok);
    if (!ok) {
      char log[1024];
      g_gpu.GetShaderInfoLog(frag, sizeof(log), NULL, log);
      fprintf(stderr, "[GPU] Fragment shader error:\n%s\n", log);
      g_gpu.DeleteShader(vert);
      g_gpu.DeleteShader(frag);
      return -1;
    }
  }

  /* ── Link program ───────────────────────────────────────────────────── */
  g_gpu.program = g_gpu.CreateProgram();
  g_gpu.AttachShader(g_gpu.program, vert);
  g_gpu.AttachShader(g_gpu.program, frag);
  g_gpu.LinkProgram(g_gpu.program);
  {
    GLint ok = 0;
    g_gpu.GetProgramiv(g_gpu.program, 0x8B82 /* GL_LINK_STATUS */, &ok);
    if (!ok) {
      char log[1024];
      g_gpu.GetProgramInfoLog(g_gpu.program, sizeof(log), NULL, log);
      fprintf(stderr, "[GPU] Link error:\n%s\n", log);
      g_gpu.DeleteShader(vert);
      g_gpu.DeleteShader(frag);
      g_gpu.DeleteProgram(g_gpu.program);
      return -1;
    }
  }
  g_gpu.DeleteShader(vert);
  g_gpu.DeleteShader(frag);

  /* ── Uniform locations ──────────────────────────────────────────────── */
  g_gpu.u_resolution = g_gpu.GetUniformLocation(g_gpu.program, "iResolution");
  g_gpu.u_time       = g_gpu.GetUniformLocation(g_gpu.program, "iTime");
  g_gpu.u_mouse      = g_gpu.GetUniformLocation(g_gpu.program, "iMouse");
  g_gpu.u_cam_origin = g_gpu.GetUniformLocation(g_gpu.program, "iCamOrigin");
  g_gpu.u_cam_forward= g_gpu.GetUniformLocation(g_gpu.program, "iCamForward");
  g_gpu.u_cam_right  = g_gpu.GetUniformLocation(g_gpu.program, "iCamRight");
  g_gpu.u_cam_up     = g_gpu.GetUniformLocation(g_gpu.program, "iCamUp");
  g_gpu.u_cam_fov    = g_gpu.GetUniformLocation(g_gpu.program, "iCamFov");

  /* ── L24 scene uniform locations ─────────────────────────────────── */
#define ULOC(field, name) \
  g_gpu.field = g_gpu.GetUniformLocation(g_gpu.program, name)
  ULOC(u_mat_count, "uMatCount");     ULOC(u_mat_color, "uMatColor");
  ULOC(u_mat_albedo, "uMatAlbedo");   ULOC(u_mat_spec, "uMatSpec");
  ULOC(u_mat_ior, "uMatIOR");
  ULOC(u_sphere_count, "uSphereCount"); ULOC(u_sphere, "uSphere");
  ULOC(u_sphere_mat, "uSphereMat");
  ULOC(u_light_count, "uLightCount"); ULOC(u_light, "uLight");
  ULOC(u_box_count, "uBoxCount");     ULOC(u_box_center, "uBoxCenter");
  ULOC(u_box_half, "uBoxHalf");       ULOC(u_box_mat, "uBoxMat");
  ULOC(u_vox_count, "uVoxCount");     ULOC(u_vox_pos, "uVoxPos");
  ULOC(u_vox_scale, "uVoxScale");     ULOC(u_vox_mat, "uVoxMat");
  ULOC(u_vox_bbox_center, "uVoxBBoxCenter");
  ULOC(u_vox_bbox_half, "uVoxBBoxHalf");
  ULOC(u_vox_solid_count, "uVoxSolidCount");
  ULOC(u_vox_bitfield, "uVoxBitfield");
  ULOC(u_mesh_count, "uMeshCount");   ULOC(u_mesh_pos, "uMeshPos");
  ULOC(u_mesh_bbox_center, "uMeshBBoxCenter");
  ULOC(u_mesh_bbox_half, "uMeshBBoxHalf");
  ULOC(u_mesh_mat, "uMeshMat");       ULOC(u_mesh_tri_count, "uMeshTriCount");
  ULOC(u_mesh_data_off, "uMeshDataOff");
  ULOC(u_mesh_tri_tex, "uMeshTriTex");
  ULOC(u_env_mode, "uEnvMode");       ULOC(u_env_equirect, "uEnvEquirect");
  ULOC(u_env_cube, "uEnvCube");       ULOC(u_features, "uFeatures");
#undef ULOC

  /* ── Empty VAO (required for core-profile gl_VertexID) ──────────────── */
  g_gpu.GenVertexArrays(1, &g_gpu.vao);

  double t1 = (double)clock() / (double)CLOCKS_PER_SEC;
  state->gpu_compile_ms = (float)((t1 - t0) * 1000.0);

  /* Store renderer string for HUD. */
  const char *renderer = (const char *)glGetString(GL_RENDERER);
  if (renderer) {
    strncpy(state->gpu_renderer, renderer, sizeof(state->gpu_renderer) - 1);
    state->gpu_renderer[sizeof(state->gpu_renderer) - 1] = '\0';
  } else {
    strcpy(state->gpu_renderer, "Unknown");
  }

  g_gpu.ready = 1;
  state->gpu_available = 1;
  printf("[GPU] Shader compiled in %.1f ms | %s\n",
         state->gpu_compile_ms, state->gpu_renderer);
  return 0;
}

/* Upload static scene data to GPU uniforms + textures (called once). */
static void gpu_upload_scene(const Scene *scene, const RenderSettings *settings) {
  if (!g_gpu.ready || !g_gpu.Uniform1i) return;

  GpuSceneData d;
  gpu_pack_scene(&d, scene, settings);

  g_gpu.UseProgram(g_gpu.program);

  /* Materials */
  g_gpu.Uniform1i(g_gpu.u_mat_count, d.mat_count);
  if (d.mat_count > 0) {
    g_gpu.Uniform3fv(g_gpu.u_mat_color,  d.mat_count, d.mat_color);
    g_gpu.Uniform4fv(g_gpu.u_mat_albedo, d.mat_count, d.mat_albedo);
    g_gpu.Uniform1fv(g_gpu.u_mat_spec,   d.mat_count, d.mat_spec);
    g_gpu.Uniform1fv(g_gpu.u_mat_ior,    d.mat_count, d.mat_ior);
  }

  /* Spheres */
  g_gpu.Uniform1i(g_gpu.u_sphere_count, d.sphere_count);
  if (d.sphere_count > 0) {
    g_gpu.Uniform4fv(g_gpu.u_sphere, d.sphere_count, d.sphere);
    g_gpu.Uniform1iv(g_gpu.u_sphere_mat, d.sphere_count, d.sphere_mat);
  }

  /* Lights */
  g_gpu.Uniform1i(g_gpu.u_light_count, d.light_count);
  if (d.light_count > 0)
    g_gpu.Uniform4fv(g_gpu.u_light, d.light_count, d.light);

  /* Boxes */
  g_gpu.Uniform1i(g_gpu.u_box_count, d.box_count);
  if (d.box_count > 0) {
    g_gpu.Uniform3fv(g_gpu.u_box_center, d.box_count, d.box_center);
    g_gpu.Uniform3fv(g_gpu.u_box_half,   d.box_count, d.box_half);
    g_gpu.Uniform1iv(g_gpu.u_box_mat,    d.box_count, d.box_mat);
  }

  /* Voxels */
  g_gpu.Uniform1i(g_gpu.u_vox_count, d.voxel_count);
  if (d.voxel_count > 0) {
    g_gpu.Uniform3fv(g_gpu.u_vox_pos,         d.voxel_count, d.vox_pos);
    g_gpu.Uniform1fv(g_gpu.u_vox_scale,       d.voxel_count, d.vox_scale);
    g_gpu.Uniform1iv(g_gpu.u_vox_mat,         d.voxel_count, d.vox_mat);
    g_gpu.Uniform3fv(g_gpu.u_vox_bbox_center, d.voxel_count, d.vox_bbox_center);
    g_gpu.Uniform3fv(g_gpu.u_vox_bbox_half,   d.voxel_count, d.vox_bbox_half);
    g_gpu.Uniform1iv(g_gpu.u_vox_solid_count, d.voxel_count, d.vox_solid_count);
    if (g_gpu.Uniform1uiv)
      g_gpu.Uniform1uiv(g_gpu.u_vox_bitfield, d.voxel_count * 5,
                         d.vox_bitfield);
  }

  /* Meshes */
  g_gpu.Uniform1i(g_gpu.u_mesh_count, d.mesh_count);
  if (d.mesh_count > 0) {
    g_gpu.Uniform3fv(g_gpu.u_mesh_pos,         d.mesh_count, d.mesh_pos);
    g_gpu.Uniform3fv(g_gpu.u_mesh_bbox_center, d.mesh_count, d.mesh_bbox_center);
    g_gpu.Uniform3fv(g_gpu.u_mesh_bbox_half,   d.mesh_count, d.mesh_bbox_half);
    g_gpu.Uniform1iv(g_gpu.u_mesh_mat,         d.mesh_count, d.mesh_mat);
    g_gpu.Uniform1iv(g_gpu.u_mesh_tri_count,   d.mesh_count, d.mesh_tri_count);
    g_gpu.Uniform1iv(g_gpu.u_mesh_data_off,    d.mesh_count, d.mesh_data_offset);
  }

  /* Mesh triangle texture (RGBA32F) */
  if (d.tri_tex_data && d.tri_tex_total > 0) {
    glGenTextures(1, &g_gpu.tex_mesh_tri);
    glBindTexture(GL_TEXTURE_2D, g_gpu.tex_mesh_tri);
    glTexImage2D(GL_TEXTURE_2D, 0, 0x8814 /* GL_RGBA32F */,
                 d.tri_tex_width, 1, 0, GL_RGBA, GL_FLOAT, d.tri_tex_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  /* Environment map textures */
  const EnvMap *em = &scene->envmap;
  if (em->pixels && em->width > 0 && em->height > 0) {
    glGenTextures(1, &g_gpu.tex_env_equirect);
    glBindTexture(GL_TEXTURE_2D, g_gpu.tex_env_equirect);
    GLenum fmt = (em->channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, em->width, em->height,
                 0, fmt, GL_UNSIGNED_BYTE, em->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  if (em->faces[0].pixels) {
    glGenTextures(1, &g_gpu.tex_env_cube);
    glBindTexture(0x8513 /* GL_TEXTURE_CUBE_MAP */, g_gpu.tex_env_cube);
    for (int f = 0; f < 6; f++) {
      const CubeMapFace *face = &em->faces[f];
      if (!face->pixels) continue;
      GLenum target = 0x8515 /* GL_TEXTURE_CUBE_MAP_POSITIVE_X */ + (GLenum)f;
      GLenum cfmt = (face->channels == 4) ? GL_RGBA : GL_RGB;
      glTexImage2D(target, 0, GL_RGB, face->width, face->height,
                   0, cfmt, GL_UNSIGNED_BYTE, face->pixels);
    }
    glTexParameteri(0x8513, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(0x8513, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(0x8513, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(0x8513, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(0x8513, 0x8072 /* GL_TEXTURE_WRAP_R */, GL_CLAMP_TO_EDGE);
  }

  g_gpu.UseProgram(0);
  gpu_scene_data_free(&d);
  g_gpu.scene_uploaded = 1;
  printf("[GPU] Scene uploaded: %d mat, %d sph, %d light, %d box, %d vox, %d mesh\n",
         d.mat_count, d.sphere_count, d.light_count,
         d.box_count, d.voxel_count, d.mesh_count);
}

static void gpu_render_scene(float time_sec, float mouse_x, float mouse_y,
                             const RtCamera *cam, const Scene *scene,
                             const RenderSettings *settings) {
  if (!g_gpu.ready) return;

  /* Switch to modern shader pipeline. */
  glDisable(GL_TEXTURE_2D);
  g_gpu.UseProgram(g_gpu.program);

  /* Set Shadertoy-compatible uniforms. */
  float w = (float)g_x11.window_w;
  float h = (float)g_x11.window_h;
  g_gpu.Uniform3f(g_gpu.u_resolution, w, h, 1.0f);
  g_gpu.Uniform1f(g_gpu.u_time, time_sec);
  g_gpu.Uniform4f(g_gpu.u_mouse, mouse_x, mouse_y, 0.0f, 0.0f);

  /* Set camera uniforms from CPU orbit camera state. */
  if (cam) {
    CameraBasis cb = camera_compute_basis(cam, (int)w, (int)h);
    g_gpu.Uniform3f(g_gpu.u_cam_origin,  cb.origin.x,  cb.origin.y,  cb.origin.z);
    g_gpu.Uniform3f(g_gpu.u_cam_forward, cb.forward.x, cb.forward.y, cb.forward.z);
    g_gpu.Uniform3f(g_gpu.u_cam_right,   cb.right.x,   cb.right.y,   cb.right.z);
    g_gpu.Uniform3f(g_gpu.u_cam_up,      cb.up.x,      cb.up.y,      cb.up.z);
    g_gpu.Uniform1f(g_gpu.u_cam_fov,     cam->fov);
  }

  /* Per-frame uniforms: feature toggles + envmap mode */
  if (g_gpu.Uniform1i) {
    g_gpu.Uniform1i(g_gpu.u_features, gpu_pack_features(settings));
    g_gpu.Uniform1i(g_gpu.u_env_mode, (int)scene->envmap.mode);
  }

  /* Bind textures */
  if (g_gpu.ActiveTexture) {
    g_gpu.ActiveTexture(0x84C0 /* GL_TEXTURE0 */);
    glBindTexture(GL_TEXTURE_2D, g_gpu.tex_mesh_tri ? g_gpu.tex_mesh_tri : 0);
    if (g_gpu.Uniform1i) g_gpu.Uniform1i(g_gpu.u_mesh_tri_tex, 0);

    g_gpu.ActiveTexture(0x84C1 /* GL_TEXTURE1 */);
    glBindTexture(GL_TEXTURE_2D, g_gpu.tex_env_equirect ? g_gpu.tex_env_equirect : 0);
    if (g_gpu.Uniform1i) g_gpu.Uniform1i(g_gpu.u_env_equirect, 1);

    g_gpu.ActiveTexture(0x84C2 /* GL_TEXTURE2 */);
    glBindTexture(0x8513 /* GL_TEXTURE_CUBE_MAP */,
                  g_gpu.tex_env_cube ? g_gpu.tex_env_cube : 0);
    if (g_gpu.Uniform1i) g_gpu.Uniform1i(g_gpu.u_env_cube, 2);

    g_gpu.ActiveTexture(0x84C0 /* GL_TEXTURE0 */);
  }

  /* Draw fullscreen triangle. */
  glViewport(0, 0, g_x11.window_w, g_x11.window_h);
  g_gpu.BindVertexArray(g_gpu.vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  g_gpu.BindVertexArray(0);

  /* Restore legacy state for HUD texture blit. */
  g_gpu.UseProgram(0);
  glEnable(GL_TEXTURE_2D);
}

/* Overlay the HUD backbuffer on top of the GPU-rendered scene.
 * Uses alpha blending so GPU raytracing shows through transparent areas. */
static void gpu_overlay_hud(Backbuffer *bb) {
  glViewport(0, 0, g_x11.window_w, g_x11.window_h);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  glOrtho(0, g_x11.window_w, g_x11.window_h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();

  /* Upload HUD pixels to texture. */
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bb->width, bb->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

  /* Alpha blend so the HUD background shows while GPU content shines through. */
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  float scale; int off_x, off_y;
  compute_letterbox(g_x11.window_w, g_x11.window_h,
                    bb->width, bb->height, &scale, &off_x, &off_y);
  float x0 = (float)off_x, y0 = (float)off_y;
  float x1 = (float)(off_x + (int)(bb->width  * scale));
  float y1 = (float)(off_y + (int)(bb->height * scale));

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);
  glEnd();

  glDisable(GL_BLEND);
  glXSwapBuffers(g_x11.display, g_x11.window);
}

static void gpu_shutdown(void) {
  if (g_gpu.tex_mesh_tri) glDeleteTextures(1, &g_gpu.tex_mesh_tri);
  if (g_gpu.tex_env_equirect) glDeleteTextures(1, &g_gpu.tex_env_equirect);
  if (g_gpu.tex_env_cube) glDeleteTextures(1, &g_gpu.tex_env_cube);
  if (g_gpu.program && g_gpu.DeleteProgram)
    g_gpu.DeleteProgram(g_gpu.program);
  if (g_gpu.vao && g_gpu.DeleteVertexArrays)
    g_gpu.DeleteVertexArrays(1, &g_gpu.vao);
  memset(&g_gpu, 0, sizeof(g_gpu));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */
int main(void) {
  if (init_window() != 0) return 1;
  setup_vsync();

  /* ── Backbuffer ──────────────────────────────────────────────────────── */
  Backbuffer bb = {
    .width = GAME_W, .height = GAME_H,
    .bytes_per_pixel = 4, .pitch = GAME_W * 4,
  };
  bb.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
  if (!bb.pixels) { fprintf(stderr, "Out of memory\n"); return 1; }
  memset(bb.pixels, 0, (size_t)(GAME_W * GAME_H) * 4);

  /* ── Game state ──────────────────────────────────────────────────────── */
  RaytracerState state;
  game_init(&state);

  /* ── L23 GPU shader init + L24 scene upload ────────────────────────── */
  gpu_init(&state); /* non-fatal: gpu_available stays 0 on failure */
  if (g_gpu.ready)
    gpu_upload_scene(&state.scene, &state.settings);

  /* ── Input double buffer ─────────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  /* ── Main loop ───────────────────────────────────────────────────────── */
  float elapsed_time = 0.0f; /* for iTime uniform in GPU mode */
  int is_running = 1;
  while (is_running) {
    timing_begin();
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);
    process_events(curr_input, &is_running);
    if (curr_input->buttons.quit.ended_down) break;

    float dt = g_timing.total_seconds > 0.0f
               ? g_timing.total_seconds
               : (1.0f / TARGET_FPS);
    elapsed_time += dt;

    game_update(&state, curr_input, dt);
    game_render(&state, &bb);

    /* Display: GPU mode renders via shader, then overlays HUD.
     * CPU modes blit the backbuffer as before. */
    if (state.settings.render_mode == RENDER_GPU && g_gpu.ready) {
      gpu_render_scene(elapsed_time, curr_input->mouse.x, curr_input->mouse.y,
                       &state.camera, &state.scene, &state.settings);
      gpu_overlay_hud(&bb);
    } else {
      display_backbuffer(&bb);
    }

    timing_mark_work_done();
    if (!g_x11.vsync_enabled) timing_sleep_until_target();
    timing_end();
  }

  /* ── Cleanup ─────────────────────────────────────────────────────────── */
#ifdef DEBUG
  print_frame_stats();
#endif
  gpu_shutdown();
  free(bb.pixels);
  shutdown_window();
  return 0;
}
