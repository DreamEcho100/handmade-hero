# Lesson 20 — Cube Map Textures

> **What you'll build:** By the end of this lesson, the raytracer supports cube map environment mapping — six separate face images (+X, -X, +Y, -Y, +Z, -Z) loaded via stb_image. Pressing **C** cycles between procedural sky, equirectangular, and cube map modes. The HUD shows the current mode.

## Observable outcome

If you place six cube map face images in `assets/textures/cube/Park3Med/` named `px.jpg`, `nx.jpg`, `py.jpg`, `ny.jpg`, `pz.jpg`, `nz.jpg`, the scene background shows the cube map. Reflective surfaces (mirror sphere) reflect the cube map. Pressing **C** cycles: `sky` (procedural) -> `eq` (equirectangular) -> `cube` (cube map) -> `sky` (wrapping around, skipping any mode with no loaded data). The HUD line 3 shows `C:sky`, `C:eq`, or `C:cube`.

If no cube map images exist, the loader falls back to equirectangular; if that also fails, it falls back to procedural sky. No crash either way.

## New concepts

- Cube map texture mapping — 6 square images forming the faces of a cube surrounding the scene
- Direction-to-face selection — finding which face a ray direction hits by comparing absolute axis components
- UV computation per face — dividing the non-dominant components by the major axis to get [-1, 1] coordinates
- OpenGL cube map convention — the standard face layout and UV orientation used by GPU APIs
- Multi-mode environment map — runtime switching between procedural, equirectangular, and cube map
- Graceful fallback chain — try cube map -> try equirect -> use procedural (no crash on missing files)

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/envmap.h` | Modified | `EnvMapMode` enum, `CubeMapFace` struct, `CUBEMAP_FACE_*` defines, `envmap_load_cubemap` declaration |
| `game/envmap.c` | Modified | `envmap_load_cubemap`, `sample_cubemap`, `sample_face` functions; `envmap_sample` dispatches on mode |
| `game/scene.h` | Modified | `scene_init` tries cube map first, falls back to equirect, then procedural |
| `game/main.c` | Modified | **C** key cycles envmap mode; HUD shows current mode |
| `game/base.h` | Modified | `cycle_envmap_mode` button added |

## Background — why this works

### JS analogy

In Three.js you load a cube map with:

```js
const loader = new THREE.CubeTextureLoader();
scene.background = loader.load([
  'px.jpg', 'nx.jpg', 'py.jpg', 'ny.jpg', 'pz.jpg', 'nz.jpg'
]);
```

In WebGL, you create a `GL_TEXTURE_CUBE_MAP` and upload 6 images to its faces, then sample with `textureCube(sampler, direction)`. The GPU hardware handles face selection and UV mapping automatically.

In our C raytracer, we do the same thing manually: given a ray direction, we determine which cube face it hits, compute the UV coordinates on that face, and look up the pixel. No GPU, no hardware — just comparisons and a division.

### Cube map vs equirectangular

**Equirectangular** (L18) maps the entire sphere to a single 2:1 image using `atan2` and `asin`. It is simple but has two drawbacks:
1. **Polar distortion** — pixels near the top and bottom of the image are stretched, wasting resolution
2. **Trig cost** — `atan2f` + `asinf` per ray direction (relatively expensive)

**Cube map** uses 6 square images. Each face covers 90 degrees of the sphere with uniform pixel density. The lookup uses only comparisons + one division (no trig). This is why GPUs use cube maps natively.

```
         ┌──────┐
         │  +Y  │  (up)
         │      │
    ┌────┼──────┼────┬──────┐
    │ -X │  +Z  │ +X │  -Z  │
    │left│front │right│ back │
    └────┼──────┼────┴──────┘
         │  -Y  │  (down)
         │      │
         └──────┘
