/* =============================================================================
 * utils/math.h — Maths Utilities (MIN, MAX, CLAMP, ABS)
 * =============================================================================
 *
 * Simple scalar macros used throughout game and audio code.
 *
 * WHY MACROS FOR MIN/MAX INSTEAD OF FUNCTIONS?
 * ─────────────────────────────────────────────
 * C (before C11 generics) has no overloading.  A function `min_int` only
 * works with int; a macro works with any numeric type.
 *
 * Trade-off: macros evaluate arguments TWICE — never pass side-effectful
 * expressions like MIN(i++, j++):
 *   MIN(i++, j++)  →  ((i++) < (j++) ? (i++) : (j++))
 *   → i or j incremented twice!
 *
 * JS equivalent:  Math.min, Math.max — built-in, generic, no trade-offs.
 *
 * FROGGER NOTE:
 *   No Vec2 or rotation math needed here — Frogger uses tile-grid movement,
 *   not continuous physics.  The scalar macros are enough.
 * =============================================================================
 */

#ifndef FROGGER_MATH_H
#define FROGGER_MATH_H

/* ══════ Scalar Macros ══════════════════════════════════════════════════════ */

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* CLAMP — restrict v to the inclusive range [lo, hi].
   Equivalent to Math.max(lo, Math.min(hi, v)) in JS.                    */
#ifndef CLAMP
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#endif

/* ABS — absolute value for any numeric type.
   For float arithmetic, prefer fabsf() from <math.h>.                   */
#ifndef ABS
#define ABS(v) ((v) < 0 ? -(v) : (v))
#endif

#endif /* FROGGER_MATH_H */
