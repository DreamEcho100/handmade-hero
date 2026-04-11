# Lesson 24 -- High Scores and File I/O

## Observable Outcome
After the game ends, the final score is checked against a persistent high score table stored in `highscores.txt`. If the score qualifies, it is inserted at the correct rank. Scores persist across application restarts -- closing and reopening the game preserves all previous high scores. The file format is human-readable: one "score initials" pair per line.

## New Concepts (max 3)
1. Simple text-based file persistence: `fopen`/`fprintf`/`fscanf` for structured data
2. Sorted insertion with array shift: maintaining a descending-order leaderboard in a fixed-size array
3. Qualification check: `high_scores_qualifies` as a gate before insertion

## Prerequisites
- Lesson 21 (game over screen, final score computation)
- Lesson 23 (game restart flow)
- Basic C file I/O (fopen, fclose, fprintf, fscanf)

## Background

High scores are the simplest form of game persistence. They give players a reason to replay -- beating your own record creates intrinsic motivation. Our implementation stores a top-10 leaderboard where each entry has a numeric score and 3-character initials (like classic arcade machines).

The file format is deliberately primitive: one line per entry, with the score and initials separated by a space. No JSON, no binary format, no checksums. This makes the file human-readable and trivially editable for debugging. The trade-off is that a malicious user could hand-edit the file to insert fake scores, but for a single-player game this is an acceptable risk.

The insertion algorithm maintains the array in descending order (highest score first). When a new score arrives, we find the first entry it exceeds, shift all lower entries down by one (dropping the lowest if the table is full), and insert the new entry at the correct position. This is O(n) for n=10 entries, which is negligible.

The qualification check is a useful optimization: before prompting the player for initials (or auto-inserting), we can quickly check if their score beats the lowest entry in the table. If the table has fewer than 10 entries, any score qualifies. If the table is full, the score must exceed the 10th entry to qualify. This prevents unnecessary file writes and UI interruptions for low scores.

## Code Walkthrough

### Step 1: Data structures (high_scores.h)

```c
#define HIGH_SCORE_COUNT 10
#define INITIALS_LEN 4  /* 3 chars + null terminator */

typedef struct {
  int score;
  char initials[INITIALS_LEN];
} HighScoreEntry;

typedef struct {
  HighScoreEntry entries[HIGH_SCORE_COUNT];
  int count;
} HighScoreTable;
```

The `count` field tracks how many valid entries exist. It starts at 0 and grows up to HIGH_SCORE_COUNT (10). The `initials` array is 4 bytes: 3 visible characters plus a null terminator.

### Step 2: Initialization (high_scores.c)

```c
void high_scores_init(HighScoreTable *table) {
  memset(table, 0, sizeof(*table));
}
```

A zeroed table has count=0 and all scores=0. This is the state when no highscores.txt file exists yet.

### Step 3: Loading from file (high_scores.c)

```c
int high_scores_load(HighScoreTable *table, const char *path) {
  high_scores_init(table);

  FILE *f = fopen(path, "r");
  if (!f) return -1;

  char line[64];
  while (table->count < HIGH_SCORE_COUNT && fgets(line, sizeof(line), f)) {
    HighScoreEntry *e = &table->entries[table->count];
    if (sscanf(line, "%d %3s", &e->score, e->initials) == 2) {
      table->count++;
    }
  }

  fclose(f);
  return 0;
}
```

The function reads one line at a time with `fgets`, then parses each line with `sscanf`. The `%3s` format specifier reads at most 3 non-whitespace characters into the initials field. If a line does not match the expected format (e.g., empty line, comment), `sscanf` returns fewer than 2 and the line is silently skipped. The table is re-initialized before loading, preventing stale data from a previous load.

The function returns -1 if the file does not exist. This is not an error -- it simply means no scores have been saved yet. The game continues with an empty table.

### Step 4: Saving to file (high_scores.c)

