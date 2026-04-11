# Lesson 27 -- SoA vs AoS (Structure of Arrays vs Array of Structures)

> **What you'll build:** A 1M-particle system implemented in both Array-of-Structures (AoS) and Structure-of-Arrays (SoA) layouts, with benchmarks showing when SoA wins and why.

## Observable outcome

The program prints a layout comparison showing how memory is organized under each scheme. The benchmark shows SoA is faster than AoS for `update_positions` (which touches 4 of 5 fields), while the gap narrows for `update_all` (which touches all 5 fields).

## New concepts

- AoS layout: each object is a contiguous struct, objects stored sequentially
- SoA layout: each field is a separate contiguous array
- Cache utilization percentage: bytes loaded vs bytes actually used per cache line
- SIMD friendliness: why contiguous same-type arrays vectorize better
- When AoS is acceptable (small N, all fields accessed together)

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_27.c` | New | `ParticleAoS`, `ParticlesSoA`, init/update functions, benchmarks |
| `src/common/bench.h` | Dependency | Benchmark timing infrastructure |

---

## Background -- data layout determines performance

Lesson 26 showed that memory access order matters enormously. This lesson applies that knowledge to **data structure design**. The question is: when you have a million objects with five fields, how should you lay them out in memory?

### AoS: Array of Structures (the default in C and JS)

```
  struct Particle { float x, y, vx, vy, life; };
  Particle particles[N];

  Memory layout (each particle is 20 bytes):
  [x0 y0 vx0 vy0 l0] [x1 y1 vx1 vy1 l1] [x2 y2 vx2 vy2 l2] ...
  |---- particle 0 ---|---- particle 1 ---|---- particle 2 ---|
```

This is the natural layout. In JavaScript, it is equivalent to:
```js
  const particles = [
    { x: 0, y: 0, vx: 1, vy: 0.5, life: 100 },
    { x: 0, y: 0, vx: 1, vy: 0.5, life: 100 },
    // ... 1 million objects
  ];
```

### SoA: Structure of Arrays

```
  struct Particles {
    float *x, *y, *vx, *vy, *life;
  };

  Memory layout (5 separate contiguous arrays):
  x:    [x0    x1    x2    x3    x4    ...]
  y:    [y0    y1    y2    y3    y4    ...]
  vx:   [vx0   vx1   vx2   vx3   vx4   ...]
  vy:   [vy0   vy1   vy2   vy3   vy4   ...]
  life: [l0    l1    l2    l3    l4    ...]
```

In JavaScript, this would be:
```js
  const particles = {
    x:    new Float32Array(1000000),
    y:    new Float32Array(1000000),
    vx:   new Float32Array(1000000),
    vy:   new Float32Array(1000000),
    life: new Float32Array(1000000),
  };
```

### Why does layout matter?

Consider an `update_positions` function that only touches `x, y, vx, vy` (4 of 5 fields):

```
  AoS cache utilization:
  Cache line loads: [x0 y0 vx0 vy0 l0] [x1 y1 ...]
                                    ^^
                             unused! (life field loaded but never read)
  20 bytes loaded per particle, 16 used = 80% utilization
  For 1M particles: 4 MB of wasted bandwidth loading 'life'

  SoA cache utilization:
  x array:  [x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 x10 x11 x12 x13 x14 x15]
            |------------ one 64-byte cache line = 16 floats ----------|
  100% utilization. 'life' array is never touched, stays out of cache.
```

The SoA advantage grows as you skip more fields. A hypothetical `is_alive` check that only reads `life`:

```
  AoS: 20 bytes loaded per particle, 4 used = 20% utilization
  SoA: 4 bytes loaded per particle, 4 used = 100% utilization
  That is a 5x bandwidth improvement.
```

---

## Code walkthrough

### Data structures

```c
typedef struct ParticleAoS {
  float x, y, vx, vy, life;
} ParticleAoS;
/* sizeof = 20 bytes per particle */

typedef struct ParticlesSoA {
  float *x, *y, *vx, *vy, *life;
} ParticlesSoA;
/* 5 separate arrays, each N * sizeof(float) bytes */
```

### SoA creation and cleanup

```c
static ParticlesSoA soa_create(int n) {
  ParticlesSoA s;
  s.x    = malloc((size_t)n * sizeof(float));
  s.y    = malloc((size_t)n * sizeof(float));
  s.vx   = malloc((size_t)n * sizeof(float));
  s.vy   = malloc((size_t)n * sizeof(float));
  s.life = malloc((size_t)n * sizeof(float));
  return s;
}

