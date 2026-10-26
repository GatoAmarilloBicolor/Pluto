#ifndef MOCK_NOTIFICATION_H
#define MOCK_NOTIFICATION_H

#include "Message.h"
#include "BString.h"

#define B_NOTIFICATION_TYPE_INFO 1
#define B_NOTIFICATION_TYPE_WARNING 2
#define B_NOTIFICATION_TYPE_ERROR 3

class BNotification {
public:
    BNotification(int32_t type, const BString& title, const BString& content) {
        (void)type; (void)title; (void)content;
    }
    ~BNotification() = default;
    
    void Send() {}
};

#endif
