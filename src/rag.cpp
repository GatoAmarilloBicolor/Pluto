// Local BeAPI knowledge retrieval — keyword + TF-IDF scoring.
// No external dependencies beyond libbe: loads a JSONL index into memory and
// scores entries by class match + keyword overlap.

#include "rag.h"
#include "json.h"
#include "text_index.h"
#include "util.h"

#ifdef __HAIKU__
#include <Node.h>
#include <Entry.h>
#include <Directory.h>
#include <File.h>
#include <TypeConstants.h>
#include <string.h>
#endif

#include <sys/stat.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace pluto {

std::mutex g_rag_mutex;   // protects g_entries and g_idf

struct InternalRagEntry {
    RagEntry entry;
    std::unordered_map<std::string, int> term_freq;
    double idf_score = 0.0;
};

std::vector<InternalRagEntry> g_entries;
std::unordered_map<std::string, int> g_doc_freq;   // term -> document count
std::unordered_map<std::string, double> g_idf;     // term -> idf score
bool g_idf_dirty = true;

// Helper: simple tokenizer (split on non-alnum, lowercase) - defined in text_index.cpp

void ComputeIdf() {
    if (!g_idf_dirty) return;
    g_doc_freq.clear();
    g_idf.clear();
    size_t N = g_entries.size();
    if (N == 0) {
        g_idf_dirty = false;
        return;
    }
    for (const auto& e : g_entries) {
        std::unordered_set<std::string> seen;
        for (const auto& term : e.term_freq) {
            if (seen.insert(term.first).second) g_doc_freq[term.first]++;
        }
    }
    for (const auto& [term, df] : g_doc_freq) {
        g_idf[term] = log(static_cast<double>(N) / df);
    }
    g_idf_dirty = false;
}

int RagInit(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    g_entries.clear();
    g_doc_freq.clear();
    g_idf.clear();
    g_idf_dirty = true;

    std::ifstream in(path);
    if (!in) return 0;

    std::string line;
    int count = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto v = json::Value::parse(line);
        if (!v.is_obj()) continue;

        InternalRagEntry e;
        const json::Value* topic = v.find("topic");
        const json::Value* question = v.find("question");
        const json::Value* answer = v.find("answer");
        if (topic) e.entry.topic = topic->as_str();
        if (question) e.entry.question = question->as_str();
        if (answer) e.entry.answer = answer->as_str();

        const json::Value* classes = v.find("classes");
        if (classes && classes->is_arr()) {
            auto arr = classes->as_arr();
            for (size_t i = 0; i < arr.size(); ++i) {
                e.entry.classes.push_back(arr[i].as_str());
            }
        }

        auto terms = Tokenize(e.entry.question + " " + e.entry.answer + " " + e.entry.topic);
        for (const auto& t : terms) e.term_freq[t]++;

        g_entries.push_back(std::move(e));
        count++;
    }

    ComputeIdf();
    return count;
}

std::vector<std::pair<double, int>> RagQuery(const std::string& query, int top_k, const std::string& class_filter) {
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    if (g_entries.empty()) return {};

    ComputeIdf();

    auto q_terms = Tokenize(query);
    std::vector<std::pair<double, int>> scored;
    scored.reserve(g_entries.size());

    for (size_t i = 0; i < g_entries.size(); ++i) {
        if (!class_filter.empty()) {
            bool match = false;
            for (const auto& c : g_entries[i].entry.classes) {
                if (c == class_filter) { match = true; break; }
            }
            if (!match) continue;
        }

        double score = 0.0;
        for (const auto& t : q_terms) {
            auto it_tf = g_entries[i].term_freq.find(t);
            auto it_idf = g_idf.find(t);
            if (it_tf != g_entries[i].term_freq.end() && it_idf != g_idf.end()) {
                score += it_tf->second * it_idf->second;
            }
        }
        if (score > 0) scored.emplace_back(score, static_cast<int>(i));
    }

    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    if ((int)scored.size() > top_k) scored.resize(top_k);
    return scored;
}

