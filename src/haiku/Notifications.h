// Haiku-native system notifications using BNotification.
// Integrates with Pluto's event system to show native notifications
// when responses complete, errors occur, or routines run.

#pragma once

#include <string>

#ifdef __HAIKU__
#include <Notification.h>
#include <Looper.h>
#endif

namespace pluto {

// Send a native Haiku system notification.
// On non-Haiku platforms this is a no-op.
void Notify(const std::string& title, const std::string& body,
            const std::string& icon = "");

// A BLooper-based notification center that queues and dispatches
// notifications serially to avoid flooding the notification server.
// On non-Haiku platforms this is a stub.
class NotificationCenter {
public:
	NotificationCenter();
	~NotificationCenter();

	// Queue a notification for delivery.
	void Post(const std::string& title, const std::string& body,
	          const std::string& icon = "");

private:
#ifdef __HAIKU__
	class NotificationLooper : public BLooper {
	public:
		NotificationLooper() : BLooper("pluto-notifications") { Run(); }
		void MessageReceived(BMessage* msg) override;
	};
	NotificationLooper* fLooper = nullptr;
#endif
};

}  // namespace pluto
