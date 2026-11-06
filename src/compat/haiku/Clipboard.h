/*
 * Clipboard.h for Haiku compatibility
 * Minimal stub for Haiku BClipboard class
 */

#ifndef _CLIPBOARD_H
#define _CLIPBOARD_H

#include <Message.h>

#ifdef __cplusplus

class BClipboard {
public:
    BClipboard(const char* name = NULL);
    virtual ~BClipboard();

    status_t InitCheck() const;
    status_t Commit();
    status_t Revert();
    status_t Clear();

    BMessage* Data() const;
    bool IsEmpty() const;
    bool Lock();
    void Unlock();
    bool LockWithTimeout(bigtime_t timeout);
};

#endif /* __cplusplus */

#endif /* _CLIPBOARD_H */