int RagExportBfs(const std::string& dir) {
#ifdef __HAIKU__
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    if (g_entries.empty()) return 0;

    BDirectory bdir(dir.c_str());
    if (bdir.InitCheck() != B_OK) return 0;

    int written = 0;
    for (size_t i = 0; i < g_entries.size(); ++i) {
        const auto& e = g_entries[i].entry;
        std::string filename = "snippet_" + std::to_string(i) + ".txt";

        BFile file(&bdir, filename.c_str(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
        if (file.InitCheck() != B_OK) continue;

        // Write content
        if (file.Write(e.answer.c_str(), e.answer.size()) != static_cast<ssize_t>(e.answer.size())) continue;

        // Write attributes
        if (file.WriteAttr("beapi:topic", B_STRING_TYPE, 0, e.topic.c_str(), e.topic.size() + 1) < 0) continue;

        std::string classes_str;
        for (size_t j = 0; j < e.classes.size(); ++j) {
            if (j) classes_str += ",";
            classes_str += e.classes[j];
        }
        if (file.WriteAttr("beapi:classes", B_STRING_TYPE, 0, classes_str.c_str(), classes_str.size() + 1) < 0) continue;

        ++written;
    }
    return written;
#else
    (void)dir;
    return 0;
#endif
}

int RunBeapiCli(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: pluto beapi <query|update|export|stats>\n");
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "update") {
        std::string path = RagDefaultPath();
        if (path.empty()) {
            std::fprintf(stderr, "Index not found. Run `pluto beapi update` from source tree.\n");
            return 1;
        }
        int n = RagInit(path);
        std::printf("Loaded %d entries from %s\n", n, path.c_str());
        return 0;
    }

    if (cmd == "query") {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: pluto beapi query <text> [class]\n");
            return 1;
        }
        std::string query = argv[2];
        std::string cls = (argc >= 4) ? argv[3] : "";
        std::string path = RagDefaultPath();
        if (path.empty()) {
            std::fprintf(stderr, "Index not found. Run `pluto beapi update` first.\n");
            return 1;
        }
        RagInit(path);
        auto results = RagQuery(query, 10, cls);
        for (const auto& [score, idx] : results) {
            const auto& e = g_entries[idx].entry;
            std::printf("[%.3f] %s\n", score, e.topic.c_str());
            std::printf("  Q: %s\n", e.question.c_str());
            std::printf("  A: %.80s...\n", e.answer.c_str());
        }
        return 0;
    }

    if (cmd == "stats") {
        std::string path = RagDefaultPath();
        if (path.empty()) {
            std::fprintf(stderr, "Index not found.\n");
            return 1;
        }
        RagInit(path);
        std::printf("Entries: %zu\n", g_entries.size());
        std::printf("Vocabulary: %zu\n", g_idf.size());
        return 0;
    }

    if (cmd == "export") {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: pluto beapi export <dir>\n");
            return 1;
        }
        std::string path = RagDefaultPath();
        if (path.empty()) {
            std::fprintf(stderr, "Index not found. Run `pluto beapi update` first.\n");
            return 1;
        }
        RagInit(path);
        int n = RagExportBfs(argv[2]);
        std::printf("Exported %d entries to %s\n", n, argv[2]);
        return 0;
    }

    std::fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
    return 1;
}

std::string RagDefaultPath() {
    // Prefer the checked-in index at src/beapi_custom.jsonl if present.
    // This works when running from the source tree.
    std::string src_path = "src/beapi_custom.jsonl";
    struct stat st;
    if (stat(src_path.c_str(), &st) == 0) return src_path;

    // Fallback: look in the settings directory.
    return SettingsDir() + "/beapi_custom.jsonl";
}

std::string RagCustomPath() {
    return SettingsDir() + "/beapi-custom.jsonl";
}

int RagEntryCount() {
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    return static_cast<int>(g_entries.size());
}

std::vector<RagResult> RagSearch(const std::string& query, int top_k) {
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    if (g_entries.empty()) return {};

    ComputeIdf();

    auto q_terms = Tokenize(query);
    std::vector<std::pair<double, int>> scored;
    scored.reserve(g_entries.size());

    for (size_t i = 0; i < g_entries.size(); ++i) {
        double score = 0.0;
        for (const auto& t : q_terms) {
            auto it_tf = g_entries[i].term_freq.find(t);
            auto it_idf = g_idf.find(t);
            if (it_tf != g_entries[i].term_freq.end() && it_idf != g_idf.end()) {
                score += it_tf->second * it_idf->second;
            }
        }
        if (score > 0) scored.emplace_back(score, static_cast<int>(i));
    }

    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    if ((int)scored.size() > top_k) scored.resize(top_k);

    std::vector<RagResult> results;
    results.reserve(scored.size());
    for (const auto& [score, idx] : scored) {
        results.push_back({&g_entries[idx].entry, score});
    }
    return results;
}

std::vector<std::string> RagExtractClasses(const std::string& text) {
    std::vector<std::string> classes;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == 'B' && i + 1 < text.size() && std::isupper(static_cast<unsigned char>(text[i + 1]))) {
            if (!current.empty()) {
                classes.push_back(current);
                current.clear();
            }
            current = "B";
            ++i;
            while (i < text.size() && std::isalnum(static_cast<unsigned char>(text[i]))) {
                current += text[i];
                ++i;
            }
            --i;
        } else if (!current.empty() && std::isalnum(static_cast<unsigned char>(ch))) {
            current += ch;
        } else if (!current.empty()) {
            classes.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) classes.push_back(current);
    return classes;
}

std::string RagFormatContext(const std::vector<RagResult>& results) {
    if (results.empty()) return "";
    std::string context = "Relevant BeAPI snippets:\n\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& e = results[i].entry;
        context += "--- " + e->topic + " (score: " + std::to_string(results[i].score) + ") ---\n";
        context += "Q: " + e->question + "\n";
        context += "A: " + e->answer + "\n\n";
    }
    return context;
}

int RagLoadCustom(const std::string& custom_path) {
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    std::ifstream in(custom_path);
    if (!in) return 0;

    std::string line;
    int count = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto v = json::Value::parse(line);
        if (!v.is_obj()) continue;

        InternalRagEntry e;
        const json::Value* topic = v.find("topic");
        const json::Value* question = v.find("question");
        const json::Value* answer = v.find("answer");
        if (topic) e.entry.topic = topic->as_str();
        if (question) e.entry.question = question->as_str();
        if (answer) e.entry.answer = answer->as_str();

        const json::Value* classes = v.find("classes");
        if (classes && classes->is_arr()) {
            auto arr = classes->as_arr();
            for (size_t i = 0; i < arr.size(); ++i) {
                e.entry.classes.push_back(arr[i].as_str());
            }
        }

        auto terms = Tokenize(e.entry.question + " " + e.entry.answer + " " + e.entry.topic);
        for (const auto& t : terms) e.term_freq[t]++;

        g_entries.push_back(std::move(e));
        count++;
    }

    g_idf_dirty = true;
    return count;
}

}  // namespace pluto