# Lesson 30 -- JSON Parser Arena Capstone

> **What you'll build:** A complete recursive-descent JSON parser that allocates every node, string, and linked-list entry on a caller-provided arena, demonstrating the culminating pattern of this entire course.

## Observable outcome

The program parses a multi-level JSON document, prints the reconstructed tree, walks it to extract structured data (chapter titles, tags, author info), then resets the arena and reparses into the same memory with zero OS calls. The benchmark shows 10,000 parse-and-reset cycles with O(1) cleanup each time.

## New concepts

- Recursive-descent parsing with arena-backed AST allocation
- The "pass the arena" pattern: caller owns lifetime, parser is allocation-agnostic
- `arena_reset` for reuse without OS calls: parse, use, reset, repeat
- Zero-leak guarantee by construction: if the arena is freed, everything is freed
- Tagged unions in C for representing polymorphic data (JSON value types)

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_30.c` | New | JSON parser, tree printer, tree walker, lifetime demo, benchmark |
| `src/lib/arena.h` | Dependency | Arena allocator (the core of this entire course) |
| `src/lib/str.h` | Dependency | Arena-backed string utilities |
| `src/lib/str.c` | Dependency | `str_push`, `str_pushf` implementations |

---

## Background -- the "pass the arena" pattern

> "Pass the arena. Parser pushes onto it. Caller controls lifetime."
> -- Ryan Fleury

This lesson ties together everything from Lessons 10-29. The JSON parser does not call `malloc` or `free`. It receives an `Arena *` and pushes every allocation onto it. The caller decides when to free.

### Why this matters for web developers

In JavaScript, `JSON.parse()` returns an object tree. The GC tracks every node, every string, every array element. When nothing references the tree, the GC eventually collects it -- but "eventually" means unpredictable latency spikes during garbage collection pauses.

In C with an arena, cleanup is deterministic and instant:

```
  JS approach:                     C arena approach:
  const data = JSON.parse(text);   JsonValue *data = json_parse(&arena, text);
  // ... use data ...              // ... use data ...
  data = null;                     arena_reset(&arena);
  // GC collects "sometime later"  // Freed NOW. Zero latency. Zero leaks.
```

### The architecture

```
  +------------------+        +------------------+
  | Caller           |        | JSON Parser      |
  |                  |        |                  |
  |  Arena arena;    |------->|  json_parse(     |
  |  json_parse(     |  arena |    &arena,       |
  |    &arena, ...); |  ptr   |    src, len)     |
  |                  |        |                  |
  |  // use tree     |        |  Internally:     |
  |                  |        |  ARENA_PUSH_*()  |
  |  arena_reset()   |        |  for every node, |
  |  // or           |        |  string, entry   |
  |  arena_free()    |        +------------------+
  +------------------+

  The parser does not know or care when memory is freed.
  The caller does not know or care how many nodes were created.
  Separation of allocation from lifetime management.
```

A traditional malloc-based JSON parser would require a recursive `json_free` function that walks every node, every array entry, every object entry, and every string -- matching `json_parse` in reverse. With an arena, cleanup is `arena_free(&arena)` regardless of tree depth, node count, or string lengths. **The parser cannot leak by construction.**

---

## Code walkthrough

### JSON value types -- tagged union

```c
typedef enum JsonType {
  JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT,
} JsonType;

struct JsonValue {
  JsonType type;
  union {
    int          boolean;
    double       number;
    const char  *string;
    struct { JsonObjectEntry *first; size_t count; } object;
    struct { JsonArrayEntry  *first; size_t count; } array;
  } as;
};
```

A tagged union is C's answer to TypeScript's discriminated union:

```ts
  // TypeScript equivalent:
  type JsonValue =
    | { type: "null" }
    | { type: "bool"; value: boolean }
    | { type: "number"; value: number }
    | { type: "string"; value: string }
    | { type: "array"; items: JsonValue[] }
    | { type: "object"; entries: Map<string, JsonValue> }
```

Objects and arrays use linked lists of entries rather than dynamic arrays. This avoids the need for `realloc` -- each entry is simply pushed onto the arena as it is parsed.

```
  JSON object in memory (all on the arena):

  JsonValue (type=OBJECT)
  +------------------+
  | first  ----------+---> JsonObjectEntry
  | count: 3         |     +------------------+
  +------------------+     | key: "name"      |  (string on arena)
                           | value: --------  |  (JsonValue on arena)
                           | next: ----------+---> JsonObjectEntry
                           +------------------+     +------------------+
                                                    | key: "version"   |
                                                    | value: --------  |
                                                    | next: ----------+-> ...
                                                    +------------------+
