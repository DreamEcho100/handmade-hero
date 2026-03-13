# Lesson 14 — Anaglyph + Side-by-Side Stereo

> **What you'll build:** By the end of this lesson, pressing **A** exports an anaglyph stereo image (viewable with red/cyan 3D glasses), and pressing **S** exports a side-by-side stereoscopic image (viewable with Google Cardboard or cross-eyed free-viewing).

## Observable outcome

Two new PPM export modes: `anaglyph.ppm` shows the scene in red/cyan 3D (left eye = red channel, right eye = cyan channels). `sidebyside.ppm` places left and right eye views next to each other at double width. Both produce genuine stereoscopic depth — objects at different distances shift by different amounts between the two eye views.

## New concepts

- Binocular parallax — the depth cue from each eye seeing a slightly different angle of the same scene
- Eye separation — human eyes are ~6.5 cm apart; shifting the camera left/right simulates each eye
- Asymmetric frustum correction — the "render wider, then crop" trick to compensate for off-axis cameras
- Anaglyph merge — left eye luminance in the red channel, right eye luminance in green+blue (cyan)
- Side-by-side stereoscope — full-color left and right images placed side by side

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/stereo.h` | Created | `EYE_SEPARATION` constant; `render_anaglyph` and `render_side_by_side` declarations |
| `game/stereo.c` | Created | Eye camera offset; wider-then-crop rendering; anaglyph merge; side-by-side layout |
| `game/main.c` | Modified | A key calls `render_anaglyph`; S key calls `render_side_by_side` |
| `game/base.h` | Modified | `export_anaglyph` and `export_sidebyside` buttons added to `GameInput` |

## Background — why this works

### JS analogy

In JavaScript you might use the WebXR API or Three.js `StereoEffect` to render stereo pairs. The browser handles eye offsets and frustum correction internally. In C we do it all manually: shift the camera, re-render the full scene, and merge the two images pixel by pixel.

### Parallax and depth perception (ssloy Part 2)

Human eyes are approximately 6.5 cm apart. Each eye sees a slightly different angle of the same scene. The brain fuses these two images into a single perception of depth.

Three types of parallax exist:
- **Zero parallax:** object at the screen plane — both eyes see the same pixel position
- **Positive parallax:** object behind the screen — pixels shift apart (diverge)
- **Negative parallax:** object in front of the screen — pixels shift toward each other (converge)

The amount of shift (parallax) is proportional to the object's distance from the screen plane. Close objects shift more; distant objects shift less. This is why stereo 3D works.

### The "render wider, then crop" trick

Each eye is offset from center, making its view frustum through the screen asymmetric. Our raytracer only supports symmetric frustums (the standard NDC-to-ray formula assumes the camera is centered on the image plane).

ssloy's solution: render a **wider** image than needed, then **crop** out the center portion. The extra width on each side compensates for the parallax shift caused by the eye offset. In practice, a 10% oversize works well for typical viewing distances:

```
     ┌─── wider render ───┐
     │   ┌─ crop ─┐       │
     │   │        │       │
     │   │ output │       │
     │   │        │       │
     │   └────────┘       │
     └────────────────────┘
```

### Anaglyph color encoding

Red/cyan 3D glasses work by color filtering:
- The **red lens** blocks cyan (green + blue) light — only the red channel reaches the left eye
- The **cyan lens** blocks red light — only the green + blue channels reach the right eye

So we convert each eye's image to grayscale (luminance), then place left-eye luminance in the red channel and right-eye luminance in both green and blue channels. The brain fuses the two luminance images into depth, sacrificing color for stereoscopic perception.

## Code walkthrough

### `game/stereo.h` — complete file

```c
#ifndef GAME_STEREO_H
#define GAME_STEREO_H

#include "scene.h"
#include "render.h"
#include <stdint.h>

#define EYE_SEPARATION 0.065f

void render_anaglyph(int width, int height,
                     const Scene *scene, const RtCamera *cam,
                     const char *filename, const RenderSettings *settings);

void render_side_by_side(int width, int height,
                         const Scene *scene, const RtCamera *cam,
                         const char *filename, const RenderSettings *settings);

