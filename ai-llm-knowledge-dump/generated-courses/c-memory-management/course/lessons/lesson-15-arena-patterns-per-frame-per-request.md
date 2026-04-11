# Lesson 15 -- Arena Patterns: Per-Frame, Per-Request, Per-Thread

> **What you'll build:** Three complete arena usage patterns -- a game loop scratch arena, a web server per-request arena, and per-thread scratch arenas using `_Thread_local` -- showing how arenas replace garbage collectors in real systems.

## Observable outcome

Run the program and you see 5 simulated game frames with entity/distance calculations freed each frame, 4 HTTP requests handled with per-request allocation and instant cleanup, 4 threads each running with their own scratch arena (no locks needed), and a summary table mapping patterns to use cases.

## New concepts

| Concept | JS/TS analogy |
|---------|---------------|
| Per-frame scratch arena | Like React's reconciliation: build the new tree, diff, then discard the scratch work |
| Per-request arena | Like Express middleware context that is garbage-collected after `res.send()`, but instant |
| `_Thread_local` | Like Node's `AsyncLocalStorage` -- per-execution-context state invisible to callers |
| Lifetime categories | Like React component lifecycles (mount/unmount), but for raw memory |

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_15.c` | New | Three arena patterns: per-frame, per-request, per-thread |
| `src/lib/arena.h` | Dependency | Arena with TempMemory support |

---

## Background -- thinking in lifetimes, not objects

In JavaScript, you allocate objects freely and the garbage collector figures out when they die. This works but has costs: GC pauses, unpredictable latency, memory overhead from tracking references.

The arena insight (from Ryan Fleury) is that most allocations naturally cluster into lifetime groups. Instead of tracking individual objects, you identify the natural boundaries:

```
  Lifetime categories in a typical program:

  Permanent ──────────────────────────────────────> (app lifetime)
    Config, lookup tables, font data

  Per-frame   [frame 0] [frame 1] [frame 2] ...    (16ms each)
    Entity arrays, collision results, debug strings

  Per-request    [req A]   [req B]    [req C] ...   (variable)
    Parse buffers, response body, temp strings

  Per-thread ════thread 0════  ════thread 1════     (thread lifetime)
    Scratch space for parallel work
```

Each category gets one arena. When the lifetime ends, you reset the arena. All allocations in that lifetime are freed in one operation. No individual tracking, no GC pauses, no leaks.

### JS comparison

```
  // JS: GC handles cleanup (unpredictable timing)
  app.get('/users', (req, res) => {
      const users = db.query('SELECT * FROM users');
      const json = JSON.stringify(users);
      res.send(json);
      // GC will eventually free users, json, etc.
  });

  // C with arenas: deterministic instant cleanup
  void handle_request(Arena *request_arena, ...) {
      TempMemory scope = arena_begin_temp(request_arena);
      User *users = query_users(request_arena);
      char *json = format_json(request_arena, users);
      send_response(json);
      arena_end_temp(scope);  // EVERYTHING freed instantly
  }
