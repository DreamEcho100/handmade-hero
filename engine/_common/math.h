#ifndef DE100_COMMON_MATH_H
#define DE100_COMMON_MATH_H

#include "base.h"

// TODO: support other int type like i8, i16, i64, u8, u16, u64

#if !HAS_C11
#error "C11 support is required for _Generic macros."
#endif
// ═══════════════════════════════════════════════════════════════════════════
// INTEGER LIMITS FOR OVERFLOW CHECKING
// ═══════════════════════════════════════════════════════════════════════════

#define DE100_I8_MIN (-128)
#define DE100_I8_MAX 127
#define DE100_U8_MAX 255U

#define DE100_I16_MIN (-32768)
#define DE100_I16_MAX 32767
#define DE100_U16_MAX 65535U

#define DE100_I32_MIN (-2147483647 - 1)
#define DE100_I32_MAX 2147483647
#define DE100_U32_MAX 4294967295U

#define DE100_I64_MIN (-9223372036854775807LL - 1)
#define DE100_I64_MAX 9223372036854775807LL
#define DE100_U64_MAX 18446744073709551615ULL

// For round half-away-from-zero
#define DE100_ROUND_HALF_F32 0.5f
#define DE100_ROUND_HALF_F64 0.5

// ═══════════════════════════════════════════════════════════════════════════
// FUNCTION GENERATOR MACROS
// ═══════════════════════════════════════════════════════════════════════════

// Safe floor: truncate-and-correct, works for all finite values in range.
#define DE100_DEFINE_FLOOR(ftype, itype)                                       \
  de100_file_scoped_fn inline itype floor_##ftype##_to_##itype(ftype x) {      \
    itype i = (itype)x;                                                        \
    return i - (i > x);                                                        \
  }

// Overflow-safe floor: checks bounds and clamps to integer limits
#define DE100_DEFINE_FLOOR_SAFE(ftype, itype, imin, imax)                      \
  de100_file_scoped_fn inline itype floor_##ftype##_to_##itype##_safe(         \
      ftype x) {                                                               \
    if (x <= (ftype)(imin))                                                    \
      return imin;                                                             \
    if (x >= (ftype)(imax))                                                    \
      return imax;                                                             \
    itype i = (itype)x;                                                        \
    return i - (i > x);                                                        \
  }

// Safe ceil: truncate-and-correct.
#define DE100_DEFINE_CEIL(ftype, itype)                                        \
  de100_file_scoped_fn inline itype ceil_##ftype##_to_##itype(ftype x) {       \
    itype i = (itype)x;                                                        \
    return i + (i < x);                                                        \
  }

// Overflow-safe ceil: checks bounds and clamps to integer limits
#define DE100_DEFINE_CEIL_SAFE(ftype, itype, imin, imax)                       \
  de100_file_scoped_fn inline itype ceil_##ftype##_to_##itype##_safe(          \
      ftype x) {                                                               \
    if (x <= (ftype)(imin))                                                    \
      return imin;                                                             \
    if (x >= (ftype)(imax))                                                    \
      return imax;                                                             \
    itype i = (itype)x;                                                        \
    return i + (i < x);                                                        \
  }

// Round: half away from zero.
#define DE100_DEFINE_ROUND(ftype, itype, half)                                 \
  de100_file_scoped_fn inline itype round_##ftype##_to_##itype(ftype x) {      \
    return (itype)(x + (x > 0 ? half : -half));                                \
  }

// Overflow-safe round: checks bounds and clamps to integer limits
#define DE100_DEFINE_ROUND_SAFE(ftype, itype, imin, imax, half)                \
  de100_file_scoped_fn inline itype round_##ftype##_to_##itype##_safe(         \
      ftype x) {                                                               \
    if (x <= (ftype)(imin))                                                    \
      return imin;                                                             \
    if (x >= (ftype)(imax))                                                    \
      return imax;                                                             \
    return (itype)(x + (x > 0 ? half : -half));                                \
  }

// ═══════════════════════════════════════════════════════════════════════════
// FLOOR FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

DE100_DEFINE_FLOOR(f32, i32)
DE100_DEFINE_FLOOR(f32, u32)
DE100_DEFINE_FLOOR(f64, i32)
DE100_DEFINE_FLOOR(f64, u32)

DE100_DEFINE_FLOOR_SAFE(f32, i32, DE100_I32_MIN, DE100_I32_MAX)
DE100_DEFINE_FLOOR_SAFE(f32, u32, 0, DE100_U32_MAX)
DE100_DEFINE_FLOOR_SAFE(f64, i32, DE100_I32_MIN, DE100_I32_MAX)
DE100_DEFINE_FLOOR_SAFE(f64, u32, 0, DE100_U32_MAX)

