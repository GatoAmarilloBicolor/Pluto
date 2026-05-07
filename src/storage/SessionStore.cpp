#include "SessionStore.h"

#include <Directory.h>
#include <Entry.h>
#include <Node.h>
#include <Path.h>
#include <Query.h>
#include <TypeConstants.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <fs_attr.h>
#include <fs_index.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>
#include <vector>

#include "../json.h"
#include "../util.h"

namespace pluto {

namespace {

std::string& dir_override() {
	static std::string s;
	return s;
}

std::string home_dir() {
	return HomeDir();
}

int64_t now_unix() {
	return static_cast<int64_t>(std::time(nullptr));
}

std::string slugify(const std::string& s) {
	std::string out;
	for (char c : s) {
		unsigned char u = static_cast<unsigned char>(c);
		if (std::isalnum(u))
			out += static_cast<char>(std::tolower(u));
		else if (c == ' ' || c == '-' || c == '_')
			out += '-';
		if (out.size() >= 40) break;
	}
	while (!out.empty() && out.front() == '-') out.erase(0, 1);
	while (!out.empty() && out.back()  == '-') out.pop_back();
	if (out.empty()) out = "session";
	return out;
}

std::string to_lower(const std::string& s) {
	std::string r;
	r.reserve(s.size());
	for (char c : s)
		r += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return r;
}

// ---------------- attribute helpers ----------------

void write_string_attr(BNode& node, const char* name, const std::string& v) {
	node.WriteAttr(name, B_STRING_TYPE, 0, v.c_str(), v.size() + 1);
}

void write_int64_attr(BNode& node, const char* name, int64_t v) {
	int64 x = v;
	node.WriteAttr(name, B_INT64_TYPE, 0, &x, sizeof(x));
}

void write_int32_attr(BNode& node, const char* name, int32_t v) {
	int32 x = v;
	node.WriteAttr(name, B_INT32_TYPE, 0, &x, sizeof(x));
}

std::string read_string_attr(BNode& node, const char* name) {
	attr_info info;
	if (node.GetAttrInfo(name, &info) != B_OK) return {};
	if (info.type != B_STRING_TYPE) return {};
	std::vector<char> buf(info.size > 0 ? info.size : 1);
	ssize_t n = node.ReadAttr(name, B_STRING_TYPE, 0, buf.data(), info.size);
	if (n <= 0) return {};
	std::string s(buf.data(), n);
	while (!s.empty() && s.back() == 0) s.pop_back();
	return s;
}

int64_t read_int64_attr(BNode& node, const char* name) {
	attr_info info;
	if (node.GetAttrInfo(name, &info) != B_OK) return 0;
	if (info.type != B_INT64_TYPE) return 0;
	int64 v = 0;
	node.ReadAttr(name, B_INT64_TYPE, 0, &v, sizeof(v));
	return v;
}

int32_t read_int32_attr(BNode& node, const char* name) {
	attr_info info;
	if (node.GetAttrInfo(name, &info) != B_OK) return 0;
	if (info.type != B_INT32_TYPE) return 0;
	int32 v = 0;
	node.ReadAttr(name, B_INT32_TYPE, 0, &v, sizeof(v));
	return v;
}

// ---------------- JSONL serialization ----------------

json::Value message_to_json(const Message& m) {
	json::Value mv;
	mv.set("role", m.role);
	json::Array blocks;
	for (const auto& b : m.content) {
		json::Value bv;
		if (b.type == ContentBlock::TEXT) {
			bv.set("type", std::string("text"));
			bv.set("text", b.text);
		} else if (b.type == ContentBlock::TOOL_USE) {
			bv.set("type", std::string("tool_use"));
			bv.set("id", b.tool_id);
			bv.set("name", b.tool_name);
			json::Value input(json::Object{});
			if (!b.tool_input.empty()) {
				try { input = json::Value::parse(b.tool_input); }
				catch (const std::exception&) {}
			}
			bv.set("input", input);
		} else if (b.type == ContentBlock::TOOL_RESULT) {
			bv.set("type", std::string("tool_result"));
			bv.set("tool_use_id", b.tool_id);
			bv.set("content", b.text);
			if (b.is_error) bv.set("is_error", json::Value(true));
		} else if (b.type == ContentBlock::IMAGE) {
			bv.set("type", std::string("image"));
			bv.set("media_type", b.image_media_type);
			bv.set("data", b.image_data);
		}
		blocks.push_back(bv);
	}
	mv.set("content", json::Value(std::move(blocks)));
	return mv;
}

Message message_from_json(const json::Value& v) {
	Message m;
	m.role = v.str("role");
	const json::Value* content = v.find("content");
	if (!content || !content->is_arr()) return m;
	for (const auto& b : content->as_arr()) {
		ContentBlock cb;
		std::string type = b.str("type");
		if (type == "text") {
			cb.type = ContentBlock::TEXT;
			cb.text = b.str("text");
		} else if (type == "tool_use") {
			cb.type = ContentBlock::TOOL_USE;
			cb.tool_id = b.str("id");
			cb.tool_name = b.str("name");
			const json::Value* input = b.find("input");
			if (input) cb.tool_input = input->serialize();
		} else if (type == "tool_result") {
			cb.type = ContentBlock::TOOL_RESULT;
			cb.tool_id = b.str("tool_use_id");
			cb.text = b.str("content");
			const json::Value* err = b.find("is_error");
			if (err && err->is_bool()) cb.is_error = err->as_bool();
		} else if (type == "image") {
			cb.type = ContentBlock::IMAGE;
			cb.image_media_type = b.str("media_type");
			cb.image_data = b.str("data");
		}
		m.content.push_back(std::move(cb));
	}
	return m;
}

}  // namespace

// ---------------- SessionStore ----------------

std::string SessionStore::sessions_dir() {
	if (!dir_override().empty()) return dir_override();
	return home_dir() + "/config/settings/pluto/sessions";
}

void SessionStore::set_dir_override(const std::string& path) {
	dir_override() = path;
}

bool SessionStore::ensure_sessions_dir() {
	std::string dir = sessions_dir();
	std::string acc;
	for (size_t i = 0; i < dir.size(); ++i) {
		acc += dir[i];
		if (dir[i] == '/' && i > 0) {
			::mkdir(acc.c_str(), 0755);
		}
	}
	::mkdir(dir.c_str(), 0755);

	// Create BFS indexes on the boot volume so Tracker queries (and our
	// own future BQuery-based search) can find sessions by attribute.
	// fs_create_index is idempotent — returns -1/EEXIST if already there,
	// which we silently ignore.
	BVolume boot;
	if (BVolumeRoster().GetBootVolume(&boot) == B_OK) {
		dev_t dev = boot.Device();
		fs_create_index(dev, "pluto:title",   B_STRING_TYPE, 0);
		fs_create_index(dev, "pluto:model",   B_STRING_TYPE, 0);
		fs_create_index(dev, "pluto:tags",    B_STRING_TYPE, 0);
		fs_create_index(dev, "pluto:updated", B_INT64_TYPE,  0);
		fs_create_index(dev, "pluto:created", B_INT64_TYPE,  0);
	}

	struct stat st;
	return ::stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string SessionStore::save(std::string path,
                                const std::string& title,
                                const std::string& model,
                                int64_t tokens_in,
                                int64_t tokens_out,
                                const std::vector<Message>& messages,
                                std::string& error) {
	if (!ensure_sessions_dir()) {
		error = "cannot create sessions directory";
		return {};
	}

	int64_t created_ts = 0;
	if (path.empty()) {
		std::string slug = slugify(title.empty() ? std::string("session") : title);
		int64_t now = now_unix();
		// 4-hex random suffix to avoid collision between concurrent instances.
		static thread_local std::mt19937 rng(std::random_device{}());
		char buf[80];
		std::snprintf(buf, sizeof(buf), "%lld-%s-%04x",
			(long long)now, slug.c_str(),
			(unsigned)(rng() & 0xffff));
		path = sessions_dir() + "/" + buf + ".jsonl";
		created_ts = now;
	}

	std::ofstream f(path);
	if (!f) {
		error = "cannot open for writing: " + path;
		return {};
	}
	for (const auto& m : messages) {
		f << message_to_json(m).serialize() << "\n";
	}
	f.close();

	BNode node(path.c_str());
	if (node.InitCheck() == B_OK) {
		if (!title.empty())
			write_string_attr(node, "pluto:title", title);
		if (!model.empty())
			write_string_attr(node, "pluto:model", model);
		write_int64_attr(node, "pluto:tokens_in", tokens_in);
		write_int64_attr(node, "pluto:tokens_out", tokens_out);
		if (created_ts > 0)
			write_int64_attr(node, "pluto:created", created_ts);
		write_int64_attr(node, "pluto:updated", now_unix());
		write_int32_attr(node, "pluto:turns",
			static_cast<int32_t>(messages.size()));
	}

	return path;
}

bool SessionStore::load(const std::string& path,
                        std::vector<Message>& out,
                        SessionMetadata& meta,
                        std::string& error) {
	std::ifstream f(path);
	if (!f) {
		error = "cannot open: " + path;
		return false;
	}
	out.clear();
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty()) continue;
		try {
			auto v = json::Value::parse(line);
			out.push_back(message_from_json(v));
		} catch (const std::exception&) {
			// skip malformed
		}
	}

