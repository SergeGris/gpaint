
#pragma once

// TODO

#define __def_minmax(type, minmax, op) \
  static inline type                   \
      minmax_##type (type a, type b)   \
  {                                    \
    return a op b ? a : b;             \
  }

__def_minmax (int, max, >=);
__def_minmax (int, min, <=);
__def_minmax (float, max, >=);
__def_minmax (float, min, <=);
