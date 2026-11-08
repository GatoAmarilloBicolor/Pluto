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
#include <string.h>
#endif

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

struct CachedEntry {
    RagEntry entry;
    std::unordered_set<std::string> topic_tokens;
    std::unordered_set<std::string> question_tokens;
    std::unordered_set<std::string> class_tokens;  // lowercase
};

std::vector<CachedEntry> g_entries;

// IDF cache built via text_index.
IdfTable g_idf;

// Build IDF table from all entries (topic + question + classes).
void build_idf() {
    std::vector<std::unordered_set<std::string>> doc_tokens;
    doc_tokens.reserve(g_entries.size());

    for (const auto& e : g_entries) {
        std::unordered_set<std::string> seen;
        seen.reserve(e.topic_tokens.size() + e.question_tokens.size() + e.class_tokens.size());
        
        seen.insert(e.topic_tokens.begin(), e.topic_tokens.end());
        seen.insert(e.question_tokens.begin(), e.question_tokens.end());
        seen.insert(e.class_tokens.begin(), e.class_tokens.end());
        
        doc_tokens.push_back(std::move(seen));
    }

    g_idf = BuildIdf(doc_tokens);
}

// Score a single entry against a query using cached tokens.
double score_entry(const CachedEntry& entry,
                   const std::vector<std::string>& query_tokens,
                   const std::vector<std::string>& query_classes) {
    double score = 0.0;

    // 1. Class match bonus (strongest signal): exact Be class name match.
    //    Each matching class contributes a high fixed score weighted by IDF.
    for (const auto& qc : query_classes) {
        if (entry.class_tokens.count(qc)) {
            double idf = 1.0;
            auto it = g_idf.find(qc);
            if (it != g_idf.end()) idf = it->second;
            score += 5.0 * idf;
        }
    }

    // 2. Topic keyword match: tokens appearing in the entry's topic.
    for (const auto& qt : query_tokens) {
        if (entry.topic_tokens.count(qt)) {
            double idf = 1.0;
            auto it = g_idf.find(qt);
            if (it != g_idf.end()) idf = it->second;
            score += 3.0 * idf;
        }
    }

    // 3. Question keyword match: tokens appearing in the question field.
    for (const auto& qt : query_tokens) {
        if (entry.question_tokens.count(qt)) {
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
    int count = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            json::Value v = json::Value::parse(line);
            if (!v.is_obj()) continue;

            CachedEntry ce;
            const json::Value* topic = v.find("topic");
            const json::Value* question = v.find("question");
            const json::Value* answer = v.find("answer");
            if (!topic || !question || !answer) continue;
            ce.entry.topic = topic->as_str();
            ce.entry.question = question->as_str();
            ce.entry.answer = answer->as_str();
            
            const json::Value* classes = v.find("classes");
            if (classes && classes->is_arr()) {
                for (const auto& c : classes->as_arr()) {
                    if (c.is_str()) ce.entry.classes.push_back(c.as_str());
                }
            }

            // Pre-compute and cache tokens
            auto topic_toks = Tokenize(ce.entry.topic);
            ce.topic_tokens.insert(topic_toks.begin(), topic_toks.end());
            
            auto q_toks = Tokenize(ce.entry.question);
            ce.question_tokens.insert(q_toks.begin(), q_toks.end());
            
            // Pre-compute lowercase class names
            for (const auto& cl : ce.entry.classes) {
                ce.class_tokens.insert(ToLower(cl));
            }

            g_entries.push_back(std::move(ce));
            ++count;
        } catch (...) {
            // Skip malformed lines
        }
    }

    if (count > 0) build_idf();
    return count;
}

int RagEntryCount() {
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    return static_cast<int>(g_entries.size());
}

std::vector<RagResult> RagSearch(const std::string& query, int top_k) {
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    if (g_entries.empty() || query.empty()) return {};

    auto query_tokens = Tokenize(query);
    if (query_tokens.empty()) return {};

    auto query_classes = RagExtractClasses(query);
    for (auto& c : query_classes) c = ToLower(c);

    std::vector<RagResult> results;
    results.reserve(g_entries.size());

    for (const auto& e : g_entries) {
        double s = score_entry(e, query_tokens, query_classes);
        if (s > 0.0) {
            results.push_back({ &e.entry, s });
        }
    }

    if (results.empty()) return {};

    std::partial_sort(results.begin(),
                      results.begin() + std::min<size_t>(top_k, results.size()),
                      results.end(),
                      [](const RagResult& a, const RagResult& b) {
                          return a.score > b.score;
                      });

    if (results.size() > static_cast<size_t>(top_k))
        results.resize(top_k);

    return results;
}

