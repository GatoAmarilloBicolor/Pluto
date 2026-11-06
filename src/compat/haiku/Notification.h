/*
 * Notification.h for Haiku compatibility
 * Minimal stub for Haiku BNotification class
 */

#ifndef _NOTIFICATION_H
#define _NOTIFICATION_H

#include <Handler.h>

#ifdef __cplusplus

class BNotification : public BArchivable {
public:
    BNotification(const char* title, uint32 type = 0);
    BNotification(BMessage* archive);
    virtual ~BNotification();

    virtual status_t Archive(BMessage* archive, bool deep = true) const;
    static BArchivable* Instantiate(BMessage* archive);

    status_t SetTitle(const char* title);
    const char* Title() const;

    status_t SetContent(const char* content);
    const char* Content() const;

    status_t SetGroup(const char* group);
    const char* Group() const;

    status_t SetIcon(const BBitmap* icon);
    const BBitmap* Icon() const;

    status_t Send();
    status_t Update();
    status_t Dismiss();

    bool IsDispatched() const;
};

#endif /* __cplusplus */

#endif /* _NOTIFICATION_H */