```

### The parser state

```c
typedef struct JsonParser {
  const char *src;      /* source JSON string */
  size_t      pos;      /* current position */
  size_t      len;      /* total length */
  Arena      *arena;    /* all allocations go here */
  const char *error;    /* NULL if no error */
} JsonParser;
```

The parser is a simple struct passed to every parse function. The `arena` pointer is the only way memory is allocated. The `error` field is set on failure -- there is no exception mechanism in C.

### json_parse_value -- the recursive dispatcher

```c
static JsonValue *json_parse_value(JsonParser *p) {
  JsonValue *val = ARENA_PUSH_ZERO_STRUCT(p->arena, JsonValue);
  if (!val) { p->error = "arena OOM"; return NULL; }

  char c = json_peek(p);

  if (json_match(p, "null"))  { val->type = JSON_NULL;   return val; }
  if (json_match(p, "true"))  { val->type = JSON_BOOL;   val->as.boolean = 1; return val; }
  if (json_match(p, "false")) { val->type = JSON_BOOL;   val->as.boolean = 0; return val; }
  if (c == '"')               { /* parse string */ }
  if (c == '-' || isdigit(c)) { /* parse number */ }
  if (c == '[')               { /* parse array (recursive) */ }
  if (c == '{')               { /* parse object (recursive) */ }
}
```

Every call to `json_parse_value` pushes a `JsonValue` onto the arena. For arrays and objects, it recursively parses elements/entries, pushing each one. The key insight: if parsing fails halfway through, the arena still owns all partial allocations. There is nothing to clean up -- the caller will `arena_free` or `arena_reset` anyway.

### json_parse_string -- copying onto the arena

```c
static const char *json_parse_string(JsonParser *p) {
  /* Skip opening '"', find closing '"' */
  size_t start = p->pos;
  while (p->pos < p->len && p->src[p->pos] != '"') {
    if (p->src[p->pos] == '\\') p->pos++;   /* skip escaped char */
    p->pos++;
  }
  size_t end = p->pos;
  p->pos++;   /* skip closing '"' */

  /* Copy string onto arena */
  size_t slen = end - start;
  char *buf = (char *)arena_push_size(p->arena, slen + 1, 1);
  memcpy(buf, p->src + start, slen);
  buf[slen] = '\0';
  return buf;
}
```

The string is copied from the source buffer onto the arena. This is important: the source buffer might be transient (e.g., a network buffer that gets reused). The arena copy ensures the parsed tree is self-contained.

### json_parse (array parsing detail)

```c
if (c == '[') {
  json_advance(p);
  val->type = JSON_ARRAY;

  JsonArrayEntry *tail = NULL;
  while (1) {
    JsonValue *elem = json_parse_value(p);    /* recurse */

    JsonArrayEntry *entry = ARENA_PUSH_ZERO_STRUCT(p->arena, JsonArrayEntry);
    entry->value = elem;
    entry->next  = NULL;

    if (!tail) val->as.array.first = entry;
    else       tail->next = entry;
    tail = entry;
    val->as.array.count++;

    if (json_peek(p) == ',') { json_advance(p); continue; }
    if (json_peek(p) == ']') { json_advance(p); break; }
  }
}
```

Each array element creates a `JsonArrayEntry` node on the arena, linked to the previous one. The tail pointer builds the list in order without reversal.

### Walking the parsed tree

```c
JsonValue *name = json_obj_get(root, "name");
printf("name: %s\n", name ? name->as.string : "(null)");

JsonValue *chapters = json_obj_get(root, "chapters");
for (JsonArrayEntry *e = chapters->as.array.first; e; e = e->next) {
  JsonValue *title = json_obj_get(e->value, "title");
  printf("  %s\n", title->as.string);
}
```

`json_obj_get` is a linear scan of the object's entry linked list. For a parsed config file, this is fine. For a 100K-entry JSON object, you would want a hash map -- which we built in Lesson 29.

### Lifetime demo: parse, reset, reparse

```c
Arena arena = {0};
arena.min_block_size = 64 * 1024;

/* Parse 1 */
JsonValue *r1 = json_parse(&arena, json_src, json_len, &err);
size_t used1 = arena_total_used(&arena);     /* e.g., 2048 bytes */

/* Reset -- wipe arena to empty, keep backing memory */
arena_reset(&arena);
/* used is now 0, but the 64 KB backing block is still allocated */

/* Parse 2 -- reuses the same physical memory */
JsonValue *r2 = json_parse(&arena, json_src, json_len, &err);
size_t used2 = arena_total_used(&arena);     /* same as used1 */
```

`arena_reset` rewinds the arena to empty without freeing the backing memory. The next parse reuses the same pages -- zero system calls, zero TLB misses. This is the ideal pattern for repeated parsing: config file reloading, API response processing, or frame-by-frame data deserialization.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Parse error "unexpected character" | Input has BOM or non-ASCII characters | `json_skip_ws` only handles ASCII whitespace; strip BOM before parsing |
| Segfault accessing tree after `arena_reset` | All pointers into the arena are invalidated by reset | Use the tree before resetting, or keep the arena alive |
| "arena OOM" on large documents | `min_block_size` too small | Set `arena.min_block_size` to at least 2x the document size |
| Strings contain garbage | Escape sequences not fully handled | This parser handles `\\` but not `\n`, `\u`, etc. -- it is a teaching parser |
| No `json_free` function | This is by design, not a bug | The arena owns everything; `arena_free` or `arena_reset` is the free function |

---

## Build and run

```bash
./build-dev.sh --lesson=30 -r              # parse, walk, lifetime demo
./build-dev.sh --lesson=30 --bench -r      # 10K parse+reset benchmark
```

---

## Key takeaways

1. The "pass the arena" pattern separates allocation from lifetime management. The parser allocates; the caller decides when to free. This is the single most important pattern in this entire course.
2. A recursive-descent parser with arena backing cannot leak by construction. If parsing fails halfway, the arena still owns all partial allocations. One `arena_free` cleans up everything.
3. `arena_reset` enables O(1) cleanup and zero-allocation reparses. The backing memory is retained, so repeated parse cycles do not touch the OS allocator at all.
4. Tagged unions (`JsonType` enum + `union`) are C's equivalent of TypeScript's discriminated unions. You check the `type` field before accessing the `as.string` or `as.number` member.
5. This capstone demonstrates the full progression of the course: from raw `malloc`/`free` (Lessons 1-9), to arena allocators (10-14), to arena-backed data structures (hash maps in Lesson 29), to complete arena-backed applications (this JSON parser). The arena is the foundation; everything else builds on top.
