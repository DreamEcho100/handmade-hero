# Lesson 11 — Interactive Camera + Mouse + Multi-threading

> **What you'll build:** By the end of this lesson, you can orbit the camera around the scene with mouse drag, pan with right-click, zoom with scroll wheel, and use arrow keys or WASD. Rendering is parallelized across CPU cores using pthreads, making the frame rate viable for interactive use.

## Observable outcome

Dragging the mouse (left button) orbits the camera around the scene. Right-click drag pans the camera target. Scroll wheel zooms in/out. Arrow keys and WASD also control the camera (arrows orbit, W/S zoom). The scene renders noticeably faster than before because all CPU cores are utilized. The camera orbits around a target point (the center of the sphere cluster), and pitch is clamped to prevent flipping upside down.

## New concepts

- `RtCamera` struct with `position`, `target`, `yaw`, `pitch`, `fov`, `orbit_radius` — an orbit camera that circles a look-at target
- `MouseState` in `GameInput` — tracks mouse position, delta, button state, and scroll wheel
- `CameraBasis` struct — pre-computed `forward`/`right`/`up` vectors computed ONCE per frame (not per pixel)
- pthreads multi-threaded rendering — rows split across CPU cores, `pthread_create`/`pthread_join`

## Files changed

| File                      | Change type | Summary                                                                                                                                        |
| ------------------------- | ----------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `game/render.h`           | Modified    | `RtCamera` struct replaces simple camera; `CameraBasis` struct; mouse/zoom constants; `MAX_RENDER_THREADS`; `camera_compute_basis` declaration |
| `game/render.c`           | Modified    | `camera_init`, `camera_update` (mouse+keyboard), `camera_compute_basis`, multi-threaded `render_scene` with `RenderJob` + `render_thread_fn`   |
| `game/base.h`             | Modified    | Add `MouseState` struct; add `camera_forward`/`camera_backward` buttons; expand button array                                                   |
| `game/main.c`             | Modified    | Call `camera_update` with input; pass camera to `render_scene`                                                                                 |
| `platforms/x11/main.c`    | Modified    | Mouse button/motion/scroll event handling; WASD key bindings                                                                                   |
| `platforms/raylib/main.c` | Modified    | Same mouse/key additions                                                                                                                       |
| `build-dev.sh`            | Modified    | Add `-lpthread` to linker flags for X11 backend; `--coord-mode=explicit` already set from L01 (no change needed here)                         |

## Background — why this works

### JS analogy

This camera system is directly inspired by Three.js `OrbitControls`: left-drag to orbit, right-drag to pan, scroll to zoom. The difference is that Three.js delegates rendering to the GPU and `OrbitControls` just updates a camera matrix. In our CPU raytracer, we must manually compute the camera basis vectors and generate a ray direction for every pixel, and we need multi-threading to keep frame rates interactive.

### The orbit camera model

The camera orbits around a **target** point at a fixed **radius**. Two angles define the position on the orbit sphere:

- **yaw**: rotation around the Y axis (horizontal orbit)
- **pitch**: rotation above/below the horizontal plane (vertical orbit)

The camera position is computed from these parameters:

```c
cam->position = vec3_add(cam->target, vec3_make(
  radius * sinf(yaw) * cosf(pitch),   /* x */
  radius * sinf(pitch),                /* y */
  radius * cosf(yaw) * cosf(pitch)    /* z */
));
```

This places the camera on a sphere of the given radius centered at the target. Pitch is clamped to `[-0.44*PI, +0.44*PI]` to prevent the camera from going directly overhead (which causes the up-vector to degenerate).

### MouseState

The `MouseState` struct tracks everything the camera needs from the mouse:

```c
typedef struct {
  float x, y;           /* current pixel position                        */
  float dx, dy;         /* delta since last frame (for drag)             */
  float scroll;         /* wheel scroll delta (+ = zoom in, - = out)    */
  int   left_down;      /* left button held = orbit                      */
  int   right_down;     /* right button held = pan                       */
  int   middle_down;    /* middle button held = pan (alt)                */
} MouseState;
```

The platform backend accumulates `dx`/`dy` from `MotionNotify` events each frame, then `camera_update` consumes them.

### CameraBasis — compute once, use everywhere

