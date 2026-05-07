// Local BeAPI knowledge retrieval — keyword + TF-IDF scoring.
// No external dependencies beyond libbe: loads a JSONL index into memory and
// scores entries by class match + keyword overlap.

#include "rag.h"
#include "json.h"
#include "text_index.h"
#include "util.h"

#include <Node.h>
#include <String.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace pluto {

namespace {

std::mutex g_rag_mutex;   // protects g_entries and g_idf

std::vector<RagEntry> g_entries;

// IDF cache built via text_index.
IdfTable g_idf;

// Build IDF table from all entries (topic + question + classes).
void build_idf() {
	std::vector<std::unordered_set<std::string>> doc_tokens;
	doc_tokens.reserve(g_entries.size());

	for (const auto& e : g_entries) {
		auto toks = Tokenize(e.topic + " " + e.question);
		std::unordered_set<std::string> seen(toks.begin(), toks.end());
		// Add class names as tokens.
		for (const auto& cl : e.classes)
			seen.insert(ToLower(cl));
		doc_tokens.push_back(std::move(seen));
	}

	g_idf = BuildIdf(doc_tokens);
}

// Score a single entry against a query.
double score_entry(const RagEntry& entry,
                   const std::vector<std::string>& query_tokens,
                   const std::vector<std::string>& query_classes) {
	double score = 0.0;

	// 1. Class match bonus (strongest signal): exact Be class name match.
	//    Each matching class contributes a high fixed score weighted by IDF.
	for (const auto& qc : query_classes) {
		std::string qcl = ToLower(qc);
		for (const auto& ec : entry.classes) {
			if (ToLower(ec) == qcl) {
				double idf = 1.0;
				auto it = g_idf.find(qcl);
				if (it != g_idf.end()) idf = it->second;
				score += 5.0 * idf;
				break;
			}
		}
	}

	// 2. Topic keyword match: tokens appearing in the entry's topic.
	auto topic_tokens = Tokenize(entry.topic);
	std::unordered_set<std::string> topic_set(topic_tokens.begin(),
	                                           topic_tokens.end());
	for (const auto& qt : query_tokens) {
		if (topic_set.count(qt)) {
			double idf = 1.0;
			auto it = g_idf.find(qt);
			if (it != g_idf.end()) idf = it->second;
			score += 3.0 * idf;
		}
	}

	// 3. Question keyword match: tokens appearing in the question field.
	auto q_tokens = Tokenize(entry.question);
	std::unordered_set<std::string> q_set(q_tokens.begin(), q_tokens.end());
	for (const auto& qt : query_tokens) {
		if (q_set.count(qt)) {
			double idf = 1.0;
			auto it = g_idf.find(qt);
			if (it != g_idf.end()) idf = it->second;
			score += 1.0 * idf;
		}
	}

	return score;
}

}  // namespace

std::vector<std::string> RagExtractClasses(const std::string& text) {
	std::vector<std::string> classes;
	std::unordered_set<std::string> seen;
	size_t i = 0;
	while (i < text.size()) {
		// Look for 'B' followed by uppercase letter.
		if (text[i] == 'B' && i + 1 < text.size()
		    && std::isupper(static_cast<unsigned char>(text[i + 1]))) {
			// Check word boundary before 'B'.
			if (i > 0 && std::isalnum(static_cast<unsigned char>(text[i - 1]))) {
				++i;
				continue;
			}
			size_t start = i;
			while (i < text.size()
			       && std::isalnum(static_cast<unsigned char>(text[i]))) {
				++i;
			}
			std::string cls = text.substr(start, i - start);
			if (cls.size() >= 3 && !seen.count(cls)) {
				seen.insert(cls);
				classes.push_back(cls);
			}
		} else {
			++i;
		}
	}
	return classes;
}

