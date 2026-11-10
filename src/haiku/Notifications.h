// Haiku-native system notifications using BNotification.
// Integrates with Pluto's event system to show native notifications
// when responses complete, errors occur, or routines run.

#pragma once

#include <string>

#ifdef __HAIKU__
#include <Notification.h>
#include <Looper.h>
#include <Message.h>
#include <Locker.h>
#endif

namespace pluto {

// Send a native Haiku system notification.
// On non-Haiku platforms this is a no-op.
// Note: icon parameter is not supported by BNotification (requires BBitmap*).
void Notify(const std::string& title, const std::string& body,
            const std::string& /*icon*/ = "");

// A BLooper-based notification center that queues and dispatches
// notifications serially to avoid flooding the notification server.
// On non-Haiku platforms this is a stub.
class NotificationCenter {
public:
    NotificationCenter();
    ~NotificationCenter();

    // Queue a notification for delivery.
    // Note: icon parameter is not supported by BNotification (requires BBitmap*).
    void Post(const std::string& title, const std::string& body,
              const std::string& /*icon*/ = "");

    // Shut down the notification center cleanly.
    // Call this before program exit to avoid use-after-free.
    void Shutdown();

private:
#ifdef __HAIKU__
    class NotificationLooper : public BLooper {
    public:
        NotificationLooper();
        void MessageReceived(BMessage* msg) override;
    };

    // Thread-safe access to looper (can be null during shutdown)
    std::atomic<NotificationLooper*> fLooper{nullptr};

    // Rate limiting: minimum interval between notifications
    static constexpr bigtime_t kMinInterval = 500000;  // 500ms
    bigtime_t fLastNotificationTime = 0;
    std::string fLastTitle;
    std::string fLastBody;
    BLocker fLock;
#endif
};

}  // namespace pluto
