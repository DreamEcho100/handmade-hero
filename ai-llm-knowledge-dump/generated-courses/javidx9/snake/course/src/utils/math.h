/* ═══════════════════════════════════════════════════════════════════════════
 * utils/math.h  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * Tiny math utility macros shared across the entire game.
 * No game-specific logic — just generic helpers that any C project needs.
 *
 * WHY MACROS INSTEAD OF FUNCTIONS?
 * ─────────────────────────────────
 * These macros work on any numeric type (int, float, double) without needing
 * overloads or templates.  The compiler inlines them completely — zero overhead.
 *
 * JS equivalent:
 *   C: #define MIN(a, b) ((a) < (b) ? (a) : (b))
 *   JS: Math.min(a, b)
 *
 * ⚠ WARNING — double-evaluation
 *   These macros evaluate their arguments potentially twice.  NEVER pass an
 *   expression with side effects as an argument:
 *     Bad:  MIN(x++, y)   — x is incremented twice!
 *     Good: int tmp = x++; int result = MIN(tmp, y);
 *
 * WHY INCLUDE GUARDS?
 * ───────────────────
 * #ifndef / #define / #endif prevents this header being processed more than
 * once per translation unit, even if multiple files #include it.
 * JS equivalent: ES module deduplication (each module is only evaluated once).
 */

#ifndef UTILS_MATH_H   /* Include guard: skip entire file if already included */
#define UTILS_MATH_H

/* MIN — return the smaller of two values.
 * JS: Math.min(a, b)
 * C: the ternary expression — no standard Math object here. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* MAX — return the larger of two values.
 * JS: Math.max(a, b) */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* CLAMP — keep a value within [lo, hi].
 * JS: Math.max(lo, Math.min(hi, v))
 * Example: CLAMP(105, 0, 100) → 100 */
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

/* ABS — absolute value (works for int or float).
 * JS: Math.abs(v)
 * Note: the C standard library has abs() for int and fabsf() for float,
 * but this macro works for both without importing anything. */
#define ABS(v) ((v) < 0 ? -(v) : (v))

#endif /* UTILS_MATH_H */