int RagInit(const std::string& index_path) {
	std::lock_guard<std::mutex> lock(g_rag_mutex);
	g_entries.clear();
	g_idf.clear();

	std::ifstream f(index_path);
	if (!f) return 0;

	std::string line;
	while (std::getline(f, line)) {
		if (line.empty()) continue;
		try {
			json::Value v = json::Value::parse(line);
			RagEntry e;
			e.topic = v.str("topic");
			e.question = v.str("question");
			e.answer = v.str("answer");
			const json::Value* arr = v.find("classes");
			if (arr && arr->is_arr()) {
				for (const auto& c : arr->as_arr()) {
					if (c.is_str()) e.classes.push_back(c.as_str());
				}
			}
			if (!e.topic.empty())
				g_entries.push_back(std::move(e));
		} catch (const std::exception&) {
			// Skip malformed lines.
		}
	}

	build_idf();
	return (int)g_entries.size();
}

int RagEntryCount() {
	std::lock_guard<std::mutex> lock(g_rag_mutex);
	return (int)g_entries.size();
}

std::vector<RagResult> RagSearch(const std::string& query, int top_k) {
	std::lock_guard<std::mutex> lock(g_rag_mutex);
	if (g_entries.empty() || query.empty()) return {};

	auto query_tokens = Tokenize(query);
	auto query_classes = RagExtractClasses(query);

	// Also add lowercased class names to tokens for keyword matching.
	for (const auto& cls : query_classes)
		query_tokens.push_back(ToLower(cls));

	// Score all entries.
	std::vector<RagResult> results;
	results.reserve(g_entries.size());
	for (const auto& entry : g_entries) {
		double s = score_entry(entry, query_tokens, query_classes);
		if (s > 0.0)
			results.push_back({ &entry, s });
	}

	// Sort by descending score.
	std::sort(results.begin(), results.end(),
		[](const RagResult& a, const RagResult& b) {
			return a.score > b.score;
		});

	// Trim to top_k.
	if ((int)results.size() > top_k)
		results.resize(top_k);

	return results;
}

std::string RagDefaultPath() {
	// 1. Development: next to the binary.
	struct stat st;
	if (::stat("data/beapi-index.jsonl", &st) == 0)
		return "data/beapi-index.jsonl";

	// 2. User-installed.
	std::string user_path = SettingsDir() + "/beapi-index.jsonl";
	if (::stat(user_path.c_str(), &st) == 0)
		return user_path;

	// 3. Packaged.
	const char* pkg = "/boot/system/data/pluto/beapi-index.jsonl";
	if (::stat(pkg, &st) == 0)
		return std::string(pkg);

	return {};
}

std::string RagFormatContext(const std::vector<RagResult>& results) {
	if (results.empty()) return {};
	std::string ctx = "<beapi_reference>\n";
	for (const auto& r : results) {
		ctx += "## ";
		ctx += r.entry->topic;
		ctx += "\n";
		ctx += r.entry->answer;
		ctx += "\n\n";
	}
	ctx += "</beapi_reference>";
	return ctx;
}

std::string RagCustomPath() {
	return SettingsDir() + "/beapi-custom.jsonl";
}

int RagLoadCustom(const std::string& custom_path) {
	std::lock_guard<std::mutex> lock(g_rag_mutex);
	std::ifstream f(custom_path);
	if (!f) return 0;

	int added = 0;
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty()) continue;
		try {
			json::Value v = json::Value::parse(line);
			RagEntry e;
			e.topic = v.str("topic");
			e.question = v.str("question");
			e.answer = v.str("answer");
			const json::Value* arr = v.find("classes");
			if (arr && arr->is_arr()) {
				for (const auto& c : arr->as_arr()) {
					if (c.is_str()) e.classes.push_back(c.as_str());
				}
			}
			// Auto-extract classes if not provided.
			if (e.classes.empty())
				e.classes = RagExtractClasses(e.answer);
			if (!e.topic.empty()) {
				g_entries.push_back(std::move(e));
				++added;
			}
		} catch (const std::exception&) {}
	}

	if (added > 0) build_idf();
	return added;
}

