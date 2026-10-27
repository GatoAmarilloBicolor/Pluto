#ifndef MOCK_SUPPORT_DEFS_H
#define MOCK_SUPPORT_DEFS_H

#include "Types.h"

// Additional type codes not in Types.h
enum {
    B_BOOL_TYPE       = 1,
    B_INT8_TYPE       = 2,
    B_INT16_TYPE      = 3,
    B_UINT8_TYPE      = 6,
    B_UINT16_TYPE     = 7,
    B_UINT32_TYPE     = 8,
    B_UINT64_TYPE     = 9,
    B_RGB_COLOR_TYPE  = 17,
    B_ANY_TYPE_LAST   = 24
};

#endif // MOCK_SUPPORT_DEFS_H