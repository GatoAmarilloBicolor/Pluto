#ifndef MOCK_NOTIFICATION_H
#define MOCK_NOTIFICATION_H

#include "Message.h"
#include "BString.h"
#include "Bitmap.h"

#define B_INFORMATION_NOTIFICATION 1
#define B_IMPORTANT_NOTIFICATION 2
#define B_ERROR_NOTIFICATION 3
#define B_PROGRESS_NOTIFICATION 4

class BMessenger;

class BNotification {
public:
    BNotification(int32_t type) : fType(type) { (void)type; }
    ~BNotification() = default;
    
    // Match real Haiku API: Send() with default timeout parameter
    status_t Send(bigtime_t timeout = -1) { (void)timeout; return B_OK; }
    void SetTitle(const BString& title) { (void)title; }
    void SetContent(const BString& content) { (void)content; }
    void SetGroup(const BString& group) { (void)group; }
    void SetIcon(const BBitmap* icon) { (void)icon; }
    void SetMessageID(const BString& id) { (void)id; }
    void SetOnClickApp(const BString& signature) { (void)signature; }
    void SetProgress(float progress) { (void)progress; }

private:
    int32_t fType;
};

#endif
