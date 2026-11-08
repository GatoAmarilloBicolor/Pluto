#ifndef MOCK_LOCKER_H
#define MOCK_LOCKER_H

#include "Types.h"

class BLocker {
public:
    BLocker(const char* name = nullptr) { (void)name; }
    virtual ~BLocker() = default;
    
    bool Lock() { return true; }
    bool Unlock() { return true; }
    bool IsLocked() const { return false; }
    thread_id LockingThread() const { return -1; }
    int32 CountLocks() const { return 0; }
    
    // For condition variables
    void LockWithTimeout(bigtime_t timeout) { (void)timeout; Lock(); }
    bool LockWithTimeout(bigtime_t timeout, bool* locked) { (void)timeout; *locked = Lock(); return *locked; }
};

#endif