	BNode node(path.c_str());
	meta = SessionMetadata{};
	meta.path = path;
	BPath p(path.c_str());
	meta.filename = p.Leaf() ? p.Leaf() : path;
	if (node.InitCheck() == B_OK) {
		meta.title      = read_string_attr(node, "pluto:title");
		meta.model      = read_string_attr(node, "pluto:model");
		meta.tags       = read_string_attr(node, "pluto:tags");
		meta.tokens_in  = read_int64_attr (node, "pluto:tokens_in");
		meta.tokens_out = read_int64_attr (node, "pluto:tokens_out");
		meta.created    = read_int64_attr (node, "pluto:created");
		meta.updated    = read_int64_attr (node, "pluto:updated");
		meta.turns      = read_int32_attr (node, "pluto:turns");
	}
	return true;
}

// Helper: build SessionMetadata from a BEntry.
static bool entry_to_meta(BEntry& entry, SessionMetadata& m) {
	if (!entry.IsFile()) return false;
	BPath p;
	if (entry.GetPath(&p) != B_OK) return false;
	BNode node(&entry);
	if (node.InitCheck() != B_OK) return false;

	m.path       = p.Path() ? p.Path() : "";
	m.filename   = p.Leaf() ? p.Leaf() : m.path;
	m.title      = read_string_attr(node, "pluto:title");
	m.model      = read_string_attr(node, "pluto:model");
	m.tags       = read_string_attr(node, "pluto:tags");
	m.tokens_in  = read_int64_attr (node, "pluto:tokens_in");
	m.tokens_out = read_int64_attr (node, "pluto:tokens_out");
	m.created    = read_int64_attr (node, "pluto:created");
	m.updated    = read_int64_attr (node, "pluto:updated");
	m.turns      = read_int32_attr (node, "pluto:turns");
	return true;
}

