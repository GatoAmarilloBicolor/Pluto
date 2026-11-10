// Notifications.cpp — Haiku-native BNotification support.

#include "Notifications.h"

#ifdef __HAIKU__
#include <Application.h>
#include <Notification.h>
#include <String.h>
#include <stdio.h>
#include <memory>
#endif

#include <cstdio>

namespace pluto {

#ifdef __HAIKU__

void Notify(const std::string& title, const std::string& body,
            const std::string& /*icon*/) {
    // Note: BNotification::SetIcon() only accepts BBitmap*, not a string path.
    // For cross-compilation, we ignore the icon parameter.
    BNotification notification(B_INFORMATION_NOTIFICATION);
    notification.SetTitle(title.c_str());
    notification.SetContent(body.c_str());
    status_t err = notification.Send();
    if (err != B_OK) {
        fprintf(stderr, "[pluto] notification send failed: %s\n", strerror(err));
    }
}

NotificationCenter::NotificationLooper::NotificationLooper()
    : BLooper("pluto-notifications") {
    Run();
}

void NotificationCenter::NotificationLooper::MessageReceived(BMessage* msg) {
    if (msg->what == static_cast<uint32>(1)) {
        BString title, body;
        msg->FindString("title", &title);
        msg->FindString("body", &body);

        BNotification notification(B_INFORMATION_NOTIFICATION);
        notification.SetTitle(title);
        notification.SetContent(body);
        // Note: icon parameter is ignored since BNotification::SetIcon() requires BBitmap*
        status_t err = notification.Send();
        if (err != B_OK) {
            fprintf(stderr, "[pluto] notification send failed: %s\n", strerror(err));
        }
    } else {
        BLooper::MessageReceived(msg);
    }
}

NotificationCenter::NotificationCenter() {
    fLock = std::make_unique<BLocker>("notification-lock");
    fLooper.store(new NotificationLooper(), std::memory_order_release);
}

NotificationCenter::~NotificationCenter() {
    Shutdown();
}

void NotificationCenter::Shutdown() {
    NotificationLooper* looper = fLooper.exchange(nullptr, std::memory_order_acq_rel);
    if (looper) {
        // Quit the looper and wait for thread to finish
        looper->Lock();
        looper->Quit();
        // Wait for the looper thread to finish (it will unlock itself when exiting Run())
        // We don't call Unlock() here - the looper unlocks itself when exiting Run()
        delete looper;
    }
}

void NotificationCenter::Post(const std::string& title,
                               const std::string& body,
                               const std::string& /*icon*/) {
    NotificationLooper* looper = fLooper.load(std::memory_order_acquire);
    if (!looper) return;

    // Rate limiting + deduplication
    bigtime_t now = system_time();
    if (fLock) {
        if (fLock->Lock()) {
            bigtime_t elapsed = now - fLastNotificationTime;
            if (elapsed < kMinInterval &&
                fLastTitle == title && fLastBody == body) {
                fLock->Unlock();
                return;  // Skip duplicate within rate limit window
            }
            fLastNotificationTime = now;
            fLastTitle = title;
            fLastBody = body;
            fLock->Unlock();
        }
    }

    BMessage msg(static_cast<uint32>(1));
    msg.AddString("title", title.c_str());
    msg.AddString("body", body.c_str());
    // Note: icon parameter is not passed since BNotification::SetIcon() requires BBitmap*

    looper->PostMessage(&msg);
}

#else  // __HAIKU__

void Notify(const std::string& /*title*/, const std::string& /*body*/,
            const std::string& /*icon*/) {
    // No-op on non-Haiku platforms.
}

NotificationCenter::NotificationCenter() {}
NotificationCenter::~NotificationCenter() {}
void NotificationCenter::Shutdown() {}
void NotificationCenter::Post(const std::string&, const std::string&,
                              const std::string&) {}

#endif  // __HAIKU__

}  // namespace pluto
