#ifndef MOCK_NOTIFICATION_H
#define MOCK_NOTIFICATION_H

#include "Message.h"
#include "BString.h"
#include "Bitmap.h"

#define B_INFORMATION_NOTIFICATION 1
#define B_WARNING_NOTIFICATION 2
#define B_ERROR_NOTIFICATION 3

class BNotification {
public:
    BNotification(int32_t type) : fType(type) { (void)type; }
    ~BNotification() = default;
    
    void Send() {}
    void SetTitle(const BString& title) { (void)title; }
    void SetContent(const BString& content) { (void)content; }
    void SetGroup(const BString& group) { (void)group; }
    void SetIcon(const BBitmap* icon) { (void)icon; }

private:
    int32_t fType;
};

#endif
