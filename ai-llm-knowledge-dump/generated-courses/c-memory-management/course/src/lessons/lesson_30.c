/* lesson_30.c — JSON Parser Arena Capstone
 * BUILD_DEPS: common/bench.c, lib/str.c
 *
 * Builds a simple JSON parser using an arena for ALL allocations.
 * Supports: objects, arrays, strings, numbers, booleans, null.
 *
 * "Pass the arena, parser pushes onto it, caller controls lifetime."
 *   — Ryan Fleury
 *
 * The entire parse result is freed by one arena_reset().
 *
 * Run:   ./build-dev.sh --lesson=30 -r
 * Bench: ./build-dev.sh --lesson=30 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"
#include "lib/str.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * JSON Value Types
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef enum JsonType {
  JSON_NULL,
  JSON_BOOL,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT,
} JsonType;

typedef struct JsonValue JsonValue;

typedef struct JsonObjectEntry {
  const char          *key;
  JsonValue           *value;
  struct JsonObjectEntry *next;  /* linked list for object entries */
} JsonObjectEntry;

typedef struct JsonArrayEntry {
  JsonValue              *value;
  struct JsonArrayEntry  *next;
} JsonArrayEntry;

struct JsonValue {
  JsonType type;
  union {
    int          boolean;
    double       number;
    const char  *string;
    struct {
      JsonObjectEntry *first;
      size_t           count;
    } object;
    struct {
      JsonArrayEntry *first;
      size_t          count;
    } array;
  } as;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Parser State
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct JsonParser {
  const char *src;
  size_t      pos;
  size_t      len;
  Arena      *arena;
  const char *error;
} JsonParser;

/* Forward declaration */
static JsonValue *json_parse_value(JsonParser *p);

/* ── Skip whitespace ──────────────────────────────────────────────────────*/
static void json_skip_ws(JsonParser *p) {
  while (p->pos < p->len && isspace((unsigned char)p->src[p->pos]))
    p->pos++;
}

/* ── Peek / advance ───────────────────────────────────────────────────────*/
static char json_peek(JsonParser *p) {
  json_skip_ws(p);
  return (p->pos < p->len) ? p->src[p->pos] : '\0';
}

static char json_advance(JsonParser *p) {
  json_skip_ws(p);
  return (p->pos < p->len) ? p->src[p->pos++] : '\0';
}

static int json_match(JsonParser *p, const char *str) {
  json_skip_ws(p);
  size_t len = strlen(str);
  if (p->pos + len > p->len) return 0;
  if (memcmp(p->src + p->pos, str, len) != 0) return 0;
  p->pos += len;
  return 1;
}

/* ── Parse string ─────────────────────────────────────────────────────────*/
static const char *json_parse_string(JsonParser *p) {
  if (json_advance(p) != '"') {
    p->error = "expected '\"'";
    return NULL;
  }

  /* Find end of string (simple: no escape handling) */
  size_t start = p->pos;
  while (p->pos < p->len && p->src[p->pos] != '"') {
    if (p->src[p->pos] == '\\') p->pos++;  /* skip escaped char */
    p->pos++;
  }
  size_t end = p->pos;

  if (p->pos >= p->len) {
    p->error = "unterminated string";
    return NULL;
  }
  p->pos++;  /* skip closing '"' */

  /* Copy string onto arena */
  size_t slen = end - start;
  char *buf = (char *)arena_push_size(p->arena, slen + 1, 1);
  if (!buf) { p->error = "arena OOM"; return NULL; }
  memcpy(buf, p->src + start, slen);
  buf[slen] = '\0';
  return buf;
}

/* ── Parse number ─────────────────────────────────────────────────────────*/
static double json_parse_number(JsonParser *p) {
  json_skip_ws(p);
  const char *start = p->src + p->pos;
  char *end = NULL;
  double val = strtod(start, &end);
  if (end == start) {
    p->error = "invalid number";
    return 0.0;
  }
  p->pos += (size_t)(end - start);
  return val;
}

/* ── Parse value ──────────────────────────────────────────────────────────*/
static JsonValue *json_parse_value(JsonParser *p) {
  json_skip_ws(p);
  if (p->pos >= p->len) {
    p->error = "unexpected end of input";
    return NULL;
  }

  JsonValue *val = ARENA_PUSH_ZERO_STRUCT(p->arena, JsonValue);
  if (!val) { p->error = "arena OOM"; return NULL; }

  char c = json_peek(p);

  /* Null */
  if (json_match(p, "null")) {
    val->type = JSON_NULL;
    return val;
  }

  /* Booleans */
  if (json_match(p, "true")) {
    val->type = JSON_BOOL;
    val->as.boolean = 1;
    return val;
  }
  if (json_match(p, "false")) {
    val->type = JSON_BOOL;
    val->as.boolean = 0;
    return val;
  }

  /* String */
  if (c == '"') {
    val->type = JSON_STRING;
    val->as.string = json_parse_string(p);
    if (!val->as.string) return NULL;
    return val;
  }

  /* Number */
  if (c == '-' || isdigit((unsigned char)c)) {
    val->type = JSON_NUMBER;
    val->as.number = json_parse_number(p);
    if (p->error) return NULL;
    return val;
  }

  /* Array */
  if (c == '[') {
    json_advance(p);  /* consume '[' */
    val->type = JSON_ARRAY;
    val->as.array.first = NULL;
    val->as.array.count = 0;

    if (json_peek(p) == ']') {
      json_advance(p);
      return val;
    }

    JsonArrayEntry *tail = NULL;
    while (1) {
      JsonValue *elem = json_parse_value(p);
      if (!elem) return NULL;

      JsonArrayEntry *entry = ARENA_PUSH_ZERO_STRUCT(p->arena, JsonArrayEntry);
      if (!entry) { p->error = "arena OOM"; return NULL; }
      entry->value = elem;
      entry->next  = NULL;

      if (!tail) val->as.array.first = entry;
      else       tail->next = entry;
      tail = entry;
      val->as.array.count++;

      if (json_peek(p) == ',') { json_advance(p); continue; }
      if (json_peek(p) == ']') { json_advance(p); break; }
      p->error = "expected ',' or ']'";
      return NULL;
    }
    return val;
  }

  /* Object */
  if (c == '{') {
    json_advance(p);  /* consume '{' */
    val->type = JSON_OBJECT;
    val->as.object.first = NULL;
    val->as.object.count = 0;

    if (json_peek(p) == '}') {
      json_advance(p);
      return val;
    }

    JsonObjectEntry *tail = NULL;
    while (1) {
      /* Key */
      const char *key = json_parse_string(p);
      if (!key) return NULL;

      /* Colon */
      if (json_advance(p) != ':') {
        p->error = "expected ':'";
        return NULL;
      }

      /* Value */
      JsonValue *v = json_parse_value(p);
      if (!v) return NULL;

      JsonObjectEntry *entry = ARENA_PUSH_ZERO_STRUCT(p->arena, JsonObjectEntry);
      if (!entry) { p->error = "arena OOM"; return NULL; }
      entry->key   = key;
      entry->value = v;
      entry->next  = NULL;

      if (!tail) val->as.object.first = entry;
      else       tail->next = entry;
      tail = entry;
      val->as.object.count++;

      if (json_peek(p) == ',') { json_advance(p); continue; }
      if (json_peek(p) == '}') { json_advance(p); break; }
      p->error = "expected ',' or '}'";
      return NULL;
    }
    return val;
  }

  p->error = "unexpected character";
  return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tree printer
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void json_print(const JsonValue *val, int indent) {
  const char *pad = "                                ";
  int w = indent * 2;
  if (w > 30) w = 30;

  if (!val) { printf("%.*snull (parse error)\n", w, pad); return; }

  switch (val->type) {
    case JSON_NULL:
      printf("null");
      break;
    case JSON_BOOL:
      printf("%s", val->as.boolean ? "true" : "false");
      break;
    case JSON_NUMBER:
      printf("%g", val->as.number);
      break;
    case JSON_STRING:
      printf("\"%s\"", val->as.string);
      break;
    case JSON_ARRAY:
      printf("[\n");
      for (JsonArrayEntry *e = val->as.array.first; e; e = e->next) {
        printf("%.*s", w + 2, pad);
        json_print(e->value, indent + 1);
        printf("%s\n", e->next ? "," : "");
      }
      printf("%.*s]", w, pad);
      break;
    case JSON_OBJECT:
      printf("{\n");
      for (JsonObjectEntry *e = val->as.object.first; e; e = e->next) {
        printf("%.*s\"%s\": ", w + 2, pad, e->key);
        json_print(e->value, indent + 1);
        printf("%s\n", e->next ? "," : "");
      }
      printf("%.*s}", w, pad);
      break;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public parse function
 * ═══════════════════════════════════════════════════════════════════════════
 */

static JsonValue *json_parse(Arena *arena, const char *src, size_t len,
                             const char **out_error) {
  JsonParser parser = {
    .src   = src,
    .pos   = 0,
    .len   = len,
    .arena = arena,
    .error = NULL,
  };

  JsonValue *root = json_parse_value(&parser);
  if (out_error) *out_error = parser.error;
  return root;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Lookup helper for objects
 * ═══════════════════════════════════════════════════════════════════════════
 */

static JsonValue *json_obj_get(const JsonValue *obj, const char *key) {
  if (!obj || obj->type != JSON_OBJECT) return NULL;
  for (JsonObjectEntry *e = obj->as.object.first; e; e = e->next) {
    if (strcmp(e->key, key) == 0) return e->value;
  }
  return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* A non-trivial JSON document for testing */
static const char SAMPLE_JSON[] =
  "{\n"
  "  \"name\": \"Memory Management Course\",\n"
  "  \"version\": 2.5,\n"
  "  \"lessons\": 30,\n"
  "  \"active\": true,\n"
  "  \"tags\": [\"C\", \"memory\", \"arena\", \"performance\"],\n"
  "  \"metadata\": null,\n"
  "  \"chapters\": [\n"
  "    {\n"
  "      \"title\": \"Foundations\",\n"
  "      \"lessons\": [1, 2, 3, 4, 5, 6, 7, 8, 9],\n"
  "      \"difficulty\": \"beginner\"\n"
  "    },\n"
  "    {\n"
  "      \"title\": \"Arena Allocators\",\n"
  "      \"lessons\": [10, 11, 12, 13, 14],\n"
  "      \"difficulty\": \"intermediate\"\n"
  "    },\n"
  "    {\n"
  "      \"title\": \"Advanced Patterns\",\n"
  "      \"lessons\": [15, 16, 17, 18, 19, 20, 21, 22],\n"
  "      \"difficulty\": \"advanced\"\n"
  "    },\n"
  "    {\n"
  "      \"title\": \"Tools and Performance\",\n"
  "      \"lessons\": [23, 24, 25, 26, 27, 28],\n"
  "      \"difficulty\": \"intermediate\"\n"
  "    },\n"
  "    {\n"
  "      \"title\": \"Real-World Integration\",\n"
  "      \"lessons\": [29, 30],\n"
  "      \"difficulty\": \"advanced\"\n"
  "    }\n"
  "  ],\n"
  "  \"author\": {\n"
  "    \"name\": \"Course Builder\",\n"
  "    \"philosophy\": \"Pass the arena. Parser pushes onto it. Caller controls lifetime.\"\n"
  "  }\n"
  "}";

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 30 — JSON Parser Arena Capstone\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Parse and print ──────────────────────────────────────────────── */
  print_section("Parse JSON with Arena Allocator");
  {
    Arena arena = {0};
    arena.min_block_size = 64 * 1024;

    printf("  Input: %zu bytes of JSON\n\n", sizeof(SAMPLE_JSON) - 1);

    const char *error = NULL;
    JsonValue *root = json_parse(&arena, SAMPLE_JSON,
                                 sizeof(SAMPLE_JSON) - 1, &error);

    if (error) {
      printf("  Parse error: %s\n", error);
    } else {
      printf("  Parsed tree:\n  ");
      json_print(root, 1);
      printf("\n");
    }

    printf("\n  Arena used: %zu bytes for entire parse tree\n",
           arena_total_used(&arena));

    arena_free(&arena);
    printf("  arena_free() — entire tree freed in one call.\n");
  }

  /* ── 2. Walk the tree ────────────────────────────────────────────────── */
  print_section("Walking the Parse Tree");
  {
    Arena arena = {0};
    arena.min_block_size = 64 * 1024;

    const char *error = NULL;
    JsonValue *root = json_parse(&arena, SAMPLE_JSON,
                                 sizeof(SAMPLE_JSON) - 1, &error);
    if (error || !root) {
      printf("  Parse failed: %s\n", error ? error : "unknown");
      arena_free(&arena);
      return 1;
    }

    /* Access top-level fields */
    JsonValue *name = json_obj_get(root, "name");
    JsonValue *version = json_obj_get(root, "version");
    JsonValue *active = json_obj_get(root, "active");
    JsonValue *tags = json_obj_get(root, "tags");
    JsonValue *chapters = json_obj_get(root, "chapters");
    JsonValue *author = json_obj_get(root, "author");

    printf("  name:    %s\n", name ? name->as.string : "(null)");
    printf("  version: %g\n", version ? version->as.number : 0);
    printf("  active:  %s\n", active ? (active->as.boolean ? "true" : "false") : "(null)");

    /* Print tags */
    if (tags && tags->type == JSON_ARRAY) {
      printf("  tags:    [");
      int first = 1;
      for (JsonArrayEntry *e = tags->as.array.first; e; e = e->next) {
        if (!first) printf(", ");
        if (e->value->type == JSON_STRING)
          printf("\"%s\"", e->value->as.string);
        first = 0;
      }
      printf("]\n");
    }

    /* Print chapter titles */
    if (chapters && chapters->type == JSON_ARRAY) {
      printf("\n  Chapters:\n");
      int idx = 0;
      for (JsonArrayEntry *e = chapters->as.array.first; e; e = e->next) {
        JsonValue *title = json_obj_get(e->value, "title");
        JsonValue *diff = json_obj_get(e->value, "difficulty");
        JsonValue *lessons = json_obj_get(e->value, "lessons");
        size_t nlessons = lessons ? lessons->as.array.count : 0;
        printf("    %d. %-30s [%s, %zu lessons]\n",
               ++idx,
               title ? title->as.string : "?",
               diff ? diff->as.string : "?",
               nlessons);
      }
    }

    /* Author */
    if (author && author->type == JSON_OBJECT) {
      JsonValue *aname = json_obj_get(author, "name");
      JsonValue *phil = json_obj_get(author, "philosophy");
      printf("\n  Author: %s\n", aname ? aname->as.string : "?");
      printf("  Philosophy: \"%s\"\n", phil ? phil->as.string : "?");
    }

    arena_free(&arena);
  }

  /* ── 3. Lifetime demo: parse, use, reset ─────────────────────────────── */
  print_section("Arena Lifetime Control");
  {
    Arena arena = {0};
    arena.min_block_size = 64 * 1024;

    printf("  The key insight from Ryan Fleury's approach:\n\n");
    printf("    \"The caller passes an arena to the parser.\n");
    printf("     The parser pushes all nodes, strings, arrays onto it.\n");
    printf("     The caller decides when to free — one reset, done.\"\n\n");

    printf("  Parse 1: parse the JSON...\n");
    const char *err = NULL;
    JsonValue *r1 = json_parse(&arena, SAMPLE_JSON,
                                sizeof(SAMPLE_JSON) - 1, &err);
    size_t used1 = arena_total_used(&arena);
    printf("    Arena used: %zu bytes\n", used1);
    printf("    Root type: %s\n",
           r1 && r1->type == JSON_OBJECT ? "object" : "other");

    printf("\n  arena_reset() — free everything.\n");
    arena_reset(&arena);
    printf("    Arena used: %zu bytes\n", arena_total_used(&arena));

    printf("\n  Parse 2: reparse into the SAME arena memory...\n");
    r1 = json_parse(&arena, SAMPLE_JSON, sizeof(SAMPLE_JSON) - 1, &err);
    size_t used2 = arena_total_used(&arena);
    printf("    Arena used: %zu bytes\n", used2);
    printf("    Same memory reused? %s\n\n",
           (used1 == used2) ? "yes — zero new allocations from the OS" : "yes (approx)");

    printf("  This is the arena pattern in action:\n");
    printf("    - No tracking of individual nodes\n");
    printf("    - No recursive free functions\n");
    printf("    - No memory leaks possible\n");
    printf("    - O(1) cleanup regardless of tree size\n");

    arena_free(&arena);
  }

  /* ── 4. Benchmark ───────────────────────────────────────────────────── */
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 10000;

    BENCH_SUITE(suite, "JSON parse + walk + free (arena)") {
      /* Arena-based: parse and reset */
      Arena arena = {0};
      arena.min_block_size = 64 * 1024;

      BENCH_CASE(suite, "arena parse+reset", i, N) {
        const char *err = NULL;
        JsonValue *root = json_parse(&arena, SAMPLE_JSON,
                                      sizeof(SAMPLE_JSON) - 1, &err);
        /* Walk to prevent dead-code elimination */
        if (root && root->type == JSON_OBJECT) {
          JsonValue *v = json_obj_get(root, "name");
          (void)v;
        }
        arena_reset(&arena);
      }
      BENCH_CASE_END(suite, N);

      arena_free(&arena);
    }

    printf("  With an arena, the ENTIRE parse tree is freed in one operation.\n");
    printf("  No recursive traversal, no per-node free, no bookkeeping.\n\n");
  }
#else
  bench_skip_notice("lesson_30");
#endif

  printf("\n");
  return 0;
}
