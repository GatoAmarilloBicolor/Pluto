#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pluto {
namespace ipc {

// Information about a running Pluto peer (GUI or CLI).
struct PeerInfo {
	int32_t  team_id = -1;
	std::string signature;     // BApplication signature
	std::string workspace;     // working directory (from scripting, or "?")
	std::string model;         // model in use (from scripting, or "?")
	std::string session_title; // current session title
	bool     busy = false;     // true if processing a turn
	bool     is_gui = false;   // true if a GUI instance
};

// Discover all running Pluto instances (GUI) via BRoster.
// Queries each for workspace/model/busy via the BeOS scripting protocol.
std::vector<PeerInfo> find_peers();

// Send a BMessage to a specific peer (by team_id). Returns true on success.
bool send_to(int32_t team_id, uint32_t what, const std::string& data = "");

// Broadcast a message to all peers.
int broadcast(uint32_t what, const std::string& data = "");

// Send a synchronous query to a peer and wait for a reply (timeout in ms).
// Returns the reply string, or empty on timeout/error.
std::string query(int32_t team_id, uint32_t what,
                  const std::string& data = "",
                  int timeout_ms = 5000);

// Find a peer by team_id or by workspace substring. Returns a PeerInfo
// with team_id == -1 if not found.
PeerInfo find_peer(const std::string& id_or_workspace);

// Check if another Pluto instance is already working in the given
// workspace directory. Returns the PeerInfo of the conflicting peer,
// or a PeerInfo with team_id == -1 if no collision.
PeerInfo check_workspace_collision(const std::string& workspace);

// Query a peer for conversation context about a topic. Uses the
// "Context" scripting property. Returns extracted text (max ~2000 chars)
// or empty string on failure.
std::string query_context(int32_t team_id, const std::string& topic);

// Message constants for IPC.
enum {
	kMsgPeerPing       = 'plPi',  // ping a peer (no data)
	kMsgPeerPong       = 'plPo',  // pong reply
	kMsgPeerPrompt     = 'plPT',  // inject a prompt (data = text)
	kMsgPeerGetInfo    = 'plGI',  // request peer info
	kMsgPeerAsk        = 'plAk',  // /ask: inject prompt into peer conversation
	kMsgPeerDelegate   = 'plDl',  // /delegate: inject prompt, notify when done
};

}  // namespace ipc
}  // namespace pluto