```

### Direction-to-face algorithm

Given a normalized direction `(dx, dy, dz)`:

1. **Find the dominant axis** — the axis with the largest absolute value:
   - If `|dx| >= |dy|` and `|dx| >= |dz|` → X-axis face
   - If `|dy| >= |dx|` and `|dy| >= |dz|` → Y-axis face
   - Otherwise → Z-axis face

2. **Pick the sign** — positive component = positive face, negative = negative face

3. **Compute UV** — divide the other two components by the major axis absolute value. This projects the direction onto the face plane, giving values in [-1, 1].

The UV formulas per face (OpenGL convention):

| Face | u | v |
|------|---|---|
| +X (right) | `-dz / |dx|` | `-dy / |dx|` |
| -X (left) | `dz / |dx|` | `-dy / |dx|` |
| +Y (up) | `dx / |dy|` | `dz / |dy|` |
| -Y (down) | `dx / |dy|` | `-dz / |dy|` |
| +Z (front) | `dx / |dz|` | `-dy / |dz|` |
| -Z (back) | `-dx / |dz|` | `-dy / |dz|` |

The sign flips ensure the UV orientation matches the OpenGL/Shadertoy convention. The results are in [-1, 1] and get remapped to [0, 1] for pixel lookup.

## Code walkthrough

### `game/envmap.h` — new types (L20 additions)

```c
/* ── Cube map face indices ─────────────────────────────────────────────── */
#define CUBEMAP_FACE_POS_X  0  /* right  */
#define CUBEMAP_FACE_NEG_X  1  /* left   */
#define CUBEMAP_FACE_POS_Y  2  /* up     */
#define CUBEMAP_FACE_NEG_Y  3  /* down   */
#define CUBEMAP_FACE_POS_Z  4  /* front  */
#define CUBEMAP_FACE_NEG_Z  5  /* back   */
#define CUBEMAP_NUM_FACES   6

/* ── Environment map modes ─────────────────────────────────────────────── */
typedef enum {
  ENVMAP_PROCEDURAL  = 0,  /* no image, use procedural sky gradient      */
  ENVMAP_EQUIRECT    = 1,  /* single equirectangular panorama (L18)      */
  ENVMAP_CUBEMAP     = 2,  /* 6 face images (L20)                        */
} EnvMapMode;

/* ── Per-face image data ───────────────────────────────────────────────── */
typedef struct {
  unsigned char *pixels;   /* NULL if face not loaded                     */
  int width, height;
  int channels;            /* 3 (RGB) or 4 (RGBA)                        */
} CubeMapFace;

