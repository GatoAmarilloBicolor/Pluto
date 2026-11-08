#ifndef MOCK_INTERFACE_DEFS_H
#define MOCK_INTERFACE_DEFS_H

#include "Types.h"
#include "Rect.h"
#include "String.h"

enum drawing_mode {
    B_OP_COPY = 0,
    B_OP_OVER = 1,
    B_OP_ERASE = 2,
    B_OP_SELECT = 3,
    B_OP_MIN = 4,
    B_OP_MAX = 5,
    B_OP_ADD = 6,
    B_OP_SUBTRACT = 7,
    B_OP_BLEND = 8,
    B_OP_ALPHA = 9
};

// BField, BStringField, BBitmapField are defined in ColumnListView.h

// Color constants (for ui_color) - using existing color_which typedef from Types.h
inline rgb_color ui_color(color_which which) {
    (void)which;
    return { 255, 255, 255, 255 };
}

#endif