#define de100_floorf_to_i32(x)                                                 \
  _Generic((x), f32: floor_f32_to_i32, f64: floor_f64_to_i32)(x)
#define de100_floorf_to_u32(x)                                                 \
  _Generic((x), f32: floor_f32_to_u32, f64: floor_f64_to_u32)(x)

#define de100_floorf_to_i32_safe(x)                                            \
  _Generic((x), f32: floor_f32_to_i32_safe, f64: floor_f64_to_i32_safe)(x)
#define de100_floorf_to_u32_safe(x)                                            \
  _Generic((x), f32: floor_f32_to_u32_safe, f64: floor_f64_to_u32_safe)(x)

// ═══════════════════════════════════════════════════════════════════════════
// CEIL FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

DE100_DEFINE_CEIL(f32, i32)
DE100_DEFINE_CEIL(f32, u32)
DE100_DEFINE_CEIL(f64, i32)
DE100_DEFINE_CEIL(f64, u32)

DE100_DEFINE_CEIL_SAFE(f32, i32, DE100_I32_MIN, DE100_I32_MAX)
DE100_DEFINE_CEIL_SAFE(f32, u32, 0, DE100_U32_MAX)
DE100_DEFINE_CEIL_SAFE(f64, i32, DE100_I32_MIN, DE100_I32_MAX)
DE100_DEFINE_CEIL_SAFE(f64, u32, 0, DE100_U32_MAX)

#define de100_ceilf_to_i32(x)                                                  \
  _Generic((x), f32: ceil_f32_to_i32, f64: ceil_f64_to_i32)(x)
#define de100_ceilf_to_u32(x)                                                  \
  _Generic((x), f32: ceil_f32_to_u32, f64: ceil_f64_to_u32)(x)

#define de100_ceilf_to_i32_safe(x)                                             \
  _Generic((x), f32: ceil_f32_to_i32_safe, f64: ceil_f64_to_i32_safe)(x)
#define de100_ceilf_to_u32_safe(x)                                             \
  _Generic((x), f32: ceil_f32_to_u32_safe, f64: ceil_f64_to_u32_safe)(x)

// ═══════════════════════════════════════════════════════════════════════════
// ROUND FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

DE100_DEFINE_ROUND(f32, i32, DE100_ROUND_HALF_F32)
DE100_DEFINE_ROUND(f32, u32, DE100_ROUND_HALF_F32)
DE100_DEFINE_ROUND(f64, i32, DE100_ROUND_HALF_F64)
DE100_DEFINE_ROUND(f64, u32, DE100_ROUND_HALF_F64)

DE100_DEFINE_ROUND_SAFE(f32, i32, DE100_I32_MIN, DE100_I32_MAX,
                        DE100_ROUND_HALF_F32)
DE100_DEFINE_ROUND_SAFE(f32, u32, 0, DE100_U32_MAX, DE100_ROUND_HALF_F32)
DE100_DEFINE_ROUND_SAFE(f64, i32, DE100_I32_MIN, DE100_I32_MAX,
                        DE100_ROUND_HALF_F64)
DE100_DEFINE_ROUND_SAFE(f64, u32, 0, DE100_U32_MAX, DE100_ROUND_HALF_F64)

#define de100_roundf_to_i32(x)                                                 \
  _Generic((x), f32: round_f32_to_i32, f64: round_f64_to_i32)(x)
#define de100_roundf_to_u32(x)                                                 \
  _Generic((x), f32: round_f32_to_u32, f64: round_f64_to_u32)(x)

#define de100_roundf_to_i32_safe(x)                                            \
  _Generic((x), f32: round_f32_to_i32_safe, f64: round_f64_to_i32_safe)(x)
#define de100_roundf_to_u32_safe(x)                                            \
  _Generic((x), f32: round_f32_to_u32_safe, f64: round_f64_to_u32_safe)(x)
// ═══════════════════════════════════════════════════════════════════════════
// COMMON UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

// Min
de100_file_scoped_fn inline f32 min_f32(f32 a, f32 b) { return a < b ? a : b; }
de100_file_scoped_fn inline f64 min_f64(f64 a, f64 b) { return a < b ? a : b; }
de100_file_scoped_fn inline i32 min_i32(i32 a, i32 b) { return a < b ? a : b; }
de100_file_scoped_fn inline u32 min_u32(u32 a, u32 b) { return a < b ? a : b; }

#define de100_min(a, b)                                                        \
  _Generic((a), f32: min_f32, f64: min_f64, i32: min_i32, u32: min_u32)(a, b)