/* ── Environment map ───────────────────────────────────────────────────── */
typedef struct {
  EnvMapMode mode;

  /* Equirectangular data (L18). */
  unsigned char *pixels;
  int width, height;
  int channels;

  /* Cube map data (L20). */
  CubeMapFace faces[CUBEMAP_NUM_FACES];
} EnvMap;
```

**Key design:** The `EnvMap` struct holds data for both modes simultaneously. This allows runtime switching with the **C** key without reloading. The `mode` field controls which data `envmap_sample` reads.

### `game/envmap.c` — `envmap_load_cubemap` (complete)

```c
int envmap_load_cubemap(EnvMap *e, const char *face_files[CUBEMAP_NUM_FACES]) {
  static const char *face_names[CUBEMAP_NUM_FACES] = {
    "+X (right)", "-X (left)", "+Y (up)", "-Y (down)", "+Z (front)", "-Z (back)"
  };

  for (int i = 0; i < CUBEMAP_NUM_FACES; i++) {
    int w, h, ch;
    unsigned char *data = stbi_load(face_files[i], &w, &h, &ch, 3);
    if (!data) {
      fprintf(stderr, "envmap_load_cubemap: cannot open '%s' [face %s] — "
              "falling back to procedural sky\n", face_files[i], face_names[i]);
      /* Free any already-loaded faces. */
      for (int j = 0; j < i; j++) {
        if (e->faces[j].pixels) {
          stbi_image_free(e->faces[j].pixels);
          e->faces[j].pixels = NULL;
        }
      }
      envmap_init_procedural(e);
      return -1;
    }
    e->faces[i].pixels   = data;
    e->faces[i].width    = w;
    e->faces[i].height   = h;
    e->faces[i].channels = 3;
    printf("Loaded cubemap face %s: %s (%dx%d)\n",
           face_names[i], face_files[i], w, h);
  }
  e->mode = ENVMAP_CUBEMAP;
  printf("Cube map loaded successfully (6 faces)\n");
  return 0;
}
```

**Key lines:**
- **Lines 2-4:** `face_names` array for diagnostic output. Matching order to `CUBEMAP_FACE_*` defines.
- **Line 7:** `stbi_load(face_files[i], &w, &h, &ch, 3)` — forces 3-channel RGB, same as equirectangular loading.
- **Lines 11-16:** If any face fails to load, free all previously loaded faces and fall back to procedural sky. This prevents a partial cube map (which would show black on missing faces).
- **Line 21:** `e->faces[i].channels = 3` — always 3 because we force RGB via the `3` argument to `stbi_load`.

### `game/envmap.c` — `sample_face` (UV remapping + pixel lookup)

```c
static Vec3 sample_face(const CubeMapFace *face, float u, float v) {
  if (!face->pixels)
    return vec3_make(0.0f, 0.0f, 0.0f);

  /* Remap [-1,1] → [0,1]. */
  u = 0.5f * (u + 1.0f);
  v = 0.5f * (v + 1.0f);

  int px = (int)(u * (float)face->width) % face->width;
  int py = (int)(v * (float)face->height) % face->height;
  if (px < 0)
    px += face->width;
  if (py < 0)
    py += face->height;

  int idx = (py * face->width + px) * face->channels;
  return vec3_make((float)face->pixels[idx + 0] / 255.0f,
                   (float)face->pixels[idx + 1] / 255.0f,
                   (float)face->pixels[idx + 2] / 255.0f);
}
```

**Key lines:**
- **Line 6:** `0.5f * (u + 1.0f)` — remaps from [-1, 1] to [0, 1]. This is the standard cube map UV convention.
- **Lines 11-13:** Same negative-modulo fix as equirectangular (L18). C's `%` can return negative values.
- **Lines 16-18:** Byte-to-float conversion, identical to the equirectangular sampler.

### `game/envmap.c` — `sample_cubemap` (direction-to-face + UV, complete)

```c
static Vec3 sample_cubemap(const EnvMap *e, Vec3 d) {
  float ax = fabsf(d.x), ay = fabsf(d.y), az = fabsf(d.z);
  int face_idx;
  float u, v, ma; /* ma = major axis absolute value */

  if (ax >= ay && ax >= az) {
    /* X is dominant. */
    ma = ax;
    if (d.x > 0.0f) {
      face_idx = CUBEMAP_FACE_POS_X;
      u = -d.z / ma;
      v = -d.y / ma;
    } else {
      face_idx = CUBEMAP_FACE_NEG_X;
      u =  d.z / ma;
      v = -d.y / ma;
    }
  } else if (ay >= ax && ay >= az) {
    /* Y is dominant. */
    ma = ay;
    if (d.y > 0.0f) {
      face_idx = CUBEMAP_FACE_POS_Y;
      u =  d.x / ma;
      v =  d.z / ma;
    } else {
      face_idx = CUBEMAP_FACE_NEG_Y;
      u =  d.x / ma;
      v = -d.z / ma;
    }
  } else {
    /* Z is dominant. */
    ma = az;
    if (d.z > 0.0f) {
      face_idx = CUBEMAP_FACE_POS_Z;
      u =  d.x / ma;
      v = -d.y / ma;
    } else {
      face_idx = CUBEMAP_FACE_NEG_Z;
      u = -d.x / ma;
      v = -d.y / ma;
    }
  }

  return sample_face(&e->faces[face_idx], u, v);
}
```

**Key lines:**
- **Line 2:** `fabsf(d.x)` — absolute values precomputed once. Three comparisons determine the dominant axis.
- **Line 8:** `ma = ax` — the major axis absolute value, used as the divisor for UV projection. Dividing by `ma` projects the direction onto the unit cube face.
- **Lines 11-12:** +X face: `u = -d.z / ma, v = -d.y / ma`. The negative sign on `dz` matches the OpenGL convention where looking in the +X direction, -Z is to the right.
- **Lines 15-16:** -X face: `u = dz / ma` — the sign flips because the face is mirrored relative to +X.
- **Lines 23-24:** +Y face: `u = dx, v = dz` — the Y-dominant faces use X and Z for UV (the horizontal plane).

### `game/envmap.c` — `envmap_sample` (dispatch)

```c
Vec3 envmap_sample(const EnvMap *e, Vec3 direction) {
  switch (e->mode) {
    case ENVMAP_EQUIRECT:
      return sample_equirect(e, direction);
    case ENVMAP_CUBEMAP:
      return sample_cubemap(e, direction);
    case ENVMAP_PROCEDURAL:
    default:
      return procedural_sky(direction);
  }
}
```

**Key design:** One function, three code paths. The caller (`cast_ray` in `raytracer.c`) never needs to know which mode is active — it just calls `envmap_sample(envmap, direction)`.

### `game/scene.h` — fallback chain in `scene_init`

```c
  /* ── Environment map (L18 + L20) ─────────────────────────────────────
   * Try loading cube map first (6 images); if not found, try equirectangular;
   * fall back to procedural sky if neither exists.                          */
  {
    const char *cubemap_faces[CUBEMAP_NUM_FACES] = {
      "assets/textures/cube/Park3Med/px.jpg",  /* +X right  */
      "assets/textures/cube/Park3Med/nx.jpg",  /* -X left   */
      "assets/textures/cube/Park3Med/py.jpg",  /* +Y up     */
      "assets/textures/cube/Park3Med/ny.jpg",  /* -Y down   */
      "assets/textures/cube/Park3Med/pz.jpg",  /* +Z front  */
      "assets/textures/cube/Park3Med/nz.jpg",  /* -Z back   */
    };
    if (envmap_load_cubemap(&s->envmap, cubemap_faces) != 0) {
      /* Cube map not found — try equirectangular. */
      envmap_load(&s->envmap, "assets/envmap.jpg");
    }
  }
