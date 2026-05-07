// Wiki LLM — markdown directory with frontmatter, TF-IDF search, BFS attrs.

#include "wiki.h"
#include "agent/Loop.h"
#include "config.h"
#include "frontmatter.h"
#include "provider/Provider.h"
#include "text_index.h"
#include "tools/Tool.h"
#include "util.h"
#include "workspace.h"

#include <FindDirectory.h>
#include <Node.h>
#include <OS.h>
#include <String.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

namespace pluto {

namespace {

std::mutex g_wiki_mutex;
std::vector<WikiEntry> g_wiki_entries;
IdfTable g_wiki_idf;

// Fill a WikiEntry from parsed frontmatter.
void fill_wiki_entry(const Frontmatter& fm, WikiEntry& entry) {
	entry.topic = fm.get("topic");
	entry.source = fm.get("source");
	entry.date = fm.get("date");
	entry.tags = fm.get_list("tags");
}

void build_wiki_idf() {
	// Build token sets for each document.
	std::vector<std::unordered_set<std::string>> doc_tokens;
	doc_tokens.reserve(g_wiki_entries.size());

	for (const auto& e : g_wiki_entries) {
		std::string text = e.topic;
		for (const auto& t : e.tags) text += " " + t;
		text += " " + e.body.substr(0, 500);

		auto toks = Tokenize(text);
		std::unordered_set<std::string> seen(toks.begin(), toks.end());
		doc_tokens.push_back(std::move(seen));
	}

	g_wiki_idf = BuildIdf(doc_tokens);
}

double score_wiki_entry(const WikiEntry& entry,
                        const std::vector<std::string>& query_tokens) {
	// Build tag string for scoring.
	std::string tags_text;
	for (const auto& t : entry.tags) tags_text += t + " ";

	std::vector<ScoredField> fields = {
		{ entry.topic, 4.0 },
		{ tags_text, 3.0 },
		{ entry.body.substr(0, 1000), 1.0 }
	};

	return ScoreDocument(fields, query_tokens, g_wiki_idf);
}

}  // namespace

int WikiInit(const std::string& dir) {
	std::lock_guard<std::mutex> lock(g_wiki_mutex);
	g_wiki_entries.clear();
	g_wiki_idf.clear();

	DIR* d = opendir(dir.c_str());
	if (!d) return 0;

	struct dirent* de;
	while ((de = readdir(d)) != nullptr) {
		std::string name = de->d_name;
		if (name.size() < 4) continue;
		if (name.substr(name.size() - 3) != ".md") continue;

		std::string path = dir + "/" + name;
		std::ifstream f(path);
		if (!f) continue;

		std::ostringstream ss;
		ss << f.rdbuf();
		std::string content = ss.str();

		WikiEntry entry;
		entry.path = path;
		entry.slug = name.substr(0, name.size() - 3);
		Frontmatter fm = ParseFrontmatter(content);
		fill_wiki_entry(fm, entry);
		entry.body = fm.body;

		// If no topic in frontmatter, use slug as topic.
		if (entry.topic.empty()) {
			entry.topic = entry.slug;
			for (auto& c : entry.topic)
				if (c == '-') c = ' ';
		}

		g_wiki_entries.push_back(std::move(entry));
	}
	closedir(d);

	build_wiki_idf();
	return (int)g_wiki_entries.size();
}

int WikiEntryCount() {
	std::lock_guard<std::mutex> lock(g_wiki_mutex);
	return (int)g_wiki_entries.size();
}

const std::vector<WikiEntry>& WikiEntries() {
	return g_wiki_entries;
}

std::vector<WikiResult> WikiSearch(const std::string& query, int top_k) {
	std::lock_guard<std::mutex> lock(g_wiki_mutex);
	if (g_wiki_entries.empty() || query.empty()) return {};

	auto query_tokens = Tokenize(query);

	std::vector<WikiResult> results;
	for (const auto& entry : g_wiki_entries) {
		double s = score_wiki_entry(entry, query_tokens);
		if (s > 0.0)
			results.push_back({ &entry, s });
	}

	std::sort(results.begin(), results.end(),
		[](const WikiResult& a, const WikiResult& b) {
			return a.score > b.score;
		});

	if ((int)results.size() > top_k)
		results.resize(top_k);

	return results;
}

std::string WikiSave(const std::string& dir, const std::string& topic,
                     const std::vector<std::string>& tags,
                     const std::string& source, const std::string& body) {
	::mkdir(dir.c_str(), 0755);

	std::string slug = Slugify(topic);
	std::string path = dir + "/" + slug + ".md";

	// Avoid overwriting: append number if exists.
	struct stat st;
	if (::stat(path.c_str(), &st) == 0) {
		for (int i = 2; i < 100; ++i) {
			path = dir + "/" + slug + "-" + std::to_string(i) + ".md";
			if (::stat(path.c_str(), &st) != 0) break;
		}
	}

	// Build frontmatter.
	std::ofstream f(path, std::ios::trunc);
	if (!f) return {};

	f << "---\n";
	f << "topic: " << topic << "\n";
	if (!tags.empty()) {
		f << "tags: ";
		for (size_t i = 0; i < tags.size(); ++i) {
			if (i > 0) f << ", ";
			f << tags[i];
		}
		f << "\n";
	}
	if (!source.empty())
		f << "source: " << source << "\n";

	// Current date.
	time_t now = time(nullptr);
	char datebuf[32];
	strftime(datebuf, sizeof(datebuf), "%Y-%m-%d", localtime(&now));
	f << "date: " << datebuf << "\n";
	f << "---\n\n";
	f << body << "\n";
	f.close();

	// Write BFS attributes for query_fs support.
	BNode node(path.c_str());
	if (node.InitCheck() == B_OK) {
		BString btopic(topic.c_str());
		node.WriteAttrString("wiki:topic", &btopic);

		if (!tags.empty()) {
			std::string tags_str;
			for (size_t i = 0; i < tags.size(); ++i) {
				if (i > 0) tags_str += ",";
				tags_str += tags[i];
			}
			BString btags(tags_str.c_str());
			node.WriteAttrString("wiki:tags", &btags);
		}

		if (!source.empty()) {
			BString bsource(source.c_str());
			node.WriteAttrString("wiki:source", &bsource);
		}
	}

	return path;
}

std::string WikiDefaultDir() {
	struct stat st;

	// 1. Workspace-local: <workspace>/.pluto/wiki/
	std::string ws = WorkspaceRoot();
	if (!ws.empty()) {
		std::string ws_dir = ws + "/.pluto/wiki";
		if (::stat(ws_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
			return ws_dir;
	}

	// 2. CWD-local (legacy): ./wiki/
	if (::stat("wiki", &st) == 0 && S_ISDIR(st.st_mode))
		return "wiki";

	// 3. User-global.
	std::string user_dir = SettingsDir() + "/wiki";
	if (::stat(user_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
		return user_dir;

	return {};
}

std::string WikiEnsureDir() {
	// Always prefer workspace-local when in a workspace.
	std::string ws = WorkspaceRoot();
	if (!ws.empty()) {
		std::string ws_dir = ws + "/.pluto/wiki";
		struct stat st;
		if (::stat(ws_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
			return ws_dir;
		// Create it.
		std::string pluto_dir = ws + "/.pluto";
		::mkdir(pluto_dir.c_str(), 0755);
		::mkdir(ws_dir.c_str(), 0755);
		return ws_dir;
	}

	// No workspace — use existing dir or create user-global.
	std::string dir = WikiDefaultDir();
	if (!dir.empty()) return dir;

	std::string user_dir = SettingsDir() + "/wiki";
	::mkdir(user_dir.c_str(), 0755);
	return user_dir;
}

std::string WikiFormatContext(const std::vector<WikiResult>& results) {
	if (results.empty()) return {};
	std::string ctx = "<wiki_knowledge>\n";
	for (const auto& r : results) {
		ctx += "## " + r.entry->topic + "\n";
		if (!r.entry->tags.empty()) {
			ctx += "Tags:";
			for (const auto& t : r.entry->tags) ctx += " " + t;
			ctx += "\n";
		}
		ctx += "\n" + r.entry->body + "\n\n";
	}
	ctx += "</wiki_knowledge>";
	return ctx;
}

std::string WikiIngestPrompt() {
	return R"(You are a knowledge extraction assistant. Analyze the provided content and produce a structured wiki entry.

Output EXACTLY this format (no code fences, no extra text):

TOPIC: <concise title, max 60 chars>
TAGS: <comma-separated keywords, 3-6 tags>
BODY:
<markdown summary of the key information. Include:
- Main concepts explained clearly
- Code examples if relevant (in fenced blocks)
- Links to related topics
- Keep it concise but complete: 100-500 words>)";
}

// Parse the LLM's ingest output into structured fields.
namespace {

struct IngestResult {
	std::string topic;
	std::vector<std::string> tags;
	std::string body;
};

IngestResult parse_ingest_output(const std::string& text) {
	IngestResult r;

	// Find TOPIC:
	size_t tp = text.find("TOPIC:");
	if (tp != std::string::npos) {
		size_t eol = text.find('\n', tp);
		std::string val = text.substr(tp + 6, eol - tp - 6);
		while (!val.empty() && val.front() == ' ') val = val.substr(1);
		while (!val.empty() && val.back() == ' ') val.pop_back();
		r.topic = val;
	}

	// Find TAGS:
	size_t tg = text.find("TAGS:");
	if (tg != std::string::npos) {
		size_t eol = text.find('\n', tg);
		std::string val = text.substr(tg + 5, eol - tg - 5);
		std::istringstream ts(val);
		std::string tag;
		while (std::getline(ts, tag, ',')) {
			while (!tag.empty() && tag.front() == ' ') tag = tag.substr(1);
			while (!tag.empty() && tag.back() == ' ') tag.pop_back();
			if (!tag.empty()) r.tags.push_back(tag);
		}
	}

	// Find BODY:
	size_t bp = text.find("BODY:");
	if (bp != std::string::npos) {
		size_t start = text.find('\n', bp);
		if (start != std::string::npos) {
			r.body = text.substr(start + 1);
			// Trim trailing whitespace.
			while (!r.body.empty() && (r.body.back() == '\n' || r.body.back() == ' '))
				r.body.pop_back();
		}
	}

	return r;
}

}  // namespace

int RunWikiCli(int argc, char** argv) {
	if (argc < 1) {
		std::fprintf(stderr,
			"usage: pluto wiki search <query>\n"
			"       pluto wiki list\n"
			"       pluto wiki stats\n"
			"       pluto wiki ingest <url-or-file> [--model MODEL]\n"
			"  Manage the project wiki (LLM knowledge base).\n");
		return 2;
	}

	std::string sub = argv[0];

	// pluto wiki stats
	if (sub == "stats") {
		std::string dir = WikiDefaultDir();
		if (dir.empty()) {
			std::printf("Wiki not initialized. Use 'pluto wiki ingest' to add entries.\n");
			return 0;
		}
		int n = WikiInit(dir);
		std::printf("Wiki: %d entries in %s/\n", n, dir.c_str());
		return 0;
	}

	// pluto wiki list
	if (sub == "list") {
		std::string dir = WikiDefaultDir();
		if (dir.empty()) {
			std::printf("(no wiki entries)\n");
			return 0;
		}
		int n = WikiInit(dir);
		if (n == 0) {
			std::printf("(no wiki entries)\n");
			return 0;
		}
		for (const auto& e : WikiEntries()) {
			std::printf("  \x1b[1m%s\x1b[0m", e.topic.c_str());
			if (!e.tags.empty()) {
				std::printf("  [");
				for (size_t i = 0; i < e.tags.size(); ++i) {
					if (i > 0) std::printf(", ");
					std::printf("%s", e.tags[i].c_str());
				}
				std::printf("]");
			}
			std::printf("\n    %s\n", e.path.c_str());
		}
		return 0;
	}

	// pluto wiki search <query>
	if (sub == "search") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto wiki search <query>\n");
			return 2;
		}
		std::string dir = WikiDefaultDir();
		if (dir.empty()) {
			std::printf("Wiki not initialized.\n");
			return 1;
		}
		WikiInit(dir);

		std::string query;
		for (int i = 1; i < argc; ++i) {
			if (i > 1) query += ' ';
			query += argv[i];
		}

		auto results = WikiSearch(query, 5);
		if (results.empty()) {
			std::printf("No matches for: %s\n", query.c_str());
			return 0;
		}

		for (size_t i = 0; i < results.size(); ++i) {
			const auto& r = results[i];
			std::printf("\x1b[1m[%zu] %s\x1b[0m  (score %.1f)\n",
				i + 1, r.entry->topic.c_str(), r.score);
			if (!r.entry->tags.empty()) {
				std::printf("    tags:");
				for (const auto& t : r.entry->tags)
					std::printf(" %s", t.c_str());
				std::printf("\n");
			}
			// Show first 200 chars of body.
			std::string preview = r.entry->body.substr(0, 200);
			if (r.entry->body.size() > 200) preview += "...";
			std::printf("\n%s\n\n", preview.c_str());
			if (i + 1 < results.size())
				std::printf("---\n\n");
		}
		return 0;
	}

	// pluto wiki ingest <url-or-file> [--model MODEL]
	if (sub == "ingest") {
		if (argc < 2) {
			std::fprintf(stderr,
				"usage: pluto wiki ingest <url-or-file> [--model MODEL]\n"
				"  Fetch content, analyze with LLM, save as wiki .md entry.\n"
				"  Also reads from stdin if <url-or-file> is '-'.\n");
			return 2;
		}

		std::string input = argv[1];
		std::string model;

		for (int i = 2; i < argc; ++i) {
			if (std::string(argv[i]) == "--model" && i + 1 < argc)
				model = argv[++i];
		}

		// Read input content.
		std::string content;

		if (input == "-") {
			// Read from stdin.
			std::ostringstream ss;
			ss << std::cin.rdbuf();
			content = ss.str();
		} else if (input.substr(0, 4) == "http" || input.substr(0, 4) == "www.") {
			// URL: use curl to fetch. Prepend https:// if missing.
			std::string url = input;
			if (url.substr(0, 4) == "www.")
				url = "https://" + url;
			std::string cmd = "curl -sL --max-time 30 '" + url + "'";
			FILE* pipe = popen(cmd.c_str(), "r");
			if (!pipe) {
				std::fprintf(stderr, "failed to fetch: %s\n", input.c_str());
				return 1;
			}
			char buf[4096];
			while (size_t n = fread(buf, 1, sizeof(buf), pipe))
				content.append(buf, n);
			pclose(pipe);
		} else {
			// File path.
			std::ifstream f(input);
			if (!f) {
				std::fprintf(stderr, "cannot open: %s\n", input.c_str());
				return 1;
			}
			std::ostringstream ss;
			ss << f.rdbuf();
			content = ss.str();
		}

		if (content.empty()) {
			std::fprintf(stderr, "no content to ingest\n");
			return 1;
		}

		// Truncate to ~8000 chars to avoid excessive token usage.
		if (content.size() > 8000)
			content = content.substr(0, 8000) + "\n\n[... truncated]";

		// Call the LLM in-process to analyze the content.
		std::string user_prompt = WikiIngestPrompt() +
			"\n\n---\n\nContent to analyze:\n\n" + content;

		std::fprintf(stderr, "[wiki] analyzing content...\n");

		// Load config and create provider.
		Config cfg = LoadConfig();
		if (!model.empty()) cfg.model = model;
		AutoDetectProvider(cfg);

		auto creds = ResolveCredentials(cfg);
		if (creds.empty()) {
			std::fprintf(stderr, "No credentials for provider '%s'.\n",
				cfg.provider.c_str());
			return 1;
		}

		auto provider = CreateProvider(cfg);
		if (!provider) {
			std::fprintf(stderr, "Failed to create provider.\n");
			return 1;
		}

		// Single-turn call: user message → assistant text response.
		std::vector<Message> messages;
		Message user_msg;
		user_msg.role = "user";
		ContentBlock tb;
		tb.type = ContentBlock::TEXT;
		tb.text = user_prompt;
		user_msg.content.push_back(std::move(tb));
		messages.push_back(std::move(user_msg));

		std::string llm_output;
		int rc = 0;

		UiCallbacks ui;
		ui.on_text = [&](const std::string& t) { llm_output += t; };
		ui.on_error = [&](const std::string& e) {
			std::fprintf(stderr, "[error] %s\n", e.c_str());
			rc = 1;
		};

		ToolRegistry empty_tools;
		RunTurn(*provider, cfg, empty_tools, messages, AutoAllow(), ui);
		if (rc != 0) {
			std::fprintf(stderr, "LLM analysis failed.\n");
			return 1;
		}

		// Parse structured output.
		auto result = parse_ingest_output(llm_output);

		if (result.topic.empty()) {
			std::fprintf(stderr, "LLM did not produce a valid TOPIC.\n");
			std::fprintf(stderr, "Raw output:\n%s\n", llm_output.c_str());
			return 1;
		}

		if (result.body.empty()) {
			std::fprintf(stderr, "LLM did not produce a BODY.\n");
			return 1;
		}

		// Save to wiki.
		std::string dir = WikiEnsureDir();
		std::string source;
		if (input != "-") {
			source = input;
			// Normalize URL for the source attribute.
			if (source.substr(0, 4) == "www.")
				source = "https://" + source;
		}
		std::string path = WikiSave(dir, result.topic, result.tags, source, result.body);

		if (path.empty()) {
			std::fprintf(stderr, "failed to save wiki entry\n");
			return 1;
		}

		std::printf("\x1b[32m✓\x1b[0m Saved: %s\n", path.c_str());
		std::printf("  topic: %s\n", result.topic.c_str());
		if (!result.tags.empty()) {
			std::printf("  tags: ");
			for (size_t i = 0; i < result.tags.size(); ++i) {
				if (i > 0) std::printf(", ");
				std::printf("%s", result.tags[i].c_str());
			}
			std::printf("\n");
		}
		std::printf("  BFS: query_fs 'wiki:topic == \"%s\"'\n", result.topic.c_str());

		return 0;
	}

	std::fprintf(stderr, "unknown wiki subcommand: %s\n", sub.c_str());
	return 2;
}

}  // namespace pluto
