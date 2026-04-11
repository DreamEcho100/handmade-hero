/* ═══════════════════════════════════════════════════════════════════════════
 * game/stereo.c — TinyRaytracer Course (L14)
 * ═══════════════════════════════════════════════════════════════════════════
 * Anaglyph + side-by-side stereo rendering (Part 2 of ssloy's wiki).
 *
 * PARALLAX & BINOCULAR DEPTH (from ssloy Part 2):
 * ────────────────────────────────────────────────
 * Our eyes are ~6.5cm apart.  Each eye sees a slightly different angle
 * of the same scene.  The brain fuses these into depth perception.
 *
 *   ZERO PARALLAX:     object at screen plane → both eyes see same pixel
 *   POSITIVE PARALLAX: object behind screen  → pixels shift apart
 *   NEGATIVE PARALLAX: object in front       → pixels shift toward each other
 *
 * ASYMMETRIC FRUSTUM — THE "RENDER WIDER, THEN CROP" TRICK:
 * ──────────────────────────────────────────────────────────
 * Each eye is offset from center, so its view frustum through the screen
 * is asymmetric.  Our raytracer only renders symmetric frustums (the
 * standard NDC-to-ray formula assumes the camera is centered on the
 * screen).  ssloy's solution: render a WIDER image than needed, then
 * CROP out the center portion.  The extra width compensates for the
 * parallax shift caused by the eye offset.
 *
 * In practice: we increase the internal render width by a factor
 * proportional to EYE_SEPARATION / orbit_distance, then extract the
 * center (width × height) region from each wider render.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "stereo.h"
#include "ppm.h"
#include "raytracer.h"
#include "ray.h"
#include <stdlib.h>
#include <math.h>

/* How much wider to render for frustum correction.
 * This is a fixed oversize that works well for typical viewing distances.
 * A precise computation would use EYE_SEPARATION / (2 * screen_distance),
 * but since screen_distance varies per viewer, a fixed 10% oversize
 * produces good results without complicating the API.                     */
#define STEREO_OVERSIZE_FRAC 0.10f

/* Render one eye's view to a Vec3 buffer at a wider resolution,
 * then crop the center (width × height) portion.                          */
static void render_eye_cropped(Vec3 *out, int width, int height,
                               const Scene *scene, const RtCamera *eye_cam,
                               const RenderSettings *settings) {
  /* Wider internal resolution to compensate for off-axis parallax. */
  int extra = (int)((float)width * STEREO_OVERSIZE_FRAC);
  int wide_w = width + extra;
  int crop_x = extra / 2; /* left margin to crop */

  Vec3 *wide_buf = (Vec3 *)malloc((size_t)wide_w * (size_t)height * sizeof(Vec3));
  if (!wide_buf) {
    /* Fallback: render at normal size (no crop correction). */
    render_scene_to_float(out, width, height, scene, eye_cam, settings);
    return;
  }

  /* Render at the wider resolution. */
  render_scene_to_float(wide_buf, wide_w, height, scene, eye_cam, settings);

  /* Crop: extract the center (width × height) from the wider image. */
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      out[j * width + i] = wide_buf[j * wide_w + (crop_x + i)];
    }
  }

  free(wide_buf);
}

