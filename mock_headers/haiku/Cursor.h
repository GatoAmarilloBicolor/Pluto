#ifndef MOCK_CURSOR_H
#define MOCK_CURSOR_H

#include "Types.h"

class BCursor {
public:
    BCursor() = default;
    virtual ~BCursor() = default;
    
    static BCursor* SystemCursor(int32 cursor) {
        (void)cursor;
        return nullptr;
    }
};

const int32 B_CURSOR_SYSTEM_DEFAULT = 0;
const int32 B_CURSOR_I_BEAM = 1;

#endif