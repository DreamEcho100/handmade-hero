/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/raylib/main.c — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform.h"
#include "../../game/base.h"
#include "../../game/main.h"
#include "../../game/render.h"
#include "../../game/gpu_shader.h"
#include "../../game/gpu_upload.h"
#include "../../game/settings.h"
#include <rlgl.h>
/* Raw GL for array uniform uploads + texture creation.
 * Raylib's high-level API doesn't support glUniform*v arrays,
 * RGBA32F textures, or cubemaps.  Load via dlsym at runtime.      */
#include <dlfcn.h>
typedef void (*PFN_glUniform1i)(int, int);
typedef void (*PFN_glUniform1iv)(int, int, const int *);
typedef void (*PFN_glUniform1fv)(int, int, const float *);
typedef void (*PFN_glUniform3fv)(int, int, const float *);
typedef void (*PFN_glUniform4fv)(int, int, const float *);
typedef void (*PFN_glUniform1uiv)(int, int, const unsigned int *);
typedef void (*PFN_glActiveTexture)(unsigned int);
typedef void (*PFN_glGenTextures)(int, unsigned int *);
typedef void (*PFN_glBindTexture)(unsigned int, unsigned int);
typedef void (*PFN_glTexImage2D)(unsigned int, int, int, int, int, int,
                                 unsigned int, unsigned int, const void *);
typedef void (*PFN_glTexParameteri)(unsigned int, unsigned int, int);
typedef void (*PFN_glDeleteTextures)(int, const unsigned int *);

static struct {
  PFN_glUniform1i Uniform1i;
  PFN_glUniform1iv Uniform1iv;
  PFN_glUniform1fv Uniform1fv;
  PFN_glUniform3fv Uniform3fv;
  PFN_glUniform4fv Uniform4fv;
  PFN_glUniform1uiv Uniform1uiv;
  PFN_glActiveTexture ActiveTexture;
  PFN_glGenTextures GenTextures;
  PFN_glBindTexture BindTexture;
  PFN_glTexImage2D TexImage2D;
  PFN_glTexParameteri TexParameteri;
  PFN_glDeleteTextures DeleteTextures;
} g_gl = {0};

static void load_gl_functions(void) {
  void *lib = dlopen("libGL.so.1", RTLD_LAZY);
  if (!lib) lib = dlopen("libGL.so", RTLD_LAZY);
  if (!lib) return;
#define LGL(name) g_gl.name = (PFN_gl##name)dlsym(lib, "gl" #name)
  LGL(Uniform1i);  LGL(Uniform1iv); LGL(Uniform1fv);
  LGL(Uniform3fv); LGL(Uniform4fv); LGL(Uniform1uiv);
  LGL(ActiveTexture); LGL(GenTextures); LGL(BindTexture);
  LGL(TexImage2D); LGL(TexParameteri); LGL(DeleteTextures);
#undef LGL
  /* Don't dlclose — keep symbols alive */
}
#include "../../utils/math.h"

static Texture2D g_texture = {0};

/* ═══════════════════════════════════════════════════════════════════════════
 * L23 GPU rendering — Raylib shader state
 * ═══════════════════════════════════════════════════════════════════════════
 * Raylib wraps OpenGL shaders with LoadShaderFromMemory / BeginShaderMode.
 * Much simpler than the raw GL path in X11 — Raylib handles all the
 * function pointer loading, VAO creation, and uniform binding internally.
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  int   ready;
  Shader shader;
  int   u_resolution, u_time, u_mouse;
  int   u_cam_origin, u_cam_forward, u_cam_right, u_cam_up, u_cam_fov;
  /* L24+ scene uniforms */
  int   u_mat_count, u_mat_color, u_mat_albedo, u_mat_spec, u_mat_ior;
  int   u_sphere_count, u_sphere, u_sphere_mat;
  int   u_light_count, u_light;
  int   u_box_count, u_box_center, u_box_half, u_box_mat;
  int   u_vox_count, u_vox_pos, u_vox_scale, u_vox_mat;
  int   u_vox_bbox_center, u_vox_bbox_half, u_vox_solid_count, u_vox_bitfield;
  int   u_mesh_count, u_mesh_pos, u_mesh_bbox_center, u_mesh_bbox_half;
  int   u_mesh_mat, u_mesh_tri_count, u_mesh_data_off, u_mesh_tri_tex;
  int   u_env_mode, u_env_equirect, u_env_cube;
  int   u_features;
  /* Texture IDs */
  unsigned int tex_mesh_tri;
  unsigned int tex_env_equirect;
  unsigned int tex_env_cube;
  int   scene_uploaded;
} RaylibGpuState;

