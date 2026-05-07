#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../provider/Provider.h"

namespace pluto {

// Metadata persisted as BFS extended attributes on a session file.
struct SessionMetadata {
	std::string path;          // absolute path
	std::string filename;      // basename
	std::string title;         // pluto:title
	std::string model;         // pluto:model
	std::string tags;          // pluto:tags (comma-separated)
	int64_t     tokens_in   = 0;   // pluto:tokens_in
	int64_t     tokens_out  = 0;   // pluto:tokens_out
	int64_t     created     = 0;   // pluto:created (unix)
	int64_t     updated     = 0;   // pluto:updated (unix)
	int32_t     turns       = 0;   // pluto:turns
};

// File-based session storage. Sessions live under
// ~/config/settings/pluto/sessions/ — one JSONL file per session, with
// metadata stored as BFS extended attributes on the file.
class SessionStore {
public:
	static std::string sessions_dir();
	static bool ensure_sessions_dir();

	// Override the sessions directory (test hook). Empty string clears.
	static void set_dir_override(const std::string& path);

	// Save a session. If `path` is empty, a new path is generated from
	// title + unix timestamp. Returns the final path, or empty on error.
	static std::string save(std::string path,
	                        const std::string& title,
	                        const std::string& model,
	                        int64_t tokens_in,
	                        int64_t tokens_out,
	                        const std::vector<Message>& messages,
	                        std::string& error);

	// Load a session from disk. Fills both the message list and metadata.
	static bool load(const std::string& path,
	                 std::vector<Message>& out,
	                 SessionMetadata& meta,
	                 std::string& error);

	// List all sessions in the store, sorted by updated timestamp desc.
	static std::vector<SessionMetadata> list();

	// Most recently updated session (empty path if none exist).
	static SessionMetadata most_recent();

	// Remove a session file.
	static bool remove(const std::string& path, std::string& error);

	// Find a session by case-insensitive title substring. Returns the
	// first match (empty metadata if none).
	static SessionMetadata find_by_title(const std::string& query);

	// Tag management (comma-separated list in pluto:tags).
	static bool add_tag(const std::string& path, const std::string& tag,
	                    std::string& error);
	static bool remove_tag(const std::string& path, const std::string& tag,
	                       std::string& error);
	static std::vector<std::string> get_tags(const std::string& path);
};

}  // namespace pluto
