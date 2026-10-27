// Notifications.cpp — Haiku-native BNotification support.

#include "Notifications.h"

#ifdef __HAIKU__
#include <Application.h>
#include <Notification.h>
#include <String.h>
#endif

#include <cstdio>

namespace pluto {

#ifdef __HAIKU__

void Notify(const std::string& title, const std::string& body,
            const std::string& /*icon*/) {
	BNotification notification(B_INFORMATION_NOTIFICATION);
	notification.SetTitle(title.c_str());
	notification.SetContent(body.c_str());
	notification.Send();
}

NotificationCenter::NotificationCenter() {
	fLooper = new NotificationLooper();
}

NotificationCenter::~NotificationCenter() {
	if (fLooper) {
		fLooper->Lock();
		fLooper->Quit();
		fLooper->Unlock();
		delete fLooper;
		fLooper = nullptr;
	}
}

void NotificationCenter::Post(const std::string& title,
                               const std::string& body,
                               const std::string& icon) {
	if (!fLooper) return;

	BMessage msg(static_cast<uint32>(1));
	msg.AddString("title", title.c_str());
	msg.AddString("body", body.c_str());
	if (!icon.empty()) msg.AddString("icon", icon.c_str());

	fLooper->PostMessage(&msg);
}

void NotificationCenter::NotificationLooper::MessageReceived(BMessage* msg) {
	if (msg->what == static_cast<uint32>(1)) {
		BString title, body;
		msg->FindString("title", &title);
		msg->FindString("body", &body);

		BNotification notification(B_INFORMATION_NOTIFICATION);
		notification.SetTitle(title);
		notification.SetContent(body);
		notification.Send();
	} else {
		BLooper::MessageReceived(msg);
	}
}

#else  // __HAIKU__

void Notify(const std::string& /*title*/, const std::string& /*body*/,
            const std::string& /*icon*/) {
	// No-op on non-Haiku platforms.
}

NotificationCenter::NotificationCenter() {}
NotificationCenter::~NotificationCenter() {}
void NotificationCenter::Post(const std::string&, const std::string&,
                              const std::string&) {}

#endif  // __HAIKU__

}  // namespace pluto
