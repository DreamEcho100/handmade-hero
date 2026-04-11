#ifndef GAME_HIGH_SCORES_H
#define GAME_HIGH_SCORES_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/high_scores.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Top 10 high scores with 3-character initials, persisted to a text file.
 * ═══════════════════════════════════════════════════════════════════════════
 */

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

/* Initialize with empty scores. */
void high_scores_init(HighScoreTable *table);

/* Load scores from file. Returns 0 on success, -1 if file doesn't exist. */
int high_scores_load(HighScoreTable *table, const char *path);

/* Save scores to file. Returns 0 on success. */
int high_scores_save(const HighScoreTable *table, const char *path);

/* Insert a score. Returns the rank (0-9) if it qualifies, or -1 if not. */
int high_scores_insert(HighScoreTable *table, int score, const char *initials);

/* Check if a score qualifies for the high score table. */
int high_scores_qualifies(const HighScoreTable *table, int score);

#endif /* GAME_HIGH_SCORES_H */
