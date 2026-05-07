// tests/test_ipc.cpp — PlutoMessenger IPC unit tests
//
// Tests:
// 1. PeerInfo struct default values
// 2. find_peers returns a vector (may be empty if no GUI running)
// 3. send_to with invalid team_id returns false
// 4. query with invalid team_id returns empty
// 5. broadcast with no peers returns 0
// 6. Message constants are distinct
// 7. PeerInfo fields are assignable

#include "ipc/PlutoMessenger.h"

#include <Application.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

static int g_pass = 0;
static int g_total = 0;

#define TEST(name) \
	do { ++g_total; std::printf("  %d. %s ... ", g_total, name); } while(0)
#define PASS() do { ++g_pass; std::printf("ok\n"); } while(0)
#define ASSERT(cond) do { \
	if (!(cond)) { \
		std::printf("FAIL (%s:%d: %s)\n", __FILE__, __LINE__, #cond); \
		return; \
	} \
} while(0)

// We need a BApplication for BRoster to work.
static BApplication* sApp = nullptr;

void test_default_values() {
	TEST("PeerInfo default values");
	pluto::ipc::PeerInfo p;
	ASSERT(p.team_id == -1);
	ASSERT(p.signature.empty());
	ASSERT(p.workspace.empty());
	ASSERT(p.model.empty());
	ASSERT(p.session_title.empty());
	ASSERT(p.busy == false);
	ASSERT(p.is_gui == false);
	PASS();
}

void test_find_peers() {
	TEST("find_peers returns vector");
	auto peers = pluto::ipc::find_peers();
	// May or may not find peers depending on whether GUI is running.
	// The important thing is it doesn't crash.
	(void)peers;
	PASS();
}

void test_send_to_invalid() {
	TEST("send_to invalid team_id");
	bool ok = pluto::ipc::send_to(-1, pluto::ipc::kMsgPeerPing);
	ASSERT(!ok);
	ok = pluto::ipc::send_to(999999, pluto::ipc::kMsgPeerPing);
	ASSERT(!ok);
	PASS();
}

void test_query_invalid() {
	TEST("query invalid team_id");
	std::string result = pluto::ipc::query(-1, pluto::ipc::kMsgPeerGetInfo);
	ASSERT(result.empty());
	result = pluto::ipc::query(999999, pluto::ipc::kMsgPeerGetInfo, "", 100);
	ASSERT(result.empty());
	PASS();
}

void test_broadcast_no_peers() {
	TEST("broadcast with no Pluto peers");
	// This might actually send to running Pluto instances, but at worst
	// it sends a harmless ping. The return value is the count sent.
	int sent = pluto::ipc::broadcast(pluto::ipc::kMsgPeerPing);
	// Can't assert a specific value since we don't control running apps.
	(void)sent;
	PASS();
}

void test_message_constants() {
	TEST("message constants are distinct");
	ASSERT(pluto::ipc::kMsgPeerPing != pluto::ipc::kMsgPeerPong);
	ASSERT(pluto::ipc::kMsgPeerPing != pluto::ipc::kMsgPeerPrompt);
	ASSERT(pluto::ipc::kMsgPeerPing != pluto::ipc::kMsgPeerGetInfo);
	ASSERT(pluto::ipc::kMsgPeerPong != pluto::ipc::kMsgPeerPrompt);
	// New: kMsgPeerAsk and kMsgPeerDelegate are distinct from each other
	// and from all existing constants.
	ASSERT(pluto::ipc::kMsgPeerAsk != pluto::ipc::kMsgPeerDelegate);
	ASSERT(pluto::ipc::kMsgPeerAsk != pluto::ipc::kMsgPeerPing);
	ASSERT(pluto::ipc::kMsgPeerAsk != pluto::ipc::kMsgPeerPrompt);
	ASSERT(pluto::ipc::kMsgPeerDelegate != pluto::ipc::kMsgPeerPing);
	ASSERT(pluto::ipc::kMsgPeerDelegate != pluto::ipc::kMsgPeerPrompt);
	PASS();
}

void test_peer_info_fields() {
	TEST("PeerInfo fields assignable");
	pluto::ipc::PeerInfo p;
	p.team_id = 42;
	p.signature = "application/x-vnd.test";
	p.workspace = "/boot/home/test";
	p.model = "claude-opus-4-5";
	p.session_title = "My session";
	p.busy = true;
	p.is_gui = true;

	ASSERT(p.team_id == 42);
	ASSERT(p.signature == "application/x-vnd.test");
	ASSERT(p.workspace == "/boot/home/test");
	ASSERT(p.model == "claude-opus-4-5");
	ASSERT(p.session_title == "My session");
	ASSERT(p.busy == true);
	ASSERT(p.is_gui == true);
	PASS();
}

void test_find_peer_not_found() {
	TEST("find_peer returns empty for non-existent peer");
	auto p = pluto::ipc::find_peer("999999");
	ASSERT(p.team_id == -1);
	p = pluto::ipc::find_peer("nonexistent-workspace-abc");
	ASSERT(p.team_id == -1);
	PASS();
}

void test_find_peer_numeric() {
	TEST("find_peer accepts numeric team_id string");
	// With an invalid team_id, should return not-found.
	auto p = pluto::ipc::find_peer("0");
	// team_id 0 is unlikely to be a Pluto instance.
	// Either not found (-1) or found with some workspace.
	// The function should not crash.
	(void)p;
	PASS();
}

void test_check_workspace_collision_empty() {
	TEST("check_workspace_collision with empty path");
	auto p = pluto::ipc::check_workspace_collision("");
	ASSERT(p.team_id == -1);
	PASS();
}

void test_check_workspace_collision_unique() {
	TEST("check_workspace_collision with unique path");
	// A path that no other Pluto instance is using.
	auto p = pluto::ipc::check_workspace_collision(
		"/boot/home/nonexistent-test-path-12345");
	ASSERT(p.team_id == -1);
	PASS();
}

void test_send_ask_invalid() {
	TEST("send kMsgPeerAsk to invalid peer fails");
	bool ok = pluto::ipc::send_to(-1, pluto::ipc::kMsgPeerAsk, "hello");
	ASSERT(!ok);
	ok = pluto::ipc::send_to(999999, pluto::ipc::kMsgPeerAsk, "hello");
	ASSERT(!ok);
	PASS();
}

void test_send_delegate_invalid() {
	TEST("send kMsgPeerDelegate to invalid peer fails");
	bool ok = pluto::ipc::send_to(-1, pluto::ipc::kMsgPeerDelegate, "hello");
	ASSERT(!ok);
	PASS();
}

int main() {
	// BRoster requires a BApplication context.
	sApp = new BApplication("application/x-vnd.pluto-test-ipc");

	std::printf("test_ipc\n");
	test_default_values();
	test_find_peers();
	test_send_to_invalid();
	test_query_invalid();
	test_broadcast_no_peers();
	test_message_constants();
	test_peer_info_fields();
	test_find_peer_not_found();
	test_find_peer_numeric();
	test_check_workspace_collision_empty();
	test_check_workspace_collision_unique();
	test_send_ask_invalid();
	test_send_delegate_invalid();
	std::printf("%d/%d passed\n", g_pass, g_total);

	delete sApp;
	return g_pass == g_total ? 0 : 1;
}