int RagExportBfs(const std::string& dir) {
	::mkdir(dir.c_str(), 0755);
	int written = 0;

	for (const auto& e : g_entries) {
		// Filename: sanitized topic.
		std::string fname;
		for (char c : e.topic) {
			if (std::isalnum(static_cast<unsigned char>(c)) || c == '-')
				fname += c;
			else if (c == ' ' && !fname.empty() && fname.back() != '-')
				fname += '-';
		}
		if (fname.empty()) fname = "entry-" + std::to_string(written);
		std::string path = dir + "/" + fname;

		// Write the snippet as file content.
		std::ofstream f(path, std::ios::trunc);
		if (!f) continue;
		f << "# " << e.topic << "\n\n" << e.answer << "\n";
		f.close();

		// Write BFS attributes via native BNode API (no shell).
		BNode node(path.c_str());
		if (node.InitCheck() == B_OK) {
			BString topic(e.topic.c_str());
			node.WriteAttrString("beapi:topic", &topic);

			if (!e.classes.empty()) {
				std::string cls_str;
				for (size_t i = 0; i < e.classes.size(); ++i) {
					if (i > 0) cls_str += ",";
					cls_str += e.classes[i];
				}
				BString classes(cls_str.c_str());
				node.WriteAttrString("beapi:classes", &classes);
			}
		}
		++written;
	}
	return written;
}

namespace {

int load_index_with_custom() {
	std::string path = RagDefaultPath();
	if (path.empty()) {
		std::fprintf(stderr, "beapi index not found. Expected in:\n"
			"  ./data/beapi-index.jsonl\n"
			"  ~/config/settings/pluto/beapi-index.jsonl\n"
			"  /boot/system/data/pluto/beapi-index.jsonl\n");
		return 0;
	}
	int n = RagInit(path);
	if (n == 0) {
		std::fprintf(stderr, "failed to load index: %s\n", path.c_str());
		return 0;
	}
	// Merge custom snippets if present.
	std::string custom = RagCustomPath();
	struct stat st;
	if (::stat(custom.c_str(), &st) == 0) {
		int added = RagLoadCustom(custom);
		if (added > 0)
			std::fprintf(stderr, "[beapi] loaded %d custom snippets\n", added);
	}
	return n;
}

}  // namespace

int RunBeapiCli(int argc, char** argv) {
	if (argc < 1) {
		std::fprintf(stderr,
			"usage: pluto beapi <query>\n"
			"       pluto beapi stats\n"
			"       pluto beapi export\n"
			"  Search the local BeAPI reference.\n");
		return 2;
	}

	std::string sub = argv[0];

	// pluto beapi stats
	if (sub == "stats") {
		int n = load_index_with_custom();
		if (n == 0) return 1;
		std::printf("BeAPI reference: %d entries loaded\n", RagEntryCount());
		std::string custom = RagCustomPath();
		struct stat cst;
		if (::stat(custom.c_str(), &cst) == 0)
			std::printf("Custom snippets: %s\n", custom.c_str());
		return 0;
	}

	// pluto beapi export
	if (sub == "export") {
		int n = load_index_with_custom();
		if (n == 0) return 1;
		std::string dir = SettingsDir() + "/beapi";
		int written = RagExportBfs(dir);
		std::printf("Exported %d entries to %s/\n", written, dir.c_str());
		std::printf("Search with Tracker or: query_fs 'beapi:topic == *Slider*'\n");
		return 0;
	}

	// Default: search
	// Join all args into query string.
	std::string query;
	for (int i = 0; i < argc; ++i) {
		if (i > 0) query += ' ';
		query += argv[i];
	}

	int n = load_index_with_custom();
	if (n == 0) return 1;

	auto results = RagSearch(query, 5);
	if (results.empty()) {
		std::printf("No matches for: %s\n", query.c_str());
		return 0;
	}

	for (size_t i = 0; i < results.size(); ++i) {
		const auto& r = results[i];
		std::printf("\x1b[1m[%zu] %s\x1b[0m  (score %.1f)\n",
			i + 1, r.entry->topic.c_str(), r.score);
		if (!r.entry->classes.empty()) {
			std::printf("    classes:");
			for (const auto& c : r.entry->classes)
				std::printf(" %s", c.c_str());
			std::printf("\n");
		}
		std::printf("\n%s\n\n", r.entry->answer.c_str());
		if (i + 1 < results.size())
			std::printf("---\n\n");
	}
	return 0;
}

}  // namespace pluto