Before this lesson, the camera's `forward`, `right`, and `up` vectors were implicitly recomputed for every pixel (inside the ray direction calculation). With 800x600 = 480,000 pixels, that is 480,000 redundant `normalize` + `cross` calls.

The fix is to hoist the computation into a `CameraBasis` struct computed once per frame:

```c
typedef struct {
  Vec3  origin;
  Vec3  forward;
  Vec3  right;
  Vec3  up;
  float half_fov;
  float aspect;
} CameraBasis;
```

Each pixel then uses the pre-computed basis to generate its ray direction:

```c
float x =  (2.0f * fpx / width  - 1.0f) * basis->half_fov * basis->aspect;
float y = -(2.0f * fpy / height - 1.0f) * basis->half_fov;

Vec3 dir = vec3_normalize(vec3_add(
  vec3_add(vec3_scale(basis->right, x), vec3_scale(basis->up, y)),
  basis->forward
));
```

This is the same NDC-to-ray formula from L03, but now using pre-computed vectors instead of recomputing them per pixel.

### Multi-threaded rendering with pthreads

Raytracing is **embarrassingly parallel** — each pixel is independent, with no shared mutable state. This makes it ideal for splitting across CPU cores.

The approach:

1. Detect CPU core count: `sysconf(_SC_NPROCESSORS_ONLN)`
2. Split rows evenly across threads
3. Each thread renders its row range into the shared pixel buffer
4. Main thread waits for all threads to finish

```c
typedef struct {
  int start_row, end_row;    /* row range for this thread */
  int render_w, render_h;    /* internal resolution       */
  int stride;                /* backbuffer stride         */
  uint32_t *pixels;          /* shared backbuffer         */
  const CameraBasis *basis;  /* read-only camera data     */
  const Scene *scene;        /* read-only scene data      */
} RenderJob;
```

No mutex or synchronization is needed because each thread writes to non-overlapping rows. The `CameraBasis` and `Scene` are read-only — no data races.

## Code walkthrough

### `game/render.h` — RtCamera and CameraBasis

```c
#define CAMERA_ORBIT_SPEED   1.5f
#define MOUSE_ORBIT_SPEED    0.005f
#define MOUSE_PAN_SPEED      0.01f
#define ZOOM_SPEED           0.5f
#define SCROLL_ZOOM_SPEED    0.3f
#define DEFAULT_FOV          ((float)M_PI / 3.0f)
#define DEFAULT_ORBIT_RADIUS 5.0f
#define MIN_ORBIT_RADIUS     1.0f
#define MAX_ORBIT_RADIUS     50.0f
#define MAX_RENDER_THREADS   32

typedef struct {
  Vec3  position;
  Vec3  target;
  float yaw;
  float pitch;
  float fov;
  float orbit_radius;
} RtCamera;

typedef struct {
  Vec3  origin;
  Vec3  forward;
  Vec3  right;
  Vec3  up;
  float half_fov;
  float aspect;
} CameraBasis;

void camera_init(RtCamera *cam);
int  camera_update(RtCamera *cam, const void *input, float delta_time);
CameraBasis camera_compute_basis(const RtCamera *cam, int width, int height);
void render_scene(Backbuffer *bb, const Scene *scene, const RtCamera *cam);
```

**Key design:**

- `RtCamera` uses the `Rt` prefix to avoid name collision with Raylib's built-in `Camera` type.
- `camera_update` returns `int` (1 if camera moved, 0 if unchanged) — this will be used for adaptive quality in L12.
- `camera_update` takes `const void *input` and internally casts to `GameInput *` — this avoids a circular header dependency between `render.h` and `base.h`.

### `game/render.c` — camera_update (mouse + keyboard)