static RaylibGpuState g_rl_gpu = {0};

static void display_backbuffer(Backbuffer *bb) {
  UpdateTexture(g_texture, bb->pixels);
  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();
  float sx = (float)win_w / (float)bb->width;
  float sy = (float)win_h / (float)bb->height;
  float scale = MIN(sx, sy);
  float dst_w = (float)bb->width  * scale;
  float dst_h = (float)bb->height * scale;
  float off_x = ((float)win_w - dst_w) * 0.5f;
  float off_y = ((float)win_h - dst_h) * 0.5f;

  Rectangle src  = { 0.0f, 0.0f, (float)bb->width, (float)bb->height };
  Rectangle dest = { off_x, off_y, dst_w, dst_h };

  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexturePro(g_texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
  EndDrawing();
}

/* ── L23 GPU init / render / overlay / cleanup ────────────────────────── */
static int rl_gpu_init(RaytracerState *state) {
  load_gl_functions();

  char frag_src[65536];
  if (gpu_build_scene_fragment_source(frag_src, sizeof(frag_src)) != 0)
    return -1;

  double t0 = GetTime();
  g_rl_gpu.shader = LoadShaderFromMemory(GPU_VERT_SHADER, frag_src);
  double t1 = GetTime();

  if (!IsShaderValid(g_rl_gpu.shader)) {
    /* Fallback: try checking shader.id != 0 (Raylib convention). */
    if (g_rl_gpu.shader.id == 0) {
      fprintf(stderr, "[GPU] Raylib shader compile failed\n");
      return -1;
    }
  }

  g_rl_gpu.u_resolution = GetShaderLocation(g_rl_gpu.shader, "iResolution");
  g_rl_gpu.u_time       = GetShaderLocation(g_rl_gpu.shader, "iTime");
  g_rl_gpu.u_mouse      = GetShaderLocation(g_rl_gpu.shader, "iMouse");
  g_rl_gpu.u_cam_origin = GetShaderLocation(g_rl_gpu.shader, "iCamOrigin");
  g_rl_gpu.u_cam_forward= GetShaderLocation(g_rl_gpu.shader, "iCamForward");
  g_rl_gpu.u_cam_right  = GetShaderLocation(g_rl_gpu.shader, "iCamRight");
  g_rl_gpu.u_cam_up     = GetShaderLocation(g_rl_gpu.shader, "iCamUp");
  g_rl_gpu.u_cam_fov    = GetShaderLocation(g_rl_gpu.shader, "iCamFov");

  /* L24+ scene uniform locations */
#define SLOC(field, name) \
  g_rl_gpu.field = GetShaderLocation(g_rl_gpu.shader, name)
  SLOC(u_mat_count, "uMatCount");     SLOC(u_mat_color, "uMatColor");
  SLOC(u_mat_albedo, "uMatAlbedo");   SLOC(u_mat_spec, "uMatSpec");
  SLOC(u_mat_ior, "uMatIOR");
  SLOC(u_sphere_count, "uSphereCount"); SLOC(u_sphere, "uSphere");
  SLOC(u_sphere_mat, "uSphereMat");
  SLOC(u_light_count, "uLightCount"); SLOC(u_light, "uLight");
  SLOC(u_box_count, "uBoxCount");     SLOC(u_box_center, "uBoxCenter");
  SLOC(u_box_half, "uBoxHalf");       SLOC(u_box_mat, "uBoxMat");
  SLOC(u_vox_count, "uVoxCount");     SLOC(u_vox_pos, "uVoxPos");
  SLOC(u_vox_scale, "uVoxScale");     SLOC(u_vox_mat, "uVoxMat");
  SLOC(u_vox_bbox_center, "uVoxBBoxCenter");
  SLOC(u_vox_bbox_half, "uVoxBBoxHalf");
  SLOC(u_vox_solid_count, "uVoxSolidCount");
  SLOC(u_vox_bitfield, "uVoxBitfield");
  SLOC(u_mesh_count, "uMeshCount");   SLOC(u_mesh_pos, "uMeshPos");
  SLOC(u_mesh_bbox_center, "uMeshBBoxCenter");
  SLOC(u_mesh_bbox_half, "uMeshBBoxHalf");
  SLOC(u_mesh_mat, "uMeshMat");       SLOC(u_mesh_tri_count, "uMeshTriCount");
  SLOC(u_mesh_data_off, "uMeshDataOff");
  SLOC(u_mesh_tri_tex, "uMeshTriTex");
  SLOC(u_env_mode, "uEnvMode");       SLOC(u_env_equirect, "uEnvEquirect");
  SLOC(u_env_cube, "uEnvCube");       SLOC(u_features, "uFeatures");
#undef SLOC

  state->gpu_compile_ms = (float)((t1 - t0) * 1000.0);

  /* Store renderer string for HUD (Raylib doesn't expose GL easily,
   * but we can call rlGetVersion/rlglGetString — just use a static). */
  snprintf(state->gpu_renderer, sizeof(state->gpu_renderer),
           "Raylib %s (OpenGL)", RAYLIB_VERSION);

  g_rl_gpu.ready = 1;
  state->gpu_available = 1;
  printf("[GPU] Raylib shader compiled in %.1f ms\n", state->gpu_compile_ms);
  return 0;
}

/* Upload static scene data via raw GL calls (Raylib doesn't expose array uniforms). */
static void rl_gpu_upload_scene(const Scene *scene, const RenderSettings *settings) {
  if (!g_rl_gpu.ready) return;

  GpuSceneData d;
  gpu_pack_scene(&d, scene, settings);

  /* Use raw GL via rlgl to set array uniforms */
  int id = g_rl_gpu.shader.id;
  rlEnableShader(id);

  if (!g_gl.Uniform1i) { rlDisableShader(); gpu_scene_data_free(&d); return; }

  g_gl.Uniform1i(g_rl_gpu.u_mat_count, d.mat_count);
  if (d.mat_count > 0) {
    g_gl.Uniform3fv(g_rl_gpu.u_mat_color, d.mat_count, d.mat_color);
    g_gl.Uniform4fv(g_rl_gpu.u_mat_albedo, d.mat_count, d.mat_albedo);
    g_gl.Uniform1fv(g_rl_gpu.u_mat_spec, d.mat_count, d.mat_spec);
    g_gl.Uniform1fv(g_rl_gpu.u_mat_ior, d.mat_count, d.mat_ior);
  }
  g_gl.Uniform1i(g_rl_gpu.u_sphere_count, d.sphere_count);
  if (d.sphere_count > 0) {
    g_gl.Uniform4fv(g_rl_gpu.u_sphere, d.sphere_count, d.sphere);
    g_gl.Uniform1iv(g_rl_gpu.u_sphere_mat, d.sphere_count, d.sphere_mat);
  }
  g_gl.Uniform1i(g_rl_gpu.u_light_count, d.light_count);
  if (d.light_count > 0)
    g_gl.Uniform4fv(g_rl_gpu.u_light, d.light_count, d.light);
  g_gl.Uniform1i(g_rl_gpu.u_box_count, d.box_count);
  if (d.box_count > 0) {
    g_gl.Uniform3fv(g_rl_gpu.u_box_center, d.box_count, d.box_center);
    g_gl.Uniform3fv(g_rl_gpu.u_box_half, d.box_count, d.box_half);
    g_gl.Uniform1iv(g_rl_gpu.u_box_mat, d.box_count, d.box_mat);
  }
  g_gl.Uniform1i(g_rl_gpu.u_vox_count, d.voxel_count);
  if (d.voxel_count > 0) {
    g_gl.Uniform3fv(g_rl_gpu.u_vox_pos, d.voxel_count, d.vox_pos);
    g_gl.Uniform1fv(g_rl_gpu.u_vox_scale, d.voxel_count, d.vox_scale);
    g_gl.Uniform1iv(g_rl_gpu.u_vox_mat, d.voxel_count, d.vox_mat);
    g_gl.Uniform3fv(g_rl_gpu.u_vox_bbox_center, d.voxel_count, d.vox_bbox_center);
    g_gl.Uniform3fv(g_rl_gpu.u_vox_bbox_half, d.voxel_count, d.vox_bbox_half);
    g_gl.Uniform1iv(g_rl_gpu.u_vox_solid_count, d.voxel_count, d.vox_solid_count);
    if (g_gl.Uniform1uiv)
      g_gl.Uniform1uiv(g_rl_gpu.u_vox_bitfield, d.voxel_count * 5, d.vox_bitfield);
  }
  g_gl.Uniform1i(g_rl_gpu.u_mesh_count, d.mesh_count);
  if (d.mesh_count > 0) {
    g_gl.Uniform3fv(g_rl_gpu.u_mesh_pos, d.mesh_count, d.mesh_pos);
    g_gl.Uniform3fv(g_rl_gpu.u_mesh_bbox_center, d.mesh_count, d.mesh_bbox_center);
    g_gl.Uniform3fv(g_rl_gpu.u_mesh_bbox_half, d.mesh_count, d.mesh_bbox_half);
    g_gl.Uniform1iv(g_rl_gpu.u_mesh_mat, d.mesh_count, d.mesh_mat);
    g_gl.Uniform1iv(g_rl_gpu.u_mesh_tri_count, d.mesh_count, d.mesh_tri_count);
    g_gl.Uniform1iv(g_rl_gpu.u_mesh_data_off, d.mesh_count, d.mesh_data_offset);
  }

  /* Mesh triangle texture (RGBA32F) */
  if (d.tri_tex_data && d.tri_tex_total > 0 && g_gl.GenTextures) {
    g_gl.GenTextures(1, &g_rl_gpu.tex_mesh_tri);
    g_gl.BindTexture(0x0DE1 /* GL_TEXTURE_2D */, g_rl_gpu.tex_mesh_tri);
    g_gl.TexImage2D(0x0DE1, 0, 0x8814 /* GL_RGBA32F */,
                    d.tri_tex_width, 1, 0, 0x1908 /* GL_RGBA */,
                    0x1406 /* GL_FLOAT */, d.tri_tex_data);
    g_gl.TexParameteri(0x0DE1, 0x2801 /* GL_TEXTURE_MIN_FILTER */, 0x2600 /* GL_NEAREST */);
    g_gl.TexParameteri(0x0DE1, 0x2800 /* GL_TEXTURE_MAG_FILTER */, 0x2600);
  }

  /* Environment map textures */
  const EnvMap *em = &scene->envmap;
  if (em->pixels && em->width > 0 && em->height > 0 && g_gl.GenTextures) {
    g_gl.GenTextures(1, &g_rl_gpu.tex_env_equirect);
    g_gl.BindTexture(0x0DE1, g_rl_gpu.tex_env_equirect);
    unsigned int fmt = (em->channels == 4) ? 0x1908 /* GL_RGBA */ : 0x1907 /* GL_RGB */;
    g_gl.TexImage2D(0x0DE1, 0, 0x1907 /* GL_RGB */, em->width, em->height,
                    0, fmt, 0x1401 /* GL_UNSIGNED_BYTE */, em->pixels);
    g_gl.TexParameteri(0x0DE1, 0x2801, 0x2601 /* GL_LINEAR */);
    g_gl.TexParameteri(0x0DE1, 0x2800, 0x2601);
    g_gl.TexParameteri(0x0DE1, 0x2802 /* GL_TEXTURE_WRAP_S */, 0x2901 /* GL_REPEAT */);
    g_gl.TexParameteri(0x0DE1, 0x2803 /* GL_TEXTURE_WRAP_T */, 0x812F /* GL_CLAMP_TO_EDGE */);
  }
  if (em->faces[0].pixels && g_gl.GenTextures) {
    g_gl.GenTextures(1, &g_rl_gpu.tex_env_cube);
    g_gl.BindTexture(0x8513 /* GL_TEXTURE_CUBE_MAP */, g_rl_gpu.tex_env_cube);
    for (int f = 0; f < 6; f++) {
      const CubeMapFace *face = &em->faces[f];
      if (!face->pixels) continue;
      unsigned int target = 0x8515 + (unsigned int)f;
      unsigned int cfmt = (face->channels == 4) ? 0x1908 : 0x1907;
      g_gl.TexImage2D(target, 0, 0x1907, face->width, face->height,
                      0, cfmt, 0x1401, face->pixels);
    }
    g_gl.TexParameteri(0x8513, 0x2801, 0x2601);
    g_gl.TexParameteri(0x8513, 0x2800, 0x2601);
    g_gl.TexParameteri(0x8513, 0x2802, 0x812F);
    g_gl.TexParameteri(0x8513, 0x2803, 0x812F);
    g_gl.TexParameteri(0x8513, 0x8072 /* GL_TEXTURE_WRAP_R */, 0x812F);
  }

  rlDisableShader();

  gpu_scene_data_free(&d);
  g_rl_gpu.scene_uploaded = 1;
}

static void rl_gpu_render(float elapsed_time, float mouse_x, float mouse_y,
                          const RtCamera *cam, const Scene *scene,
                          const RenderSettings *settings) {
  if (!g_rl_gpu.ready) return;

  float w = (float)GetScreenWidth();
  float h = (float)GetScreenHeight();
  float res[3] = { w, h, 1.0f };
  float mouse[4] = { mouse_x, mouse_y, 0.0f, 0.0f };

  SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_resolution, res, SHADER_UNIFORM_VEC3);
  SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_time, &elapsed_time, SHADER_UNIFORM_FLOAT);
  SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_mouse, mouse, SHADER_UNIFORM_VEC4);

  /* Set camera uniforms from CPU orbit camera state. */
  if (cam) {
    CameraBasis cb = camera_compute_basis(cam, (int)w, (int)h);
    float origin[3]  = { cb.cam_pos.x,     cb.cam_pos.y,     cb.cam_pos.z };
    float forward[3] = { cb.cam_forward.x, cb.cam_forward.y, cb.cam_forward.z };
    float right[3]   = { cb.cam_right.x,   cb.cam_right.y,   cb.cam_right.z };
    float up[3]      = { cb.cam_up.x,      cb.cam_up.y,      cb.cam_up.z };
    float fov_val    = cam->fov;
    SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_cam_origin,  origin,  SHADER_UNIFORM_VEC3);
    SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_cam_forward, forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_cam_right,   right,   SHADER_UNIFORM_VEC3);
    SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_cam_up,      up,      SHADER_UNIFORM_VEC3);
    SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_cam_fov,     &fov_val,SHADER_UNIFORM_FLOAT);
  }

  /* Per-frame uniforms: feature toggles + envmap mode */
  int features = gpu_pack_features(settings);
  int env_mode = (int)scene->envmap.mode;
  SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_features, &features, SHADER_UNIFORM_INT);
  SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_env_mode, &env_mode, SHADER_UNIFORM_INT);

  BeginDrawing();
  ClearBackground(BLACK);
  BeginShaderMode(g_rl_gpu.shader);

  /* Bind textures to units 3-5 to avoid Raylib's internal texture
   * management on unit 0 (DrawRectangle binds Raylib's white texture
   * to unit 0 via rlSetTexture, which would overwrite our mesh data). */
  if (g_gl.ActiveTexture) {
    g_gl.ActiveTexture(0x84C3); /* GL_TEXTURE3 */
    g_gl.BindTexture(0x0DE1, g_rl_gpu.tex_mesh_tri);
    int tex3 = 3;
    SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_mesh_tri_tex, &tex3, SHADER_UNIFORM_INT);
    g_gl.ActiveTexture(0x84C4); /* GL_TEXTURE4 */
    g_gl.BindTexture(0x0DE1, g_rl_gpu.tex_env_equirect);
    int tex4 = 4;
    SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_env_equirect, &tex4, SHADER_UNIFORM_INT);
    g_gl.ActiveTexture(0x84C5); /* GL_TEXTURE5 */
    g_gl.BindTexture(0x8513, g_rl_gpu.tex_env_cube);
    int tex5 = 5;
    SetShaderValue(g_rl_gpu.shader, g_rl_gpu.u_env_cube, &tex5, SHADER_UNIFORM_INT);
    g_gl.ActiveTexture(0x84C0); /* restore unit 0 for Raylib */
  }

  /* Draw a fullscreen rectangle — Raylib applies the shader to it. */
  DrawRectangle(0, 0, (int)w, (int)h, WHITE);
  EndShaderMode();
}

