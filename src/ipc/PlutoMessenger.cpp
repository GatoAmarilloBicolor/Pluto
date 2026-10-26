#include "PlutoMessenger.h"

#ifdef __HAIKU__


#include <Application.h>
#include <List.h>
#include <Messenger.h>
#include <Roster.h>

#include <cstdio>
#include <cstring>

namespace pluto {
namespace ipc {

// Base signature prefix. Multi-instance signatures are "...-<pid>".
static const char* kSigPrefix = "application/x-vnd.Hay-pluto";

std::vector<PeerInfo> find_peers() {
	std::vector<PeerInfo> peers;

	// Get the list of all running apps and filter by our signature prefix.
	BList teamList;
	be_roster->GetAppList(&teamList);

	for (int32 i = 0; i < teamList.CountItems(); ++i) {
		team_id tid = (team_id)(intptr_t)teamList.ItemAt(i);
		app_info info;
		if (be_roster->GetRunningAppInfo(tid, &info) != B_OK)
			continue;

		// Check if this is a Pluto instance (signature starts with prefix).
		if (std::strncmp(info.signature, kSigPrefix, std::strlen(kSigPrefix)) != 0)
			continue;

		PeerInfo peer;
		peer.team_id  = info.team;
		peer.signature = info.signature;
		peer.is_gui   = true;

		// Try to query this peer for workspace/model via BMessage scripting.
		BMessenger msger(info.signature);
		if (msger.IsValid()) {
			auto get_string = [&](const char* prop) -> std::string {
				BMessage request(B_GET_PROPERTY);
				request.AddSpecifier(prop);
				BMessage reply;
				if (msger.SendMessage(&request, &reply, 2000000, 2000000) == B_OK) {
					const char* val = nullptr;
					if (reply.FindString("result", &val) == B_OK && val)
						return val;
				}
				return "";
			};

			peer.workspace     = get_string("Workspace");
			peer.model         = get_string("Model");
			peer.session_title = get_string("SessionTitle");

			{
				BMessage request(B_GET_PROPERTY);
				request.AddSpecifier("Busy");
				BMessage reply;
				if (msger.SendMessage(&request, &reply, 2000000, 2000000) == B_OK) {
					bool val = false;
					if (reply.FindBool("result", &val) == B_OK)
						peer.busy = val;
				}
			}
		}

		peers.push_back(std::move(peer));
	}

	return peers;
}

bool send_to(int32_t team_id, uint32_t what, const std::string& data) {
	// Find the signature for this team.
	app_info info;
	if (be_roster->GetRunningAppInfo(team_id, &info) != B_OK)
		return false;

	BMessenger msger(info.signature);
	if (!msger.IsValid()) return false;

	BMessage msg(what);
	if (!data.empty())
		msg.AddString("data", data.c_str());
	return msger.SendMessage(&msg) == B_OK;
}

int broadcast(uint32_t what, const std::string& data) {
	auto peers = find_peers();
	int sent = 0;
	team_id self = -1;

	// Don't send to ourselves.
	if (be_app) {
		app_info selfInfo;
		if (be_app->GetAppInfo(&selfInfo) == B_OK)
			self = selfInfo.team;
	}

	for (const auto& p : peers) {
		if (p.team_id == self) continue;
		if (send_to(p.team_id, what, data))
			++sent;
	}
	return sent;
}

std::string query(int32_t team_id, uint32_t what,
                  const std::string& data, int timeout_ms) {
	app_info info;
	if (be_roster->GetRunningAppInfo(team_id, &info) != B_OK)
		return "";

	BMessenger msger(info.signature);
	if (!msger.IsValid()) return "";

	BMessage msg(what);
	if (!data.empty())
		msg.AddString("data", data.c_str());

	BMessage reply;
	bigtime_t timeout = (bigtime_t)timeout_ms * 1000;
	if (msger.SendMessage(&msg, &reply, timeout, timeout) != B_OK)
		return "";

	const char* result = nullptr;
	if (reply.FindString("result", &result) == B_OK && result)
		return result;

	return "";
}

PeerInfo find_peer(const std::string& id_or_workspace) {
	auto peers = find_peers();

	// Try numeric (team_id) first.
	bool is_numeric = !id_or_workspace.empty();
	for (char c : id_or_workspace) {
		if (c < '0' || c > '9') { is_numeric = false; break; }
	}
	if (is_numeric) {
		int32_t tid = std::atoi(id_or_workspace.c_str());
		for (const auto& p : peers) {
			if (p.team_id == tid) return p;
		}
	}

	// Try workspace substring match (case-sensitive).
	for (const auto& p : peers) {
		if (!p.workspace.empty()
		    && p.workspace.find(id_or_workspace) != std::string::npos)
			return p;
	}

	return PeerInfo{};  // team_id == -1 → not found
}

PeerInfo check_workspace_collision(const std::string& workspace) {
	if (workspace.empty()) return PeerInfo{};

	auto peers = find_peers();

	// Exclude ourselves (CLI has no BApplication, so we skip by checking
	// that this peer is a GUI instance with a different team).
	team_id self = -1;
	if (be_app) {
		app_info selfInfo;
		if (be_app->GetAppInfo(&selfInfo) == B_OK)
			self = selfInfo.team;
	}

	for (const auto& p : peers) {
		if (p.team_id == self) continue;
		if (p.workspace == workspace) return p;
	}
	return PeerInfo{};
}

std::string query_context(int32_t team_id, const std::string& topic) {
	if (team_id < 0 || topic.empty()) return {};

	BMessenger msger(nullptr, team_id);
	if (!msger.IsValid()) return {};

	// Build a B_GET_PROPERTY for "Context" with the topic as extra data.
	BMessage request(B_GET_PROPERTY);
	request.AddSpecifier("Context");
	request.AddString("data", topic.c_str());

	BMessage reply;
	status_t err = msger.SendMessage(&request, &reply, 5000000, 5000000);
	if (err != B_OK) return {};

	const char* result = nullptr;
	if (reply.FindString("result", &result) == B_OK && result)
		return std::string(result);
	return {};
}

}  // namespace ipc
}  // namespace pluto


#else  // __HAIKU__

namespace pluto {
namespace ipc {

// Forward declare Haiku types for non-Haiku builds
using team_id = int32_t;
using int32 = int32_t;
struct Message {};  // minimal stub

std::vector<PeerInfo> find_peers() { return {}; }
bool send_message(team_id, const Message&) { return false; }
bool send_reply(int32, const Message&) { return false; }

}  // namespace ipc
}  // namespace pluto

#endif  // __HAIKU__
