// Notifications.cpp — Haiku-native BNotification support.

#include "Notifications.h"

#ifdef __HAIKU__
#include <Application.h>
#include <Notification.h>
#include <string.h>
#include <TranslationUtils.h>
#include <stdio.h>
#include <memory>
#endif

#include <cstdio>
#include <chrono>

namespace pluto {

#ifdef __HAIKU__

void Notify(const std::string& title, const std::string& body,
            const std::string& icon) {
    BNotification notification(B_INFORMATION_NOTIFICATION);
    notification.SetTitle(title.c_str());
    notification.SetContent(body.c_str());
    
    // Load icon from file if provided
    if (!icon.empty()) {
        BBitmap* bitmap = BTranslationUtils::GetBitmapFile(icon.c_str(), nullptr);
        if (bitmap) {
            notification.SetIcon(bitmap);
            // notification takes ownership of bitmap, don't delete
        }
    }
    
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
        BString title, body, iconPath;
        msg->FindString("title", &title);
        msg->FindString("body", &body);
        msg->FindString("icon", &iconPath);

        BNotification notification(B_INFORMATION_NOTIFICATION);
        notification.SetTitle(title);
        notification.SetContent(body);
        // Load icon from file if provided in message
        if (iconPath.Length() > 0) {
            BBitmap* bitmap = BTranslationUtils::GetBitmapFile(iconPath.String(), nullptr);
            if (bitmap) {
                notification.SetIcon(bitmap);
                // notification takes ownership of bitmap, don't delete
            }
        }
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
    // Store raw pointer in atomic - we manage lifetime manually with Shutdown()
    fLooper.store(new NotificationLooper(), std::memory_order_release);
}

NotificationCenter::~NotificationCenter() {
    Shutdown();
}

void NotificationCenter::Shutdown() {
    NotificationLooper* looper = fLooper.exchange(nullptr, std::memory_order_acq_rel);
    if (looper) {
        // Properly shut down the looper
        looper->Lock();
        looper->Quit();
        // Looper unlocks itself when exiting Run()
        delete looper;
    }
}

void NotificationCenter::Post(const std::string& title,
                               const std::string& body,
                               const std::string& icon) {
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
    if (!icon.empty()) {
        msg.AddString("icon", icon.c_str());
    }

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

#endif

} // namespace pluto