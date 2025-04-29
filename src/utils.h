
#pragma once

// TODO

#define __def_minmax(type, minmax, op)          \
  static inline type                            \
  minmax##_##type (type a, type b)              \
  {                                             \
    return a op b ? a : b;                      \
  }

#define __def_clamp(type)                       \
  static inline type                            \
  clamp_##type (type x, type min, type max)     \
  {                                             \
    if (x < min)                                \
      return min;                               \
    if (x > max)                                \
      return max;                               \
    return x;                                   \
  }

__def_minmax (int, max, >=);
__def_minmax (int, min, <=);
__def_minmax (double, max, >=);
__def_minmax (double, min, <=);
__def_clamp (int);
__def_clamp (double);

#define BARF_IF_FAIL(e) (sizeof (char [1 - 2 * (!(e) ? 1 : 0)]) - 1)

/* Does the __typeof__ keyword work?  This could be done by
   'configure', but for now it's easier to do it by hand.  */
#if __GNUC__ >= 2 \
 || (__clang_major__ >= 4) \
 || (__IBMC__ >= 1210 && defined (__IBM__TYPEOF__)) \
 || (__SUNPRO_C >= 0x5110 && !__STDC__) \
 || defined (__TINYC__)
# define HAVE___TYPEOF__ 1
#else
# define HAVE___TYPEOF__ 0
#endif

#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)) \
	|| defined(__clang__) || defined(__PCC__) || defined(__TINYC__) \
  && HAVE___TYPEOF__ && !defined (__cplusplus)
# define is_same_type(a, b) __builtin_types_compatible_p(__typeof__(a),	\
							 __typeof__(b))
#else
# define is_same_type(a, b) 0
#endif

#define countof(array) \
  (sizeof (array) / sizeof ((array)[0]) + BARF_IF_FAIL (!is_same_type (array, &array[0])))

/* Warningless cast from const pointer to usual pointer.  */
static inline G_ALWAYS_INLINE void *
const_cast (const void *p)
{
  union
  {
    const void *p;
    void *q;
  } u = { .p = p };

  return u.q;
}