```

**Key design:** The braces create a local scope so the `cubemap_faces` array doesn't pollute the function. If `envmap_load_cubemap` returns -1 (any face missing), it falls through to `envmap_load` which itself falls back to procedural on failure.

### `game/main.c` — envmap mode cycling

```c
  /* Cycle envmap mode: procedural → equirect → cubemap → procedural (L20).
   * Only cycles to modes with loaded data.                                  */
  if (button_just_pressed(input->buttons.cycle_envmap_mode)) {
    EnvMap *em = &state->scene.envmap;
    int next = ((int)em->mode + 1) % 3;
    /* Skip equirect if no equirect data loaded. */
    if (next == ENVMAP_EQUIRECT && !em->pixels)
      next = (next + 1) % 3;
    /* Skip cubemap if no cubemap data loaded. */
    if (next == ENVMAP_CUBEMAP && !em->faces[0].pixels)
      next = (next + 1) % 3;
    em->mode = (EnvMapMode)next;
  }
```

**Key lines:**
- **Line 5:** `((int)em->mode + 1) % 3` — simple modular cycling through the three modes.
- **Lines 7-10:** Skip modes that have no data. If only procedural is available (no images on disk), pressing C does nothing visible. If only equirect is loaded, it cycles between procedural and equirect.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Cube map shows seams between faces | Face images have different resolutions | Use 6 square images of the same size (e.g., all 1024x1024) |
| One face is black | That face image failed to load | Check filename matches exactly: `px.jpg`, `nx.jpg`, etc. in correct directory |
| Background is mirrored/flipped | UV sign convention wrong | Use the OpenGL convention shown in the UV table above |
| Cube map looks rotated 90 degrees | Face images in wrong order | Order must be +X, -X, +Y, -Y, +Z, -Z (not front/back/left/right) |
| C key does nothing | No alternative modes have loaded data | Place image files in `assets/` and restart |
| All faces load but result looks wrong | Faces assigned to wrong slots | Verify: px=right, nx=left, py=up, ny=down, pz=front, nz=back |
| Performance worse than equirect | Face images are very large (e.g., 4096x4096 each) | Use 512x512 or 1024x1024 faces; cube map lookup itself is faster than equirect |
| Crash on startup | `stbi_load` returned NULL but code proceeds | Check that `envmap_load_cubemap` properly falls back on failure (it does by default) |

## Exercise

> Download a free cube map sky texture (search for "free cubemap skybox images"). Split it into 6 face images named `px.jpg`, `nx.jpg`, `py.jpg`, `ny.jpg`, `pz.jpg`, `nz.jpg`. Place them in `assets/textures/cube/Park3Med/` (or update the paths in `scene.h`). Run the raytracer and press **C** to switch between modes. Verify the reflections on the mirror sphere match the background. Compare the visual quality at the poles vs the equirectangular projection.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `new THREE.CubeTextureLoader().load([6 urls])` | `envmap_load_cubemap(e, face_files)` — 6 `stbi_load` calls | Synchronous; no callbacks; no GPU upload |
| `GL_TEXTURE_CUBE_MAP` + `gl.texImage2D(face, ...)` | `e->faces[i].pixels` — raw byte arrays per face | No GPU texture object; just RAM arrays |
| `textureCube(sampler, direction)` in GLSL | `sample_cubemap(e, direction)` — manual face selection + UV | GPU does face selection in hardware; we do it with comparisons |
| `scene.background = cubeTexture` | `e->mode = ENVMAP_CUBEMAP` — mode switch | Same effect; our dispatch is in `envmap_sample` |
| Automatic mipmap filtering on GPU | Nearest-neighbor lookup | We sample the exact pixel; no bilinear/trilinear filtering |
| `CubeCamera` for dynamic reflections | Reflections use `envmap_sample` via recursive `cast_ray` | We trace real reflection rays; GPU cube maps bake a static snapshot |
