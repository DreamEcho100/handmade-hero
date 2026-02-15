#ifndef DE100_COMMON_MATH_H
#define DE100_COMMON_MATH_H

#include "base.h"

#if !HAS_C11
#error "C11 support is required for _Generic macros."
#endif

// ═══════════════════════════════════════════════════════════════════════════
// OFFSET CONSTANTS FOR FAST FLOOR/CEIL
// ═══════════════════════════════════════════════════════════════════════════

// For int32: offset must be > max expected |x| and fit in int32
#define DE100_FAST_FLOOR_OFFSET_INT32_F32 32768.0f
#define DE100_FAST_FLOOR_OFFSET_INT32_F64 32768.0

// For uint32: offset must be > max expected |x| and fit in uint32
#define DE100_FAST_FLOOR_OFFSET_UINT32_F32 65536.0f
#define DE100_FAST_FLOOR_OFFSET_UINT32_F64 65536.0

// For round half-away-from-zero
#define DE100_ROUND_HALF_F32 0.5f
#define DE100_ROUND_HALF_F64 0.5

// ═══════════════════════════════════════════════════════════════════════════
// FUNCTION GENERATOR MACROS
// ═══════════════════════════════════════════════════════════════════════════

// Unsafe fast floor: uses offset shift trick. Only safe when |x| < offset.
#define DE100_DEFINE_UNSAFE_FAST_FLOOR(ftype, itype, offset)                   \
  const ftype unsafe_fast_floor_##ftype##_to_##itype##_offset = offset;        \
  de100_file_scoped_fn inline itype unsafe_fast_floor_##ftype##_to_##itype(    \
      ftype x) {                                                               \
    return (itype)(x + offset) - (itype)offset;                                \
  }

// Safe floor: truncate-and-correct, works for all finite values in range.
#define DE100_DEFINE_FLOOR(ftype, itype)                                       \
  de100_file_scoped_fn inline itype floor_##ftype##_to_##itype(ftype x) {      \
    itype i = (itype)x;                                                        \
    return i - (i > x);                                                        \
  }

// Unsafe fast ceil: negated floor trick.
#define DE100_DEFINE_UNSAFE_FAST_CEIL(ftype, itype, offset)                    \
  const ftype unsafe_fast_ceil_##ftype##_to_##itype##_offset = offset;         \
  de100_file_scoped_fn inline itype unsafe_fast_ceil_##ftype##_to_##itype(     \
      ftype x) {                                                               \
    return -((itype)(-x + offset) - (itype)offset);                            \
  }

// Safe ceil: truncate-and-correct.
#define DE100_DEFINE_CEIL(ftype, itype)                                        \
  de100_file_scoped_fn inline itype ceil_##ftype##_to_##itype(ftype x) {       \
    itype i = (itype)x;                                                        \
    return i + (i < x);                                                        \
  }

// Round: half away from zero.
#define DE100_DEFINE_ROUND(ftype, itype, half)                                 \
  de100_file_scoped_fn inline itype round_##ftype##_to_##itype(ftype x) {      \
    return (itype)(x + (x > 0 ? half : -half));                                \
  }

// ═══════════════════════════════════════════════════════════════════════════
// FLOOR FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

DE100_DEFINE_UNSAFE_FAST_FLOOR(f32, int32, DE100_FAST_FLOOR_OFFSET_INT32_F32)
DE100_DEFINE_UNSAFE_FAST_FLOOR(f32, uint32, DE100_FAST_FLOOR_OFFSET_UINT32_F32)
DE100_DEFINE_UNSAFE_FAST_FLOOR(f64, int32, DE100_FAST_FLOOR_OFFSET_INT32_F64)
DE100_DEFINE_UNSAFE_FAST_FLOOR(f64, uint32, DE100_FAST_FLOOR_OFFSET_UINT32_F64)

DE100_DEFINE_FLOOR(f32, int32)
DE100_DEFINE_FLOOR(f32, uint32)
DE100_DEFINE_FLOOR(f64, int32)
DE100_DEFINE_FLOOR(f64, uint32)

#define de100_floorf_to_int32(x)                                               \
  _Generic((x), f32: floor_f32_to_int32, f64: floor_f64_to_int32)(x)
#define de100_floorf_to_uint32(x)                                              \
  _Generic((x), f32: floor_f32_to_uint32, f64: floor_f64_to_uint32)(x)

// ═══════════════════════════════════════════════════════════════════════════
// CEIL FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

DE100_DEFINE_UNSAFE_FAST_CEIL(f32, int32, DE100_FAST_FLOOR_OFFSET_INT32_F32)
DE100_DEFINE_UNSAFE_FAST_CEIL(f32, uint32, DE100_FAST_FLOOR_OFFSET_UINT32_F32)
DE100_DEFINE_UNSAFE_FAST_CEIL(f64, int32, DE100_FAST_FLOOR_OFFSET_INT32_F64)
DE100_DEFINE_UNSAFE_FAST_CEIL(f64, uint32, DE100_FAST_FLOOR_OFFSET_UINT32_F64)

DE100_DEFINE_CEIL(f32, int32)
DE100_DEFINE_CEIL(f32, uint32)
DE100_DEFINE_CEIL(f64, int32)
DE100_DEFINE_CEIL(f64, uint32)

#define de100_ceilf_to_int32(x)                                                \
  _Generic((x), f32: ceil_f32_to_int32, f64: ceil_f64_to_int32)(x)
#define de100_ceilf_to_uint32(x)                                               \
  _Generic((x), f32: ceil_f32_to_uint32, f64: ceil_f64_to_uint32)(x)

// ═══════════════════════════════════════════════════════════════════════════
// ROUND FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

DE100_DEFINE_ROUND(f32, int32, DE100_ROUND_HALF_F32)
DE100_DEFINE_ROUND(f32, uint32, DE100_ROUND_HALF_F32)
DE100_DEFINE_ROUND(f64, int32, DE100_ROUND_HALF_F64)
DE100_DEFINE_ROUND(f64, uint32, DE100_ROUND_HALF_F64)

#define de100_roundf_to_int32(x)                                               \
  _Generic((x), f32: round_f32_to_int32, f64: round_f64_to_int32)(x)
#define de100_roundf_to_uint32(x)                                              \
  _Generic((x), f32: round_f32_to_uint32, f64: round_f64_to_uint32)(x)

// ═══════════════════════════════════════════════════════════════════════════
// _IN PROGRESS_ FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

#include <math.h>

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
  f64 Result = cosf(angle);
  return (Result);
}
#define de100_cosf(x) _Generic((x), f32: cos_f32, f64: cos_f64)(x)

de100_file_scoped_fn inline f32 atan2_f32(f32 y, f32 x) {
  f32 Result = atan2f(y, x);
  return (Result);
}
de100_file_scoped_fn inline f64 atan2_f64(f64 y, f64 x) {
  f64 Result = atan2(y, x);
  return (Result);
}
#define de100_atan2f(y, x) _Generic((y), f32: atan2_f32, f64: atan2_f64)(y, x)

de100_file_scoped_fn inline f32 fabs_f32(f32 x) {
  f32 Result = fabsf(x);
  return (Result);
}
de100_file_scoped_fn inline f64 fabs_f64(f64 x) {
  f64 Result = fabs(x);
  return (Result);
}
#define de100_fabsf(x) _Generic((x), f32: fabs_f32, f64: fabs_f64)(x)

#endif // DE100_COMMON_MATH_H