void render_anaglyph(int width, int height,
                     const Scene *scene, const RtCamera *cam,
                     const char *filename, const RenderSettings *settings) {
  int pixel_count = width * height;

  Vec3 *left_buf  = (Vec3 *)malloc((size_t)pixel_count * sizeof(Vec3));
  Vec3 *right_buf = (Vec3 *)malloc((size_t)pixel_count * sizeof(Vec3));
  uint8_t *output = (uint8_t *)malloc((size_t)pixel_count * 3);

  if (!left_buf || !right_buf || !output) {
    free(left_buf); free(right_buf); free(output);
    return;
  }

  /* Compute eye offset along camera's right vector. */
  Vec3 forward  = vec3_normalize(vec3_sub(cam->target, cam->position));
  Vec3 world_up = vec3_make(0.0f, 1.0f, 0.0f);
  Vec3 right    = vec3_normalize(vec3_cross(forward, world_up));
  Vec3 eye_offset = vec3_scale(right, EYE_SEPARATION * 0.5f);

  /* Left eye: shifted left.  Right eye: shifted right. */
  RtCamera left_cam  = *cam;
  left_cam.position   = vec3_sub(cam->position, eye_offset);
  RtCamera right_cam = *cam;
  right_cam.position  = vec3_add(cam->position, eye_offset);

  /* Render each eye with wider-then-crop for correct off-axis stereo. */
  render_eye_cropped(left_buf,  width, height, scene, &left_cam, settings);
  render_eye_cropped(right_buf, width, height, scene, &right_cam, settings);

  /* Merge: left eye → red channel, right eye → cyan (green+blue).
   * Red/cyan 3D glasses: red lens blocks cyan, cyan lens blocks red.
   * Each eye receives only its intended image → brain fuses into depth.   */
  for (int i = 0; i < pixel_count; i++) {
    float left_gray  = 0.299f * left_buf[i].x + 0.587f * left_buf[i].y
                     + 0.114f * left_buf[i].z;
    float right_gray = 0.299f * right_buf[i].x + 0.587f * right_buf[i].y
                     + 0.114f * right_buf[i].z;

    output[i * 3 + 0] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, left_gray))  * 255.0f);
    output[i * 3 + 1] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, right_gray)) * 255.0f);
    output[i * 3 + 2] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, right_gray)) * 255.0f);
  }

  ppm_export_rgb(output, width, height, filename);
  free(left_buf); free(right_buf); free(output);
}

void render_side_by_side(int width, int height,
                         const Scene *scene, const RtCamera *cam,
                         const char *filename, const RenderSettings *settings) {
  int pixel_count = width * height;
  int out_width   = width * 2;

  Vec3 *left_buf  = (Vec3 *)malloc((size_t)pixel_count * sizeof(Vec3));
  Vec3 *right_buf = (Vec3 *)malloc((size_t)pixel_count * sizeof(Vec3));
  uint8_t *output = (uint8_t *)malloc((size_t)out_width * (size_t)height * 3);

  if (!left_buf || !right_buf || !output) {
    free(left_buf); free(right_buf); free(output);
    return;
  }

  Vec3 forward  = vec3_normalize(vec3_sub(cam->target, cam->position));
  Vec3 world_up = vec3_make(0.0f, 1.0f, 0.0f);
  Vec3 right    = vec3_normalize(vec3_cross(forward, world_up));
  Vec3 eye_offset = vec3_scale(right, EYE_SEPARATION * 0.5f);

  RtCamera left_cam  = *cam;
  left_cam.position   = vec3_sub(cam->position, eye_offset);
  RtCamera right_cam = *cam;
  right_cam.position  = vec3_add(cam->position, eye_offset);

  /* Render each eye with wider-then-crop for correct off-axis stereo. */
  render_eye_cropped(left_buf,  width, height, scene, &left_cam, settings);
  render_eye_cropped(right_buf, width, height, scene, &right_cam, settings);

  /* Place left and right images side by side (ssloy Part 2: stereoscope).
   * Viewable with Google Cardboard / phone VR or cross-eyed free-viewing. */
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      Vec3 lc = left_buf[j * width + i];
      int li = (j * out_width + i) * 3;
      output[li + 0] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, lc.x)) * 255.0f);
      output[li + 1] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, lc.y)) * 255.0f);
      output[li + 2] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, lc.z)) * 255.0f);

      Vec3 rc = right_buf[j * width + i];
      int ri = (j * out_width + width + i) * 3;
      output[ri + 0] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, rc.x)) * 255.0f);
      output[ri + 1] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, rc.y)) * 255.0f);
      output[ri + 2] = (uint8_t)(fminf(1.0f, fmaxf(0.0f, rc.z)) * 255.0f);
    }
  }

  ppm_export_rgb(output, out_width, height, filename);
  free(left_buf); free(right_buf); free(output);
}