```c
int camera_update(RtCamera *cam, const void *input_ptr, float delta_time) {
  const GameInput *input = (const GameInput *)input_ptr;
  const MouseState *m = &input->mouse;

  float old_yaw   = cam->yaw;
  float old_pitch  = cam->pitch;
  float old_radius = cam->orbit_radius;
  Vec3  old_target = cam->target;

  /* Mouse left-drag: orbit. */
  if (m->left_down) {
    cam->yaw   -= m->dx * MOUSE_ORBIT_SPEED;
    cam->pitch += m->dy * MOUSE_ORBIT_SPEED;
  }

  /* Mouse right-drag or middle-drag: pan. */
  if (m->right_down || m->middle_down) {
    Vec3 forward  = vec3_normalize(vec3_sub(cam->target, cam->position));
    Vec3 world_up = vec3_make(0.0f, 1.0f, 0.0f);
    Vec3 right    = vec3_normalize(vec3_cross(forward, world_up));
    Vec3 up       = vec3_cross(right, forward);
    float pan_scale = cam->orbit_radius * MOUSE_PAN_SPEED;
    cam->target = vec3_add(cam->target,
      vec3_add(vec3_scale(right, -m->dx * pan_scale),
               vec3_scale(up,     m->dy * pan_scale)));
  }

  /* Scroll wheel: zoom. */
  if (m->scroll != 0.0f)
    cam->orbit_radius -= m->scroll * SCROLL_ZOOM_SPEED;

  /* Arrow keys: orbit. */
  if (input->buttons.camera_left.ended_down)
    cam->yaw -= CAMERA_ORBIT_SPEED * delta_time;
  if (input->buttons.camera_right.ended_down)
    cam->yaw += CAMERA_ORBIT_SPEED * delta_time;
  if (input->buttons.camera_up.ended_down)
    cam->pitch += CAMERA_ORBIT_SPEED * delta_time;
  if (input->buttons.camera_down.ended_down)
    cam->pitch -= CAMERA_ORBIT_SPEED * delta_time;

  /* W/S: zoom in/out. */
  if (input->buttons.camera_forward.ended_down)
    cam->orbit_radius -= ZOOM_SPEED * delta_time;
  if (input->buttons.camera_backward.ended_down)
    cam->orbit_radius += ZOOM_SPEED * delta_time;

  /* Clamp pitch and radius. */
  float max_pitch = (float)M_PI * 0.44f;
  if (cam->pitch >  max_pitch) cam->pitch =  max_pitch;
  if (cam->pitch < -max_pitch) cam->pitch = -max_pitch;
  if (cam->orbit_radius < MIN_ORBIT_RADIUS) cam->orbit_radius = MIN_ORBIT_RADIUS;
  if (cam->orbit_radius > MAX_ORBIT_RADIUS) cam->orbit_radius = MAX_ORBIT_RADIUS;

  recompute_position(cam);

  /* Return 1 if anything changed. */
  float dy = cam->yaw - old_yaw;
  float dp = cam->pitch - old_pitch;
  float dr = cam->orbit_radius - old_radius;
  Vec3 dt = vec3_sub(cam->target, old_target);
  return (dy*dy + dp*dp + dr*dr + vec3_dot(dt,dt)) > 1e-10f;
}
```

**Key lines:**

- Lines 10-14: Mouse orbit. `dx` and `dy` are pixel deltas from the platform. Multiplying by `MOUSE_ORBIT_SPEED` (0.005) converts pixels to radians — about 3 degrees per 10-pixel drag.
- Lines 17-26: Panning. We compute the camera's `right` and `up` vectors in world space using `vec3_cross`. The target moves along these vectors, and the camera follows because `recompute_position` places the camera relative to the target.
- Line 29: Scroll zoom directly modifies `orbit_radius`. Positive scroll (wheel up) zooms in (decreases radius).
- Lines 50-53: Pitch clamping at 0.44\*PI (about 79 degrees). Going to exactly PI/2 makes `cosf(pitch) = 0`, which collapses the orbit sphere to a point and breaks the camera.
- Lines 58-61: Movement detection by comparing old vs new state. The squared differences avoid expensive `sqrtf`. Threshold `1e-10f` catches tiny floating-point drift while detecting real user input.

### `game/render.c` — camera_compute_basis

```c
CameraBasis camera_compute_basis(const RtCamera *cam, int width, int height) {
  CameraBasis b;
  b.origin   = cam->position;
  b.forward  = vec3_normalize(vec3_sub(cam->target, cam->position));
  Vec3 world_up = vec3_make(0.0f, 1.0f, 0.0f);
  b.right    = vec3_normalize(vec3_cross(b.forward, world_up));
  b.up       = vec3_cross(b.right, b.forward);
  b.half_fov = tanf(cam->fov / 2.0f);
  b.aspect   = (float)width / (float)height;
  return b;
}
```

