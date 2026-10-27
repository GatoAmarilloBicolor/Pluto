#ifndef MOCK_OS_H
#define MOCK_OS_H

#include "Types.h"

// OS.h - basic system calls
inline status_t spawn_thread(void (*func)(void*), const char* name, int32 priority, void* data) {
    (void)func; (void)name; (void)priority; (void)data;
    return -1;
}

inline status_t resume_thread(thread_id id) {
    (void)id;
    return -1;
}

inline status_t kill_thread(thread_id id) {
    (void)id;
    return -1;
}

inline thread_id find_thread(const char* name) {
    (void)name;
    return -1;
}

inline status_t wait_for_thread(thread_id id, status_t* return_value) {
    (void)id; (void)return_value;
    return -1;
}

inline status_t snooze(bigtime_t usecs) {
    (void)usecs;
    return B_OK;
}

inline bigtime_t system_time() {
    return 0;
}

inline bigtime_t real_time_clock() {
    return 0;
}

inline bigtime_t real_time_clock_usecs() {
    return 0;
}

// srand/rand conflict with macOS stdlib.h - use system versions
// inline void srand(int seed) { (void)seed; }
// inline int rand() { return 0; }

#endif