```

---

## Code walkthrough

### Pattern 1: per-frame scratch arena (lines 26-60)

The `simulate_game_frame` function shows a complete frame lifecycle:

```c
static void simulate_game_frame(Arena *scratch, int frame_num) {
    TempMemory temp = arena_begin_temp(scratch);

    int entity_count = 100 + frame_num * 20;
    Entity *entities = ARENA_PUSH_ZERO_ARRAY(scratch, entity_count, Entity);
    // ... initialize entities ...

    float *distances = ARENA_PUSH_ARRAY(scratch, entity_count, float);
    // ... compute distances ...

    char *debug = (char *)arena_push_size(scratch, 128, 1);
    // ... format debug string ...

    arena_end_temp(temp);
    arena_check(scratch);
}
```

Every frame: begin a temp scope, allocate entities, distances, debug strings, do all the work, end the scope. The arena rewinds to the checkpoint. No individual frees. `arena_check` verifies nothing leaked.

The main loop (lines 141-152) calls this 5 times. Each frame allocates a different amount (100, 120, 140, 160, 180 entities), but the arena reuses the same memory every frame.

### Pattern 2: per-request arena (lines 62-100, 154-181)

The server pattern uses a shared arena with temp scopes per request:

```c
for (int i = 0; i < 4; i++) {
    TempMemory req_scope = arena_begin_temp(&request_arena);
    HttpResponse resp = handle_request(&request_arena, &requests[i]);
    // "send" the response
    arena_end_temp(req_scope);
}
arena_check(&request_arena);
```

`handle_request` allocates response buffers on the request arena:

```c
static HttpResponse handle_request(Arena *request_arena, const HttpRequest *req) {
    char *buf = (char *)arena_push_size(request_arena, buf_size, 1);
    int len = snprintf(buf, buf_size, "{\"message\": \"Handled %s %s\"}", ...);
    // ...
}
```

After "sending" each response, `arena_end_temp` frees everything for that request instantly. No per-string cleanup, no GC pause. This is how high-performance C servers handle memory -- each request gets a scope, and cleanup is a single pointer reset.

### Pattern 3: per-thread (_Thread_local) (lines 102-133, 184-201)

A thread-local arena eliminates all synchronization:

```c
static _Thread_local Arena tl_scratch = {0};
```

`_Thread_local` (C11) gives each thread its own copy of the variable. No mutex, no atomic operations, no contention. Each thread's arena is completely private:

```c
static void *thread_work(void *arg) {
    int thread_id = *(int *)arg;
    tl_scratch.min_block_size = 16 * 1024;

    TempMemory temp = arena_begin_temp(&tl_scratch);
    int *data = ARENA_PUSH_ARRAY(&tl_scratch, 1000, int);
    // ... work ...
    arena_end_temp(temp);
    arena_check(&tl_scratch);
    return NULL;
}
```

The main function launches 4 threads with `pthread_create`. Each thread writes to its own `tl_scratch` without any locks. In Node.js terms, this is like `AsyncLocalStorage` providing per-request context, except it is per-thread and involves zero overhead.

### Summary table (lines 205-213)

```
  ┌───────────────────┬────────────────────────────────────────┐
  | Pattern           | Use when...                            |
  ├───────────────────┼────────────────────────────────────────┤
  | Per-frame scratch | Game loop, animation, simulation      |
  | Per-request       | HTTP handlers, RPC, event processing  |
  | Per-thread        | Thread pools, parallel pipelines      |
  | Permanent         | App-lifetime: config, lookup tables   |
  └───────────────────┴────────────────────────────────────────┘
```

---

## Build and run

```bash
./build-dev.sh --lesson=15 -r
```

Note: this lesson uses pthreads. The build script links with `-lpthread`.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Per-frame arena grows unbounded | Permanent data pushed onto scratch arena | Use a separate permanent arena for data that outlives a frame |
| Response pointer invalid after send | `arena_end_temp` called before response fully sent | Ensure response is fully written before ending the temp scope |
| Thread-local arena not cleaned up | `arena_free` not called before thread exit | Add cleanup at end of thread function |
| "One arena or many?" | Unclear lifetime boundaries | One arena per distinct lifetime category; minimum: permanent + scratch |

---

## Key takeaways

1. Most allocations cluster into lifetime groups. Assign one arena to each group instead of tracking individual objects.
2. Per-frame: begin temp scope, do frame work, end temp scope. Repeat every frame. Zero leaks by design.
3. Per-request: wrap each request in a temp scope. All response buffers, parse trees, and temp strings freed instantly.
4. Per-thread: `_Thread_local Arena` gives each thread private scratch space with zero synchronization cost.
5. This pattern replaces garbage collection in C. It is faster (no GC pauses), simpler (no reference counting), and deterministic (you control exactly when memory is freed).
