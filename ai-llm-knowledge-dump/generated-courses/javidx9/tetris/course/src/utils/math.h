#ifndef UTILS_MATH_H
#define UTILS_MATH_H

/* =============================================================================
 * utils/math.h — Minimal Math Utilities
 * =============================================================================
 *
 * This header provides MIN, MAX, and CLAMP macros for use across the game.
 * We use macros instead of functions for two reasons:
 *   1. Zero call overhead — no function call, no stack frame.
 *   2. Works with ANY numeric type (int, float, double) without overloads.
 *      JS/TS has Math.min/Math.max built in; C's standard library only provides
 *      type-specific versions (fmin for doubles) or none at all for integers.
 *
 * WHY NOT INLINE FUNCTIONS?
 *   An `inline int min(int a, int b)` would only work for int. You'd need
 *   separate versions for float, double, etc. Macros sidestep this because
 *   they do text substitution before compilation — the type is inferred from
 *   whatever you pass in.
 * =============================================================================
 */

/* ══════ MIN ══════
 *
 * Returns the smaller of two values.
 *
 * JS equivalent:  Math.min(a, b)
 * C macro:        MIN(a, b)
 *
 * WHY THE EXTRA PARENTHESES AROUND (a) AND (b)?
 *   Macros are pure text substitution. If you write MIN(x+1, y) the macro
 *   expands to:
 *       ((x+1) < (y) ? (x+1) : (y))
 *   Without the parentheses it would expand to:
 *       (x+1 < y ? x+1 : y)
 *   which is fine here, BUT consider MIN(x & mask, y):
 *       (x & mask < y ? x & mask : y)   ← WRONG: & has lower precedence than <
 *       (x & (mask < y) ? x & mask : y) ← what C actually parses this as!
 *   Wrapping every parameter in () prevents operator-precedence surprises.
 *
 * DOUBLE-EVALUATION HAZARD:
 *   Because macros expand to text, the arguments can be evaluated TWICE.
 *   Example: MIN(++i, j)
 *   Expands to: ((++i) < (j) ? (++i) : (j))
 *   If ++i < j, then i is incremented TWICE — a silent bug.
 *   Rule: never pass an expression with side effects (++, --, function calls
 *   with side effects) to a macro. Use a temporary variable instead:
 *       int tmp = expensive_fn();
 *       int result = MIN(tmp, limit);
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* ══════ MAX ══════
 *
 * Returns the larger of two values.
 * Same rules as MIN apply — parenthesise arguments to avoid precedence bugs.
 *
 * JS equivalent: Math.max(a, b)
 */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* ══════ CLAMP ══════
 *
 * Clamps a value to the inclusive range [lo, hi].
 * Equivalent to: Math.max(lo, Math.min(hi, val))
 *
 * JS equivalent:  Math.min(Math.max(val, lo), hi)
 * C macro:        CLAMP(val, lo, hi)
 *
 * Example:
 *   CLAMP(150, 0, 100)  →  100  (too high, clamped to hi)
 *   CLAMP(-5,  0, 100)  →    0  (too low, clamped to lo)
 *   CLAMP(50,  0, 100)  →   50  (in range, unchanged)
 *
 * NOTE: val, lo, and hi are each evaluated at most twice due to the nested
 * MIN/MAX expansion — same double-evaluation warning applies.
 * For safe use, pass only simple variables or literals (not expressions with
 * side effects like function calls or ++/--).
 *
 * COURSE NOTE: This macro is not in the original reference but is added here
 * because CLAMP is used in real game code (e.g. keeping the player inside
 * screen bounds) and it neatly demonstrates macro composition.
 */
#define CLAMP(val, lo, hi) (MAX((lo), MIN((hi), (val))))

#endif /* UTILS_MATH_H */