/* Overlay the HUD texture on top of GPU rendering (called between
 * BeginDrawing and EndDrawing — rl_gpu_render already called BeginDrawing). */
static void rl_gpu_overlay_hud(Backbuffer *bb) {
  UpdateTexture(g_texture, bb->pixels);
  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();
  float sx = (float)win_w / (float)bb->width;
  float sy = (float)win_h / (float)bb->height;
  float scale = MIN(sx, sy);
  float dst_w = (float)bb->width  * scale;
  float dst_h = (float)bb->height * scale;
  float off_x = ((float)win_w - dst_w) * 0.5f;
  float off_y = ((float)win_h - dst_h) * 0.5f;

  Rectangle src  = { 0.0f, 0.0f, (float)bb->width, (float)bb->height };
  Rectangle dest = { off_x, off_y, dst_w, dst_h };
  DrawTexturePro(g_texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
  EndDrawing();
}

static void rl_gpu_shutdown(void) {
  if (g_rl_gpu.ready && g_rl_gpu.shader.id != 0)
    UnloadShader(g_rl_gpu.shader);
  memset(&g_rl_gpu, 0, sizeof(g_rl_gpu));
}

int main(void) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(GAME_W, GAME_H, TITLE);
  SetTargetFPS(TARGET_FPS);
  if (!IsWindowReady()) { fprintf(stderr, "InitWindow failed\n"); return 1; }
  SetWindowState(FLAG_WINDOW_RESIZABLE);

  /* ── Platform props (backbuffer + audio buffer + arenas) ─────────────── */
  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    CloseWindow(); return 1;
  }

  Image img = {
    .data = props.backbuffer.pixels, .width = GAME_W, .height = GAME_H,
    .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  g_texture = LoadTextureFromImage(img);
  SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);
  if (!IsTextureValid(g_texture)) {
    platform_game_props_free(&props); CloseWindow(); return 1;
  }

  RaytracerState state;
  game_init(&state);

  /* ── L23 GPU shader init + L24 scene upload ────────────────────────── */
  rl_gpu_init(&state); /* non-fatal: gpu_available stays 0 on failure */
  if (g_rl_gpu.ready)
    rl_gpu_upload_scene(&state.scene, &state.settings);

  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  float prev_mouse_x = 0.0f, prev_mouse_y = 0.0f;
  float elapsed_time = 0.0f;

  while (!WindowShouldClose()) {
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* ── Keyboard ──────────────────────────────────────────────────────── */
    UPDATE_BUTTON(&curr_input->buttons.quit,
                  IsKeyDown(KEY_ESCAPE) || IsKeyDown(KEY_Q));
    UPDATE_BUTTON(&curr_input->buttons.camera_left,
                  IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A));
    UPDATE_BUTTON(&curr_input->buttons.camera_right,
                  IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D));
    UPDATE_BUTTON(&curr_input->buttons.camera_up,
                  IsKeyDown(KEY_UP));
    UPDATE_BUTTON(&curr_input->buttons.camera_down,
                  IsKeyDown(KEY_DOWN));
    UPDATE_BUTTON(&curr_input->buttons.camera_forward,
                  IsKeyDown(KEY_W));
    UPDATE_BUTTON(&curr_input->buttons.camera_backward,
                  IsKeyDown(KEY_S) && !IsKeyDown(KEY_LEFT_SHIFT));

    /* Exports (Shift+key to avoid conflicts with WASD) */
    UPDATE_BUTTON(&curr_input->buttons.export_ppm, IsKeyDown(KEY_P));
    UPDATE_BUTTON(&curr_input->buttons.export_anaglyph,
                  IsKeyDown(KEY_A) && IsKeyDown(KEY_LEFT_SHIFT));
    UPDATE_BUTTON(&curr_input->buttons.export_sidebyside,
                  IsKeyDown(KEY_S) && IsKeyDown(KEY_LEFT_SHIFT));
    UPDATE_BUTTON(&curr_input->buttons.export_stereogram,
                  IsKeyDown(KEY_G) && !IsKeyDown(KEY_LEFT_SHIFT));
    UPDATE_BUTTON(&curr_input->buttons.export_stereogram_cross,
                  IsKeyDown(KEY_G) && IsKeyDown(KEY_LEFT_SHIFT));
    UPDATE_BUTTON(&curr_input->buttons.export_glsl,
                  IsKeyDown(KEY_L) && IsKeyDown(KEY_LEFT_SHIFT));

    /* Toggles */
    UPDATE_BUTTON(&curr_input->buttons.toggle_voxels, IsKeyDown(KEY_V));
    UPDATE_BUTTON(&curr_input->buttons.toggle_floor, IsKeyDown(KEY_F));
    UPDATE_BUTTON(&curr_input->buttons.toggle_boxes, IsKeyDown(KEY_B));
    UPDATE_BUTTON(&curr_input->buttons.toggle_meshes, IsKeyDown(KEY_M));
    UPDATE_BUTTON(&curr_input->buttons.toggle_reflections, IsKeyDown(KEY_R));
    UPDATE_BUTTON(&curr_input->buttons.toggle_refractions, IsKeyDown(KEY_T));
    UPDATE_BUTTON(&curr_input->buttons.toggle_shadows, IsKeyDown(KEY_H));
    UPDATE_BUTTON(&curr_input->buttons.toggle_envmap, IsKeyDown(KEY_E));
    UPDATE_BUTTON(&curr_input->buttons.toggle_aa, IsKeyDown(KEY_X));
    UPDATE_BUTTON(&curr_input->buttons.cycle_envmap_mode, IsKeyDown(KEY_C));
    UPDATE_BUTTON(&curr_input->buttons.scale_cycle, IsKeyDown(KEY_TAB));
    UPDATE_BUTTON(&curr_input->buttons.cycle_render_mode, IsKeyDown(KEY_N));
    UPDATE_BUTTON(&curr_input->buttons.toggle_hud, IsKeyDown(KEY_F1));

    /* ── Mouse ─────────────────────────────────────────────────────────── */
    Vector2 mpos = GetMousePosition();
    curr_input->mouse.x  = mpos.x;
    curr_input->mouse.y  = mpos.y;
    curr_input->mouse.dx = mpos.x - prev_mouse_x;
    curr_input->mouse.dy = mpos.y - prev_mouse_y;
    prev_mouse_x = mpos.x;
    prev_mouse_y = mpos.y;

    curr_input->mouse.scroll     = GetMouseWheelMove();
    curr_input->mouse.left_down  = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    curr_input->mouse.right_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    curr_input->mouse.middle_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

    if (curr_input->buttons.quit.ended_down) break;

    float dt = GetFrameTime();
    elapsed_time += dt;
    game_update(&state, curr_input, dt);
    game_render(&state, &props.backbuffer, props.world_config);

    /* Display: GPU mode renders via shader, then overlays HUD.
     * CPU modes blit the backbuffer as before. */
    if (state.settings.render_mode == RENDER_GPU && g_rl_gpu.ready) {
      rl_gpu_render(elapsed_time, curr_input->mouse.x, curr_input->mouse.y,
                    &state.camera, &state.scene, &state.settings);
      rl_gpu_overlay_hud(&props.backbuffer);
    } else {
      display_backbuffer(&props.backbuffer);
    }
  }

  rl_gpu_shutdown();
  if (IsTextureValid(g_texture)) UnloadTexture(g_texture);
  platform_game_props_free(&props);
  CloseWindow();
  return 0;
}