// Max
de100_file_scoped_fn inline f32 max_f32(f32 a, f32 b) { return a > b ? a : b; }
de100_file_scoped_fn inline f64 max_f64(f64 a, f64 b) { return a > b ? a : b; }
de100_file_scoped_fn inline i32 max_i32(i32 a, i32 b) { return a > b ? a : b; }
de100_file_scoped_fn inline u32 max_u32(u32 a, u32 b) { return a > b ? a : b; }

#define de100_max(a, b)                                                        \
  _Generic((a), f32: max_f32, f64: max_f64, i32: max_i32, u32: max_u32)(a, b)

// Clamp
de100_file_scoped_fn inline f32 clamp_f32(f32 x, f32 min_val, f32 max_val) {
  return x < min_val ? min_val : (x > max_val ? max_val : x);
}
de100_file_scoped_fn inline f64 clamp_f64(f64 x, f64 min_val, f64 max_val) {
  return x < min_val ? min_val : (x > max_val ? max_val : x);
}
de100_file_scoped_fn inline i32 clamp_i32(i32 x, i32 min_val, i32 max_val) {
  return x < min_val ? min_val : (x > max_val ? max_val : x);
}
de100_file_scoped_fn inline u32 clamp_u32(u32 x, u32 min_val, u32 max_val) {
  return x < min_val ? min_val : (x > max_val ? max_val : x);
}

#define de100_clamp(x, min_val, max_val)                                       \
  _Generic((x),                                                                \
      f32: clamp_f32,                                                          \
      f64: clamp_f64,                                                          \
      i32: clamp_i32,                                                          \
      u32: clamp_u32)(x, min_val, max_val)

// Lerp (linear interpolation)
de100_file_scoped_fn inline f32 lerp_f32(f32 a, f32 b, f32 t) {
  return a + t * (b - a);
}
de100_file_scoped_fn inline f64 lerp_f64(f64 a, f64 b, f64 t) {
  return a + t * (b - a);
}

#define de100_lerp(a, b, t) _Generic((a), f32: lerp_f32, f64: lerp_f64)(a, b, t)

// Sign (returns -1, 0, or 1)
de100_file_scoped_fn inline i32 sign_f32(f32 x) {
  return (x > 0.0f) - (x < 0.0f);
}
de100_file_scoped_fn inline i32 sign_f64(f64 x) {
  return (x > 0.0) - (x < 0.0);
}
de100_file_scoped_fn inline i32 sign_i32(i32 x) { return (x > 0) - (x < 0); }

#define de100_sign(x)                                                          \
  _Generic((x), f32: sign_f32, f64: sign_f64, i32: sign_i32)(x)

// Absolute value
de100_file_scoped_fn inline f32 abs_f32(f32 x) { return x < 0.0f ? -x : x; }
de100_file_scoped_fn inline f64 abs_f64(f64 x) { return x < 0.0 ? -x : x; }
de100_file_scoped_fn inline i32 abs_i32(i32 x) { return x < 0 ? -x : x; }

#define de100_abs(x) _Generic((x), f32: abs_f32, f64: abs_f64, i32: abs_i32)(x)
// de100_file_scoped_fn inline f32 abs_f32(f32 x) { return fabsf(x); }
// de100_file_scoped_fn inline f64 abs_f64(f64 x) { return fabs(x); }
// #define de100_absf(x) _Generic((x), f32: abs_f32, f64: abs_f64)(x)

// Square
de100_file_scoped_fn inline f32 square_f32(f32 x) { return x * x; }
de100_file_scoped_fn inline f64 square_f64(f64 x) { return x * x; }
de100_file_scoped_fn inline i32 square_i32(i32 x) { return x * x; }

#define de100_square(x)                                                        \
  _Generic((x), f32: square_f32, f64: square_f64, i32: square_i32)(x)

// Saturating add (clamps to type limits on overflow)
de100_file_scoped_fn inline u32 sat_add_u32(u32 a, u32 b) {
  u32 result = a + b;
  return result < a ? DE100_U32_MAX : result;
}
de100_file_scoped_fn inline i32 sat_add_i32(i32 a, i32 b) {
  if (b > 0 && a > DE100_I32_MAX - b)
    return DE100_I32_MAX;
  if (b < 0 && a < DE100_I32_MIN - b)
    return DE100_I32_MIN;
  return a + b;
}

#define de100_sat_add(a, b)                                                    \
  _Generic((a), i32: sat_add_i32, u32: sat_add_u32)(a, b)

// Saturating subtract (clamps to type limits on underflow)
de100_file_scoped_fn inline u32 sat_sub_u32(u32 a, u32 b) {
  return a < b ? 0 : a - b;
}
de100_file_scoped_fn inline i32 sat_sub_i32(i32 a, i32 b) {
  if (b < 0 && a > DE100_I32_MAX + b)
    return DE100_I32_MAX;
  if (b > 0 && a < DE100_I32_MIN + b)
    return DE100_I32_MIN;
  return a - b;
}

