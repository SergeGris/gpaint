
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
