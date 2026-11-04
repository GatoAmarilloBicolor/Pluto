#ifndef MOCK_OS_DEFS_H
#define MOCK_OS_DEFS_H

#include "Types.h"

inline void snooze(bigtime_t microseconds) {
    (void)microseconds;
}

#endif
