#ifndef MOCK_NODE_MONITOR_H
#define MOCK_NODE_MONITOR_H

#include "Types.h"

enum {
    B_NODE_MONITOR = 'nmon',
    B_WATCH_DIRECTORY = 0x0001,
    B_WATCH_NAME = 0x0002,
    B_WATCH_STAT = 0x0004,
    B_WATCH_ALL = 0x000f
};

struct node_ref {
    int32_t device;
    int64_t node;
    node_ref() : device(0), node(0) {}
    node_ref(int32_t d, int64_t n) : device(d), node(n) {}
};

inline status_t watch_node(const node_ref* node, uint32_t flags, BMessenger target) {
    (void)node; (void)flags; (void)target;
    return B_OK;
}

inline void stop_watching(const node_ref* node, BMessenger target) {
    (void)node; (void)target;
}

#endif