```c
int high_scores_save(const HighScoreTable *table, const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) {
    fprintf(stderr, "high_scores_save: cannot write '%s'\n", path);
    return -1;
  }

  for (int i = 0; i < table->count; i++) {
    fprintf(f, "%d %s\n", table->entries[i].score, table->entries[i].initials);
  }

  fclose(f);
  return 0;
}
```

Opening with `"w"` truncates the file, so we rewrite all entries every time. This is the simplest correct approach for a small dataset. The output format matches the input format exactly, creating a round-trip that preserves data perfectly.

### Step 5: Sorted insertion (high_scores.c)

```c
int high_scores_insert(HighScoreTable *table, int score, const char *initials) {
  /* Find insertion position */
  int pos = table->count;
  for (int i = 0; i < table->count; i++) {
    if (score > table->entries[i].score) {
      pos = i;
      break;
    }
  }

  if (pos >= HIGH_SCORE_COUNT) return -1; /* Doesn't qualify */

  /* Shift lower scores down */
  int shift_count = table->count - pos;
  if (table->count < HIGH_SCORE_COUNT) {
    shift_count = table->count - pos;
    table->count++;
  } else {
    shift_count = HIGH_SCORE_COUNT - 1 - pos;
  }

  for (int i = shift_count; i > 0; i--) {
    table->entries[pos + i] = table->entries[pos + i - 1];
  }

  /* Insert new entry */
  table->entries[pos].score = score;
  strncpy(table->entries[pos].initials, initials, 3);
  table->entries[pos].initials[3] = '\0';

  return pos;
}
```

The algorithm works in three steps:

1. **Find position**: scan from highest to lowest, stopping at the first entry the new score beats. If the new score does not beat any existing entry, `pos` equals `table->count` (one past the last entry).

2. **Shift down**: move all entries from `pos` onward one slot lower. If the table is full, the last entry is implicitly dropped (it falls off the end of the fixed-size array). If the table has room, `count` increments.

3. **Insert**: write the new score and initials at position `pos`. The `strncpy` with explicit null termination ensures the initials field is always properly terminated.

The function returns the rank (0-9) on success or -1 if the score does not qualify.

### Step 6: Qualification check (high_scores.c)

```c
int high_scores_qualifies(const HighScoreTable *table, int score) {
  if (table->count < HIGH_SCORE_COUNT) return 1;
  return score > table->entries[HIGH_SCORE_COUNT - 1].score;
}
```

If the table is not full, any score qualifies (there is room to add it). If the table is full, the score must beat the lowest entry (the one at index HIGH_SCORE_COUNT - 1).

### Step 7: Integration in game_init (game/main.c)

```c
high_scores_init(&state->high_scores);
high_scores_load(&state->high_scores, "highscores.txt");
```

Loading happens during initialization. The file path is relative to the working directory, which is the game's root folder (where `assets/` lives). If the file does not exist, the table stays empty -- no error, no crash.

## Common Mistakes

**Not initializing the table before loading.** If `high_scores_load` fails (file missing) and the table was not pre-initialized, the entries contain garbage data. The `high_scores_init` call inside `high_scores_load` handles this, but calling `high_scores_init` before the load is a defensive best practice.

**Using `%s` instead of `%3s` in sscanf.** Without the width limit, `sscanf` reads an arbitrarily long string into the 4-byte `initials` buffer, causing a buffer overflow. The `%3s` format reads at most 3 characters, fitting safely with the null terminator.

**Shifting entries in the wrong direction.** The shift loop must copy from the bottom up: `entries[pos+i] = entries[pos+i-1]`. Copying from top down (starting at `pos`) overwrites entries before they are moved, resulting in duplicate entries and lost scores.

**Opening the save file with "a" instead of "w".** Append mode adds new scores to the end without removing old ones, causing the file to grow indefinitely. Since we rewrite the entire sorted table each time, truncating with "w" is correct.

**Using strncpy without explicitly null-terminating.** If the initials string is exactly 3 characters, `strncpy(dest, src, 3)` does NOT append a null terminator. The explicit `initials[3] = '\0'` is mandatory to prevent reading past the buffer when printing.
