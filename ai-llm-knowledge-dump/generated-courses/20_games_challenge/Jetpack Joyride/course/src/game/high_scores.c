/* ═══════════════════════════════════════════════════════════════════════════
 * game/high_scores.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Simple text-based persistence: one "score initials\n" per line.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./high_scores.h"

#include <stdio.h>
#include <string.h>

void high_scores_init(HighScoreTable *table) {
  memset(table, 0, sizeof(*table));
}

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

int high_scores_qualifies(const HighScoreTable *table, int score) {
  if (table->count < HIGH_SCORE_COUNT) return 1;
  return score > table->entries[HIGH_SCORE_COUNT - 1].score;
}