**Key lines:**

- Line 4: `forward` = normalized direction from camera to target. This is the center of the view.
- Line 6: `right` = `cross(forward, world_up)`. The cross product produces a vector perpendicular to both — this is the camera's "right" direction. `vec3_cross` was added to `vec3.h` in preparation for this lesson (formally introduced in L12).
- Line 7: `up` = `cross(right, forward)`. This recomputed up vector is perpendicular to both right and forward, forming an orthonormal basis. Note: no `normalize` needed here because `right` and `forward` are already unit-length and perpendicular.
- Line 8: `tanf(fov / 2)` converts the field-of-view angle to a screen-space multiplier. A 60-degree FOV has `tan(30deg) = 0.577`, meaning the screen edges are 0.577 units from center.

### `game/render.c` — multi-threaded render_scene

```c
static void *render_thread_fn(void *arg) {
  const RenderJob *job = (const RenderJob *)arg;
  int w = job->render_w, h = job->render_h;

  for (int j = job->start_row; j < job->end_row; j++)
    for (int i = 0; i < w; i++)
      job->pixels[j * job->stride + i] = color_to_pixel(
        trace_pixel(i, j, w, h, job->basis, job->scene, NULL));
  return NULL;
}

void render_scene(Backbuffer *bb, const Scene *scene, const RtCamera *cam) {
  int stride = bb->pitch / 4;
  CameraBasis basis = camera_compute_basis(cam, bb->width, bb->height);

  int num_threads = get_num_threads();
  if (num_threads > bb->height) num_threads = bb->height;

  pthread_t threads[MAX_RENDER_THREADS];
  RenderJob jobs[MAX_RENDER_THREADS];

  int rows_per_thread = bb->height / num_threads;
  int remainder       = bb->height % num_threads;
  int row = 0;

  for (int t = 0; t < num_threads; t++) {
    int count = rows_per_thread + (t < remainder ? 1 : 0);
    jobs[t] = (RenderJob){
      .start_row = row, .end_row = row + count,
      .render_w = bb->width, .render_h = bb->height,
      .stride = stride, .pixels = bb->pixels,
      .basis = &basis, .scene = scene,
    };
    pthread_create(&threads[t], NULL, render_thread_fn, &jobs[t]);
    row += count;
  }

  for (int t = 0; t < num_threads; t++)
    pthread_join(threads[t], NULL);
}
```

**Key lines:**

- Line 5: Each thread processes its assigned row range (`start_row` to `end_row`). Rows are independent — no synchronization needed.
- Line 14: `camera_compute_basis` is called ONCE. The returned `basis` struct is passed to all threads by pointer (read-only).
- Line 22-23: `remainder` handling distributes leftover rows among the first few threads. For 600 rows across 8 threads: 75 rows each, no remainder.
- Line 30: `pthread_create` launches the thread immediately. All threads run in parallel.
- Lines 34-35: `pthread_join` waits for each thread to finish. After this loop, all pixels are written.

### `game/base.h` — MouseState addition

```c
typedef struct {
  float x, y;           /* current pixel position                        */
  float dx, dy;         /* delta since last frame (for drag)             */
  float scroll;         /* wheel scroll delta (+ = zoom in, - = out)    */
  int   left_down;      /* left button held = orbit                      */
  int   right_down;     /* right button held = pan                       */
  int   middle_down;    /* middle button held = pan (alt)                */
} MouseState;

typedef struct {
  union {
    GameButtonState all[7];
    struct {
      GameButtonState quit;
      GameButtonState camera_left;
      GameButtonState camera_right;
      GameButtonState camera_up;
      GameButtonState camera_down;
      GameButtonState camera_forward;   /* W key */
      GameButtonState camera_backward;  /* S key */
    };
  } buttons;
  MouseState mouse;
} GameInput;
```

**Key change:** `GameInput` now has a `MouseState mouse` field alongside the `buttons` union. The button count grows from 5 to 7 with the addition of `camera_forward` and `camera_backward` (W/S keys).

### Platform backend — mouse event handling (X11 example)

