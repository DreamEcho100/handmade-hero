#ifndef GAME_GPU_SHADER_H
#define GAME_GPU_SHADER_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/gpu_shader.h — TinyRaytracer Course (L23)
 * ═══════════════════════════════════════════════════════════════════════════
 * Wraps the existing Shadertoy GLSL_RAYTRACER_SOURCE (from shader_glsl.h)
 * for native OpenGL 3.3 core rendering.
 *
 * DESIGN: zero code duplication — we reuse the exact same GLSL body string
 * from L21.  This header adds only the #version, uniform declarations, and
 * the output wrapper that Shadertoy normally provides for you.
 *
 * Vertex shader: fullscreen triangle via gl_VertexID (no VAO data needed).
 * Fragment shader: preamble + GLSL_RAYTRACER_SOURCE + epilogue.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "shader_glsl.h"
#include "gpu_scene_glsl.h"
#include <stdio.h>
#include <string.h>

/* ── Fullscreen triangle vertex shader ────────────────────────────────────
 * Renders a single triangle that covers the entire clip space using only
 * gl_VertexID.  No vertex buffer needed — just glDrawArrays(GL_TRIANGLES,0,3).
 *
 * Vertices: (-1,-1), (3,-1), (-1,3)
 * After clipping, this covers the entire [-1,1]×[-1,1] viewport.           */
#define GPU_VERT_SHADER                                                        \
  "#version 330 core\n"                                                        \
  "void main() {\n"                                                            \
  "    vec2 pos[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));\n"          \
  "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n"                      \
  "}\n"

/* ── Fragment shader preamble ─────────────────────────────────────────────
 * Declares the Shadertoy-compatible uniforms that the host sets each frame.
 * These replace Shadertoy's built-in globals.
 * Also declares camera uniforms for interactive orbit (iCamOrigin/Forward/
 * Right/Up) and defines GPU_CAM so mainImage() knows to use them.          */
#define GPU_FRAG_PREAMBLE                                                      \
  "#version 330 core\n"                                                        \
  "precision highp float;\n"                                                   \
  "uniform vec3  iResolution;\n"                                               \
  "uniform float iTime;\n"                                                     \
  "uniform vec4  iMouse;\n"                                                    \
  "uniform vec3  iCamOrigin;\n"                                                \
  "uniform vec3  iCamForward;\n"                                               \
  "uniform vec3  iCamRight;\n"                                                 \
  "uniform vec3  iCamUp;\n"                                                    \
  "uniform float iCamFov;\n"                                                   \
  "out vec4 FragColor;\n\n"

/* ── Fragment shader epilogue ─────────────────────────────────────────────
 * Instead of calling mainImage() (which uses a fixed camera at the origin),
 * the epilogue builds the ray direction from the camera uniforms set by
 * the host each frame.  This gives the GPU shader the same interactive
 * orbit camera as the CPU modes (WASD, arrows, mouse orbit/pan/zoom).
 *
 * mainImage() from GLSL_RAYTRACER_SOURCE is still present for Shadertoy
 * export compatibility, but it's unused in the in-process shader — the
 * GLSL compiler silently drops it as dead code.                            */
#define GPU_FRAG_EPILOGUE                                                      \
  "\nvoid main() {\n"                                                          \
  "    vec2  ndc     = (2.0 * gl_FragCoord.xy - iResolution.xy)\n"             \
  "                    / iResolution.y;\n"                                     \
  "    float cam_fov = (iCamFov > 0.0) ? tan(iCamFov / 2.0)\n"                \
  "                    : tan(1.0472 / 2.0);\n"  /* PI/3 fallback */                                    \
  "    vec3  dir     = normalize(\n"                                           \
  "        iCamRight   * (ndc.x * cam_fov) +\n"                                \
  "        iCamUp      * (ndc.y * cam_fov) +\n"                                \
  "        iCamForward);\n"                                                    \
  "    FragColor = vec4(cast_ray(iCamOrigin, dir), 1.0);\n"                    \
  "}\n"

/* Build the complete fragment shader source into a caller-provided buffer.
 * Returns 0 on success, -1 if buffer too small.                            */
static inline int gpu_build_fragment_source(char *buf, int bufsize) {
  int needed = (int)(strlen(GPU_FRAG_PREAMBLE) + sizeof(GLSL_RAYTRACER_SOURCE) -
                     1 + strlen(GPU_FRAG_EPILOGUE) + 1);
  if (bufsize < needed) {
    fprintf(stderr, "gpu_build_fragment_source: buffer too small (%d < %d)\n",
            bufsize, needed);
    return -1;
  }
  buf[0] = '\0';
  strcat(buf, GPU_FRAG_PREAMBLE);
  strcat(buf, GLSL_RAYTRACER_SOURCE);
  strcat(buf, GPU_FRAG_EPILOGUE);
  return 0;
}

/* Build the complete SCENE-DRIVEN fragment shader source (L24+).
 * Uses GPU_SCENE_GLSL_SOURCE (data-driven) instead of GLSL_RAYTRACER_SOURCE.
 * Returns 0 on success, -1 if buffer too small.                            */
static inline int gpu_build_scene_fragment_source(char *buf, int bufsize) {
  int needed = (int)(strlen(GPU_FRAG_PREAMBLE) +
                     strlen(GPU_SCENE_UNIFORMS) +
                     sizeof(GPU_SCENE_GLSL_SOURCE) - 1 +
                     strlen(GPU_FRAG_EPILOGUE) + 1);
  if (bufsize < needed) {
    fprintf(stderr,
            "gpu_build_scene_fragment_source: buffer too small (%d < %d)\n",
            bufsize, needed);
    return -1;
  }
  buf[0] = '\0';
  strcat(buf, GPU_FRAG_PREAMBLE);
  strcat(buf, GPU_SCENE_UNIFORMS);
  strcat(buf, GPU_SCENE_GLSL_SOURCE);
  strcat(buf, GPU_FRAG_EPILOGUE);
  return 0;
}

#endif /* GAME_GPU_SHADER_H */
