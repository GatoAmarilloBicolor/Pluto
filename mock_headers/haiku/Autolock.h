#ifndef MOCK_AUTOLOCK_H
#define MOCK_AUTOLOCK_H

#include "Types.h"
#include "Locker.h"

class BLooper;
class BHandler;

class BAutolock {
public:
    explicit BAutolock(BLocker* locker) : fLocker(locker), fLooper(nullptr), fHandler(nullptr), fLocked(false) {
        if (locker) fLocked = locker->Lock();
    }
    explicit BAutolock(BHandler* handler) : fLocker(nullptr), fLooper(handler ? handler->Looper() : nullptr), fHandler(handler), fLocked(false) {
        if (fLooper) fLocked = fLooper->Lock();
    }
    explicit BAutolock(BLooper* looper) : fLocker(nullptr), fLooper(looper), fHandler(nullptr), fLocked(false) {
        if (looper) fLocked = looper->Lock();
    }
    ~BAutolock() { 
        if (fLocked) {
            if (fLocker) fLocker->Unlock();
            else if (fLooper) fLooper->Unlock();
        }
    }
    
    bool IsLocked() const { return fLocked; }
    operator bool() const { return fLocked; }
    
private:
    BLocker* fLocker;
    BLooper* fLooper;
    BHandler* fHandler;
    bool fLocked;
};

#endif