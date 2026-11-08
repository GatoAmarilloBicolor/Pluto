#ifndef MOCK_OS_H
#define MOCK_OS_H

#include "Types.h"

// OS.h - basic system calls
// Thread functions declared in Types.h to avoid overload conflicts
// inline status_t spawn_thread(void (*func)(void*), const char* name, int32 priority, void* data);
// inline status_t resume_thread(thread_id id);
// inline status_t kill_thread(thread_id id);
// inline thread_id find_thread(const char* name);
// inline status_t wait_for_thread(thread_id id, status_t* return_value);

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

// Semaphore functions
typedef int32 sem_id;

inline sem_id create_sem(int32 count, const char* name) {
    (void)count; (void)name;
    return -1;
}

inline status_t delete_sem(sem_id id) {
    (void)id;
    return B_OK;
}

inline status_t acquire_sem(sem_id id) {
    (void)id;
    return B_OK;
}

inline status_t acquire_sem_etc(sem_id id, int32 count, uint32 flags, bigtime_t timeout) {
    (void)id; (void)count; (void)flags; (void)timeout;
    return B_OK;
}

inline status_t release_sem(sem_id id) {
    (void)id;
    return B_OK;
}

inline status_t release_sem_etc(sem_id id, int32 count, uint32 flags) {
    (void)id; (void)count; (void)flags;
    return B_OK;
}

// srand/rand conflict with macOS stdlib.h - use system versions
// inline void srand(int seed) { (void)seed; }
// inline int rand() { return 0; }

#endif