#endif /* GAME_STEREO_H */
```

**Key lines:**
- Line 8: `EYE_SEPARATION 0.065f` — 6.5 cm in world units. Human interpupillary distance averages 6.5 cm.

### `game/stereo.c` — complete file

```c
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
 * then crop the center (width x height) portion.                          */
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

  /* Crop: extract the center (width x height) from the wider image. */
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

  /* Merge: left eye -> red channel, right eye -> cyan (green+blue).
   * Red/cyan 3D glasses: red lens blocks cyan, cyan lens blocks red.
   * Each eye receives only its intended image -> brain fuses into depth.   */
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
```

**Key lines:**

- **Eye offset computation (lines 62-64):** The camera's right vector is computed from the forward direction crossed with world up. Each eye is offset by half `EYE_SEPARATION` along this right vector.

- **Camera copy (lines 67-70):** `RtCamera left_cam = *cam` performs a value copy of the entire camera struct. We only change `position` — the target, FOV, and orientation remain the same so both eyes look at the same point.

- **Wider render + crop (lines 22-24):** `extra = width * 0.10` adds 10% more pixels. `crop_x = extra / 2` offsets the crop to center it. This corrects the asymmetric frustum without modifying the ray generation formula.

- **Luminance formula (lines 82-85):** `0.299*R + 0.587*G + 0.114*B` is the ITU-R BT.601 luminance formula — the standard perceptual brightness weighting used in TV and image processing. Green contributes most because human eyes are most sensitive to green light.

- **Side-by-side layout (lines 128-139):** Left image occupies columns `0..width-1`, right image occupies columns `width..2*width-1`. The output stride is `out_width = width * 2`.

### Memory management

Both functions allocate three buffers (`left_buf`, `right_buf`, `output`) and free all three on every exit path. The guard at lines 55-57 handles partial allocation failure — if any `malloc` fails, free whatever was allocated and return. This is the standard C pattern for multi-buffer cleanup without `goto`.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| No depth effect — image looks flat | `EYE_SEPARATION` is 0 or eyes are offset along the wrong axis | Verify `eye_offset` uses the camera's right vector, not forward or up |
| Anaglyph colors are inverted (cyan left, red right) | Left/right eye channels swapped | Left eye = red channel (index 0), right eye = green+blue (indices 1+2) |
| Image has vertical seam in the middle | Side-by-side `out_width` not doubled | Set `out_width = width * 2`; use `out_width` (not `width`) for row stride |
| Stereo effect is too strong / headache-inducing | `EYE_SEPARATION` too large | Use 0.065 (6.5 cm). Larger values exaggerate depth unnaturally |
| Scene shifts left/right but no depth | Both eyes render the same position | Verify `left_cam.position = vec3_sub(...)` and `right_cam.position = vec3_add(...)` |
| Black image / crash | `malloc` returned NULL for large buffers | Check pixel_count overflow; ensure `sizeof(Vec3) * pixel_count` fits in `size_t` |

## Exercise

> Modify `render_anaglyph` to produce a **full-color anaglyph** instead of luminance-only. Use the left eye's red channel directly and the right eye's green and blue channels directly (no luminance conversion). Compare the result with the grayscale version — which has better depth, and which has better color?

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `new THREE.StereoCamera()` | Manual eye offset: `vec3_sub(cam->position, eye_offset)` | No library; compute eye positions from camera right vector |
| `renderer.setSize(w, h)` + WebXR | `render_scene_to_float(buf, wide_w, height, ...)` + crop | No GPU resize; render wider and memcpy the center |
| `canvas.toDataURL('image/png')` | `ppm_export_rgb(output, width, height, filename)` | PPM is raw bytes; no compression, no encoder |
| `ImageData(new Uint8ClampedArray(...))` | `uint8_t *output = malloc(pixel_count * 3)` | Manual allocation; must free when done |
| `0.299 * r + 0.587 * g + 0.114 * b` | Same formula, with `f` suffix for float | ITU-R BT.601 luminance is universal |
