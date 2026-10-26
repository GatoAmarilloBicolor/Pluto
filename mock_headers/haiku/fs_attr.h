#ifndef MOCK_FS_ATTR_H
#define MOCK_FS_ATTR_H

#include "Types.h"

inline ssize_t fs_read_attr(int fd, const char* name, uint32_t type, off_t pos, void* buf, size_t len) {
    (void)fd; (void)name; (void)type; (void)pos; (void)buf; (void)len;
    return 0;
}

inline ssize_t fs_write_attr(int fd, const char* name, uint32_t type, off_t pos, const void* buf, size_t len) {
    (void)fd; (void)name; (void)type; (void)pos; (void)buf; (void)len;
    return 0;
}

inline ssize_t fs_remove_attr(int fd, const char* name) {
    (void)fd; (void)name;
    return 0;
}

inline int fs_list_attr(int fd, char* buf, size_t len) {
    (void)fd; (void)buf; (void)len;
    return 0;
}

inline status_t fs_create_index(dev_t dev, const char* name, uint32_t type, int flags) {
    (void)dev; (void)name; (void)type; (void)flags;
    return B_OK;
}

#endif