std::vector<SessionMetadata> SessionStore::list() {
	std::vector<SessionMetadata> result;

	// Try BQuery first: uses the pluto:updated BFS index for fast lookup.
	// Falls back to BDirectory scan if the query fails (e.g. no index).
	bool used_query = false;
	{
		BVolumeRoster roster;
		BVolume vol;
		if (roster.GetBootVolume(&vol) == B_OK) {
			BQuery query;
			query.SetVolume(&vol);
			// Find all files that have pluto:updated > 0 (i.e. all sessions).
			query.SetPredicate("pluto:updated > 0");
			if (query.Fetch() == B_OK) {
				used_query = true;
				std::string sdir = sessions_dir();
				BEntry entry;
				while (query.GetNextEntry(&entry) == B_OK) {
					// Filter to only files in our sessions directory.
					BPath p;
					if (entry.GetPath(&p) != B_OK) continue;
					std::string sp = p.Path() ? p.Path() : "";
					if (sp.rfind(sdir, 0) != 0) continue;

					SessionMetadata m;
					if (entry_to_meta(entry, m))
						result.push_back(std::move(m));
				}
			}
		}
	}

	// Fallback: plain directory scan.
	if (!used_query) {
		BDirectory dir(sessions_dir().c_str());
		if (dir.InitCheck() != B_OK) return result;
		BEntry entry;
		while (dir.GetNextEntry(&entry) == B_OK) {
			SessionMetadata m;
			if (entry_to_meta(entry, m))
				result.push_back(std::move(m));
		}
	}

	std::sort(result.begin(), result.end(),
		[](const SessionMetadata& a, const SessionMetadata& b) {
			return a.updated > b.updated;
		});
	return result;
}