```c
/* Mouse buttons */
case ButtonPress:
  if (ev.xbutton.button == 1) curr->mouse.left_down   = 1;
  if (ev.xbutton.button == 2) curr->mouse.middle_down = 1;
  if (ev.xbutton.button == 3) curr->mouse.right_down  = 1;
  if (ev.xbutton.button == 4) curr->mouse.scroll += 1.0f;  /* wheel up   */
  if (ev.xbutton.button == 5) curr->mouse.scroll -= 1.0f;  /* wheel down */
  break;

/* Mouse motion */
case MotionNotify:
  curr->mouse.x  = (float)ev.xmotion.x;
  curr->mouse.y  = (float)ev.xmotion.y;
  curr->mouse.dx = curr->mouse.x - g_prev_mouse_x;
  curr->mouse.dy = curr->mouse.y - g_prev_mouse_y;
  g_prev_mouse_x = curr->mouse.x;
  g_prev_mouse_y = curr->mouse.y;
  break;
```

**Key detail:** X11 reports scroll wheel as button 4 (up) and button 5 (down). The `dx`/`dy` deltas are computed by tracking the previous mouse position across events. The `prepare_input_frame` function resets `dx`, `dy`, and `scroll` to zero each frame, so they only accumulate during that frame's events.

## Common mistakes

| Symptom                                               | Cause                                           | Fix                                                                                                    |
| ----------------------------------------------------- | ----------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| Camera flips upside down at top/bottom                | Pitch not clamped                               | Clamp pitch to `[-0.44*PI, +0.44*PI]`                                                                  |
| Orbit is backwards (drag right = rotate left)         | Sign of `dx` multiplication                     | Check: `cam->yaw -= m->dx * MOUSE_ORBIT_SPEED` (negative dx = positive yaw)                            |
| Scene renders but camera doesn't move                 | Forgot to call `camera_update` in `game_update` | Add `camera_update(&state->camera, input, delta_time)`                                                 |
| Linker error: undefined reference to `pthread_create` | Missing `-lpthread` flag                        | Add `-lpthread` to `BACKEND_LIBS` in `build-dev.sh`                                                    |
| Frame rate unchanged with threading                   | Only 1 thread created                           | Check `sysconf(_SC_NPROCESSORS_ONLN)` returns > 1; verify `num_threads`                                |
| Garbled pixels / data race                            | Threads writing overlapping rows                | Verify `start_row`/`end_row` ranges are non-overlapping                                                |
| Camera right vector is zero                           | Camera looking straight up/down                 | Pitch clamp prevents this; `cross(forward, world_up)` degenerates when forward is parallel to world_up |
| Scroll zoom doesn't work on X11                       | Missing button 4/5 handling                     | X11 reports scroll as `ButtonPress` with button index 4 (up) and 5 (down)                              |

## Exercise

> Try changing `MAX_RENDER_THREADS` to 1 to force single-threaded rendering. Compare the frame rate. On a modern 8-core machine, you should see roughly 4-6x improvement with multi-threading (not 8x due to thread creation overhead and memory bandwidth limits). Restore the default when done.

## JS ↔ C concept map

| JS / Web concept                        | C equivalent in this lesson                                     | Key difference                                                        |
| --------------------------------------- | --------------------------------------------------------------- | --------------------------------------------------------------------- |
| `new OrbitControls(camera, canvas)`     | `camera_update(cam, input, dt)` — manual orbit math             | No library; sin/cos computed directly                                 |
| `event.clientX`, `event.movementX`      | `curr->mouse.x`, `curr->mouse.dx`                               | Platform accumulates deltas; no DOM events                            |
| `event.deltaY` (wheel)                  | `curr->mouse.scroll` (+1/-1 per tick)                           | X11 reports wheel as button 4/5, not a scroll delta                   |
| `camera.lookAt(target)`                 | `vec3_sub(target, position)` → `forward` → `right` → `up` basis | Manual basis computation instead of matrix library                    |
| `new Worker()` / `Promise.all(workers)` | `pthread_create` / `pthread_join`                               | OS threads, not web workers; shared memory instead of message passing |
| `navigator.hardwareConcurrency`         | `sysconf(_SC_NPROCESSORS_ONLN)`                                 | POSIX system call; returns physical core count                        |
