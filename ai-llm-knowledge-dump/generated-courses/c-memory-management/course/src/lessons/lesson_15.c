#define _POSIX_C_SOURCE 200809L
/* lesson_15.c — Arena Patterns: Per-Frame, Per-Request, Per-Thread
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates the three main arena usage patterns from the Fleury transcript.
 *
 * Run:  ./build-dev.sh --lesson=15 -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Pattern 1: Per-Frame Arena (Game Development)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ryan Fleury: "Every frame you clear the arena, meaning you reset the
 * allocation pointer back to zero.  Then you push and push and push."
 */

typedef struct {
  float x, y;
  float vx, vy;
  int   alive;
} Entity;

static void simulate_game_frame(Arena *scratch, int frame_num) {
  TempMemory temp = arena_begin_temp(scratch);

  /* Simulate entities */
  int entity_count = 100 + frame_num * 20;
  Entity *entities = ARENA_PUSH_ZERO_ARRAY(scratch, entity_count, Entity);
  for (int i = 0; i < entity_count; i++) {
    entities[i].x = (float)(i % 40);
    entities[i].y = (float)(i / 40);
    entities[i].alive = 1;
  }

  /* Allocate temporary distance calculations */
  float *distances = ARENA_PUSH_ARRAY(scratch, entity_count, float);
  for (int i = 0; i < entity_count; i++) {
    distances[i] = entities[i].x * entities[i].x +
                   entities[i].y * entities[i].y;
  }

  /* Build debug string */
  char *debug = (char *)arena_push_size(scratch, 128, 1);
  snprintf(debug, 128, "Frame %d: %d entities, used=%zu bytes",
           frame_num, entity_count, arena_total_used(scratch));
  printf("    %s\n", debug);

  arena_end_temp(temp);
  arena_check(scratch);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Pattern 2: Per-Request Arena (Web Server)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ryan Fleury: "If you're running a web server, you could have a
 * per-request arena.  Some request comes in, you push results, once
 * sent over the network, clear the arena."
 */

typedef struct {
  int    id;
  char   method[8];
  char   path[64];
  char   body[256];
} HttpRequest;

typedef struct {
  int    status;
  char  *body;
  size_t body_len;
} HttpResponse;

static HttpResponse handle_request(Arena *request_arena,
                                    const HttpRequest *req) {
  HttpResponse resp = {0};

  /* All allocations for this request go on the request arena */
  size_t buf_size = 512;
  char *buf = (char *)arena_push_size(request_arena, buf_size, 1);

  int len = snprintf(buf, buf_size,
                     "{\"message\": \"Handled %s %s\", \"id\": %d}",
                     req->method, req->path, req->id);

  resp.status   = 200;
  resp.body     = buf;
  resp.body_len = (size_t)len;
  return resp;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Pattern 3: Per-Thread Scratch Arena
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ryan Fleury: "You just have per-thread scratch arenas, which are
 * kind of like stacks that you've prepared for each thread."
 */

static _Thread_local Arena tl_scratch = {0};

static void *thread_work(void *arg) {
  int thread_id = *(int *)arg;

  /* Each thread has its own arena via _Thread_local */
  tl_scratch.min_block_size = 16 * 1024;

  TempMemory temp = arena_begin_temp(&tl_scratch);

  /* Do some work */
  int *data = ARENA_PUSH_ARRAY(&tl_scratch, 1000, int);
  for (int i = 0; i < 1000; i++) data[i] = thread_id * 1000 + i;

  char *msg = (char *)arena_push_size(&tl_scratch, 128, 1);
  snprintf(msg, 128, "Thread %d: data[500]=%d, used=%zu",
           thread_id, data[500], arena_total_used(&tl_scratch));
  printf("    %s\n", msg);

  arena_end_temp(temp);
  arena_check(&tl_scratch);

  return NULL;
}

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 15 — Arena Patterns\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── Pattern 1: Per-Frame ───────────────────────────────────────────*/
  print_section("Pattern 1: Per-Frame Arena (Game Dev)");
  {
    Arena scratch = {0};
    scratch.min_block_size = 64 * 1024;

    printf("  Simulating 5 game frames:\n\n");
    for (int f = 0; f < 5; f++) {
      simulate_game_frame(&scratch, f);
    }
    printf("\n  No leaks.  Scratch arena reused every frame.\n");
    arena_free(&scratch);
  }

  /* ── Pattern 2: Per-Request ─────────────────────────────────────────*/
  print_section("Pattern 2: Per-Request Arena (Web Server)");
  {
    Arena request_arena = {0};
    request_arena.min_block_size = 4096;

    HttpRequest requests[] = {
      {1, "GET",  "/users",  ""},
      {2, "POST", "/login",  "{\"user\": \"ryan\"}"},
      {3, "GET",  "/items",  ""},
      {4, "PUT",  "/items/1", "{\"name\": \"arena\"}"},
    };

    printf("  Processing 4 HTTP requests:\n\n");
    for (int i = 0; i < 4; i++) {
      TempMemory req_scope = arena_begin_temp(&request_arena);

      HttpResponse resp = handle_request(&request_arena, &requests[i]);
      printf("    [%d] %s %s → %d: %s\n",
             requests[i].id, requests[i].method, requests[i].path,
             resp.status, resp.body);

      /* "Send" the response, then free everything for this request */
      arena_end_temp(req_scope);
    }
    arena_check(&request_arena);
    printf("\n  Each request's memory freed instantly.  No GC pause.\n");
    arena_free(&request_arena);
  }

  /* ── Pattern 3: Per-Thread ──────────────────────────────────────────*/
  print_section("Pattern 3: Per-Thread Scratch Arena (_Thread_local)");
  {
    printf("  Launching 4 threads, each with its own scratch arena:\n\n");
    pthread_t threads[4];
    int ids[4] = {0, 1, 2, 3};

    for (int i = 0; i < 4; i++) {
      pthread_create(&threads[i], NULL, thread_work, &ids[i]);
    }
    for (int i = 0; i < 4; i++) {
      pthread_join(threads[i], NULL);
    }
    printf("\n  No locks needed — each thread has its own arena.\n");
    printf("  _Thread_local makes it invisible to callers.\n");

    /* Clean up main thread's tl_scratch */
    arena_free(&tl_scratch);
  }

  /* ── Summary ────────────────────────────────────────────────────────*/
  print_section("Summary: Choosing the Right Pattern");
  printf("  ┌───────────────────┬────────────────────────────────────────┐\n");
  printf("  │ Pattern           │ Use when...                            │\n");
  printf("  ├───────────────────┼────────────────────────────────────────┤\n");
  printf("  │ Per-frame scratch │ Game loop, animation, simulation      │\n");
  printf("  │ Per-request       │ HTTP handlers, RPC, event processing  │\n");
  printf("  │ Per-thread        │ Thread pools, parallel pipelines      │\n");
  printf("  │ Permanent         │ App-lifetime: config, lookup tables   │\n");
  printf("  └───────────────────┴────────────────────────────────────────┘\n");

  printf("\n");
  return 0;
}