static void soa_free(ParticlesSoA *s) {
  free(s->x);  free(s->y);
  free(s->vx); free(s->vy);
  free(s->life);
}
```

Five separate `malloc` calls. Five separate `free` calls. This is the ergonomic cost of SoA. With an arena, it would be five `ARENA_PUSH_ARRAY` calls and one `arena_free` -- much cleaner.

### AoS update_positions

```c
static void aos_update_positions(ParticleAoS *p, int n, float dt) {
  for (int i = 0; i < n; i++) {
    p[i].x += p[i].vx * dt;
    p[i].y += p[i].vy * dt;
  }
}
```

Each iteration loads a 20-byte struct but only reads/writes 16 bytes (x, y, vx, vy). The `life` field comes along for the ride in the same cache line, wasting bandwidth.

### SoA update_positions

```c
static void soa_update_positions(ParticlesSoA *s, int n, float dt) {
  for (int i = 0; i < n; i++) {
    s->x[i] += s->vx[i] * dt;
    s->y[i] += s->vy[i] * dt;
  }
}
```

This iterates over four contiguous `float` arrays. The `life` array is never touched and stays cold. Every byte loaded from cache is actually used.

Additionally, this loop is more **SIMD-friendly**. The compiler can vectorize operations on contiguous `float` arrays more easily than on interleaved struct fields. A SIMD register can load 4 floats from `s->x[i]` through `s->x[i+3]` in one instruction, but loading `p[i].x`, `p[i+1].x`, `p[i+2].x`, `p[i+3].x` from AoS requires a gather operation (much slower).

### update_all (touches all fields)

```c
static void aos_update_all(ParticleAoS *p, int n, float dt) {
  for (int i = 0; i < n; i++) {
    p[i].x    += p[i].vx * dt;
    p[i].y    += p[i].vy * dt;
    p[i].life -= dt;
  }
}
```

When all 5 fields are accessed, AoS has 100% utilization. SoA still works but now accesses 5 separate arrays, which can increase TLB pressure (more virtual memory pages in play). The gap between AoS and SoA narrows for `update_all`.

### Benchmark setup

```c
#define PARTICLE_COUNT (1024 * 1024)    /* 1M particles */

ParticleAoS *aos = malloc((size_t)N * sizeof(ParticleAoS));
ParticlesSoA soa = soa_create(N);
```

1M particles at 20 bytes each = 20 MB for AoS. For SoA, that is 5 arrays of 4 MB each = 20 MB total. Both layouts use the same total memory, but the access pattern makes the difference.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| AoS and SoA show identical times | `PARTICLE_COUNT` too small (working set fits in L1) | Use 1M+ particles so the working set exceeds L2/L3 |
| SoA `update_all` is slower than AoS | 5 separate arrays thrash the TLB with many pages | Allocate all SoA arrays from one contiguous buffer or an arena |
| Compiler auto-vectorizes AoS, closing the gap | High optimization levels can sometimes fix interleaved access | Compare at `-O1` to see the raw cache effect |
| Segfault in `soa_free` | One of the `malloc` calls returned NULL | Check each pointer after `soa_create` |

---

## Build and run

```bash
./build-dev.sh --lesson=27 -r              # layout explanation only
./build-dev.sh --lesson=27 --bench -r      # run benchmarks with 1M particles
```

---

## Key takeaways

1. AoS is the natural layout (one struct per object) but wastes cache bandwidth when you access a subset of fields. SoA separates each field into its own contiguous array, achieving 100% cache utilization for any subset of fields.
2. The performance difference scales with how many fields you skip. Accessing 1 of 8 fields gives SoA an 8x bandwidth advantage over AoS. Accessing all fields gives roughly equal performance.
3. SoA is more SIMD-friendly because contiguous same-type arrays can be loaded directly into SIMD registers without gather operations.
4. SoA is harder to manage (multiple allocations, multiple frees, no single "object" to pass around). Using an arena for the backing memory eliminates most of the management overhead.
5. In JS, the `TypedArray` approach (one `Float32Array` per field) is the SoA pattern. Game engines like Three.js use this for particle systems and GPU buffer data. The ECS (Entity Component System) architecture popular in modern game engines is essentially SoA applied to game objects.
