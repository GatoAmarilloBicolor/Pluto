#ifndef MOCK_INTERFACE_DEFS_H
#define MOCK_INTERFACE_DEFS_H

#include "Types.h"
#include "Rect.h"

enum color_space {
    B_RGB32 = 0,
    B_RGB16 = 1,
    B_RGB15 = 2,
    B_CMAP8 = 3,
    B_GRAY8 = 4,
    B_NO_COLOR_SPACE = 5
};

enum drawing_mode {
    B_OP_COPY = 0,
    B_OP_OVER = 1,
    B_OP_ERASE = 2,
    B_OP_SELECT = 3,
    B_OP_MIN = 4,
    B_OP_MAX = 5,
    B_OP_ADD = 6,
    B_OP_SUBTRACT = 7,
    B_OP_BLEND = 8
};

#endif