#define de100_sat_sub(a, b)                                                    \
  _Generic((a), i32: sat_sub_i32, u32: sat_sub_u32)(a, b)

// ═══════════════════════════════════════════════════════════════════════════
// Functions that needs `math.h`
// ═══════════════════════════════════════════════════════════════════════════

#include <math.h>

// Modulo (floating point remainder with same sign as dividend)
de100_file_scoped_fn inline f32 mod_f32(f32 x, f32 y) { return fmodf(x, y); }
de100_file_scoped_fn inline f64 mod_f64(f64 x, f64 y) { return fmod(x, y); }

#define de100_mod(x, y) _Generic((x), f32: mod_f32, f64: mod_f64)(x, y)

de100_file_scoped_fn inline f32 sin_f32(f32 angle) {
  f32 Result = sinf(angle);
  return (Result);
}
de100_file_scoped_fn inline f64 sin_f64(f64 angle) {
  f64 Result = sin(angle);
  return (Result);
}
// Generic macro for `cos`
#define de100_sinf(x) _Generic((x), f32: sin_f32, f64: sin_f64)(x)

de100_file_scoped_fn inline f32 cos_f32(f32 angle) {
  f32 Result = cosf(angle);
  return (Result);
}
de100_file_scoped_fn inline f64 cos_f64(f64 angle) {
  f64 Result = cos(angle);
  return (Result);
}
#define de100_cosf(x) _Generic((x), f32: cos_f32, f64: cos_f64)(x)

de100_file_scoped_fn inline f32 tan_f32(f32 angle) { return tanf(angle); }
de100_file_scoped_fn inline f64 tan_f64(f64 angle) { return tan(angle); }
#define de100_tanf(x) _Generic((x), f32: tan_f32, f64: tan_f64)(x)

de100_file_scoped_fn inline f32 asin_f32(f32 x) { return asinf(x); }
de100_file_scoped_fn inline f64 asin_f64(f64 x) { return asin(x); }
#define de100_asinf(x) _Generic((x), f32: asin_f32, f64: asin_f64)(x)

de100_file_scoped_fn inline f32 acos_f32(f32 x) { return acosf(x); }
de100_file_scoped_fn inline f64 acos_f64(f64 x) { return acos(x); }
#define de100_acosf(x) _Generic((x), f32: acos_f32, f64: acos_f64)(x)

de100_file_scoped_fn inline f32 atan_f32(f32 x) { return atanf(x); }
de100_file_scoped_fn inline f64 atan_f64(f64 x) { return atan(x); }
#define de100_atanf(x) _Generic((x), f32: atan_f32, f64: atan_f64)(x)

de100_file_scoped_fn inline f32 atan2_f32(f32 y, f32 x) { return atan2f(y, x); }
de100_file_scoped_fn inline f64 atan2_f64(f64 y, f64 x) { return atan2(y, x); }
#define de100_atan2f(y, x) _Generic((y), f32: atan2_f32, f64: atan2_f64)(y, x)

de100_file_scoped_fn inline f32 sqrt_f32(f32 x) { return sqrtf(x); }
de100_file_scoped_fn inline f64 sqrt_f64(f64 x) { return sqrt(x); }
#define de100_sqrtf(x) _Generic((x), f32: sqrt_f32, f64: sqrt_f64)(x)

de100_file_scoped_fn inline f32 pow_f32(f32 base, f32 exp) {
  return powf(base, exp);
}
de100_file_scoped_fn inline f64 pow_f64(f64 base, f64 exp) {
  return pow(base, exp);
}
#define de100_powf(base, exp)                                                  \
  _Generic((base), f32: pow_f32, f64: pow_f64)(base, exp)

de100_file_scoped_fn inline f32 exp_f32(f32 x) { return expf(x); }
de100_file_scoped_fn inline f64 exp_f64(f64 x) { return exp(x); }
#define de100_expf(x) _Generic((x), f32: exp_f32, f64: exp_f64)(x)

de100_file_scoped_fn inline f32 log_f32(f32 x) { return logf(x); }
de100_file_scoped_fn inline f64 log_f64(f64 x) { return log(x); }
#define de100_logf(x) _Generic((x), f32: log_f32, f64: log_f64)(x)

de100_file_scoped_fn inline f32 log10_f32(f32 x) { return log10f(x); }
de100_file_scoped_fn inline f64 log10_f64(f64 x) { return log10(x); }
#define de100_log10f(x) _Generic((x), f32: log10_f32, f64: log10_f64)(x)

#endif // DE100_COMMON_MATH_H