SessionMetadata SessionStore::most_recent() {
	auto all = list();
	return all.empty() ? SessionMetadata{} : all.front();
}

bool SessionStore::remove(const std::string& path, std::string& error) {
	BEntry entry(path.c_str());
	if (entry.InitCheck() != B_OK) {
		error = "not found: " + path;
		return false;
	}
	if (entry.Remove() != B_OK) {
		error = "remove failed: " + path;
		return false;
	}
	return true;
}

SessionMetadata SessionStore::find_by_title(const std::string& query) {
	auto all = list();
	std::string ql = to_lower(query);
	for (const auto& m : all) {
		if (to_lower(m.title).find(ql) != std::string::npos) return m;
		if (to_lower(m.filename).find(ql) != std::string::npos) return m;
	}
	return {};
}

std::vector<std::string> SessionStore::get_tags(const std::string& path) {
	std::vector<std::string> out;
	BNode node(path.c_str());
	if (node.InitCheck() != B_OK) return out;
	std::string raw = read_string_attr(node, "pluto:tags");
	std::string cur;
	for (char c : raw) {
		if (c == ',') {
			if (!cur.empty()) out.push_back(cur);
			cur.clear();
		} else if (c != ' ') cur += c;
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}

bool SessionStore::add_tag(const std::string& path, const std::string& tag,
                            std::string& error) {
	BNode node(path.c_str());
	if (node.InitCheck() != B_OK) { error = "cannot open: " + path; return false; }
	auto tags = get_tags(path);
	for (const auto& t : tags) {
		if (t == tag) return true;  // already present
	}
	tags.push_back(tag);
	std::string joined;
	for (size_t i = 0; i < tags.size(); ++i) {
		if (i > 0) joined += ",";
		joined += tags[i];
	}
	write_string_attr(node, "pluto:tags", joined);
	return true;
}

bool SessionStore::remove_tag(const std::string& path, const std::string& tag,
                               std::string& error) {
	BNode node(path.c_str());
	if (node.InitCheck() != B_OK) { error = "cannot open: " + path; return false; }
	auto tags = get_tags(path);
	std::vector<std::string> kept;
	for (const auto& t : tags) if (t != tag) kept.push_back(t);
	std::string joined;
	for (size_t i = 0; i < kept.size(); ++i) {
		if (i > 0) joined += ",";
		joined += kept[i];
	}
	if (joined.empty()) {
		node.RemoveAttr("pluto:tags");
	} else {
		write_string_attr(node, "pluto:tags", joined);
	}
	return true;
}

}  // namespace pluto