std::string RagDefaultPath() {
    // 1. ./data/beapi-index.jsonl  (development)
    std::string p1 = "data/beapi-index.jsonl";
    struct stat st;
    if (::stat(p1.c_str(), &st) == 0) return p1;

    // 2. ~/config/settings/pluto/beapi-index.jsonl  (user-installed)
    std::string p2 = SettingsDir() + "/beapi-index.jsonl";
    if (::stat(p2.c_str(), &st) == 0) return p2;

    // 3. /boot/system/data/pluto/beapi-index.jsonl  (packaged)
    std::string p3 = "/boot/system/data/pluto/beapi-index.jsonl";
    if (::stat(p3.c_str(), &st) == 0) return p3;

    return "";
}

std::string RagFormatContext(const std::vector<RagResult>& results) {
    if (results.empty()) return "";

    std::string out;
    out += "=== BeAPI Reference (top matches) ===\n\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out += "[" + std::to_string(i + 1) + "] " + r.entry->topic + " (score: " + std::to_string(r.score) + ")\n";
        if (!r.entry->classes.empty()) {
            out += "    Classes: ";
            for (size_t j = 0; j < r.entry->classes.size(); ++j) {
                if (j) out += ", ";
                out += r.entry->classes[j];
            }
            out += "\n";
        }
        out += r.entry->answer + "\n\n";
    }
    return out;
}

int RagLoadCustom(const std::string& custom_path) {
    std::lock_guard<std::mutex> lock(g_rag_mutex);
    std::ifstream f(custom_path);
    if (!f) return 0;

    std::string line;
    int added = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            json::Value v = json::Value::parse(line);
            if (!v.is_obj()) continue;

            CachedEntry ce;
            const json::Value* topic = v.find("topic");
            const json::Value* question = v.find("question");
            const json::Value* answer = v.find("answer");
            if (!topic || !question || !answer) continue;
            ce.entry.topic = topic->as_str();
            ce.entry.question = question->as_str();
            ce.entry.answer = answer->as_str();
            
            const json::Value* classes = v.find("classes");
            if (classes && classes->is_arr()) {
                for (const auto& c : classes->as_arr()) {
                    if (c.is_str()) ce.entry.classes.push_back(c.as_str());
                }
            }

            // Pre-compute and cache tokens
            auto topic_toks = Tokenize(ce.entry.topic);
            ce.topic_tokens.insert(topic_toks.begin(), topic_toks.end());
            
            auto q_toks = Tokenize(ce.entry.question);
            ce.question_tokens.insert(q_toks.begin(), q_toks.end());
            
            // Pre-compute lowercase class names
            for (const auto& cl : ce.entry.classes) {
                ce.class_tokens.insert(ToLower(cl));
            }

            g_entries.push_back(std::move(ce));
            ++added;
        } catch (...) {
            // Skip malformed lines
        }
    }

    if (added > 0) build_idf();
    return added;
}

std::string RagCustomPath() {
    return SettingsDir() + "/beapi-custom.jsonl";
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
        BEntry entry;
        if (bdir.CreateEntry(filename.c_str(), &entry) != B_OK) continue;

        BNode node(&entry);
        if (node.InitCheck() != B_OK) continue;

        // Write content
        if (node.Write(e.answer.c_str(), e.answer.size()) != static_cast<ssize_t>(e.answer.size())) continue;

        // Write attributes
        if (node.WriteAttr("beapi:topic", B_STRING_TYPE, 0, e.topic.c_str(), e.topic.size() + 1) < 0) continue;
        
        std::string classes_str;
        for (size_t j = 0; j < e.classes.size(); ++j) {
            if (j) classes_str += ",";
            classes_str += e.classes[j];
        }
        if (node.WriteAttr("beapi:classes", B_STRING_TYPE, 0, classes_str.c_str(), classes_str.size() + 1) < 0) continue;

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
        return n > 0 ? 0 : 1;
    }

    if (cmd == "stats") {
        std::string path = RagDefaultPath();
        if (path.empty()) {
            std::fprintf(stderr, "Index not found.\n");
            return 1;
        }
        int n = RagInit(path);
        std::printf("Entries: %d\n", n);
        return n > 0 ? 0 : 1;
    }

    if (cmd == "export") {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: pluto beapi export <directory>\n");
            return 1;
        }
        std::string path = RagDefaultPath();
        if (path.empty()) {
            std::fprintf(stderr, "Index not found.\n");
            return 1;
        }
        int n = RagInit(path);
        int w = RagExportBfs(argv[2]);
        std::printf("Exported %d of %d entries to %s\n", w, n, argv[2]);
        return w > 0 ? 0 : 1;
    }

    // Default: search
    std::string query;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) query += ' ';
        query += argv[i];
    }

    int n = RagInit(RagDefaultPath());
    if (n == 0) {
        // Try custom path as fallback
        n = RagLoadCustom(RagCustomPath());
        if (n == 0) return 1;
    }

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