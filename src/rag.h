// Local BeAPI knowledge retrieval for Haiku-native code generation.
// Loads a JSONL index of ~390 BeAPI code snippets and provides
// keyword-based search with TF-IDF-like scoring.

#pragma once

#include <string>
#include <vector>

namespace pluto {

struct RagEntry {
	std::string topic;
	std::string question;
	std::string answer;
	std::vector<std::string> classes;  // Be classes mentioned (BView, BWindow, ...)
};

struct RagResult {
	const RagEntry* entry;
	double score;
};

// Load the BeAPI index from a JSONL file. Returns the number of entries
// loaded, or 0 on error. Can be called multiple times (replaces index).
int RagInit(const std::string& index_path);

// Search the index for snippets relevant to the query.
// Returns up to top_k results sorted by descending score.
// Empty vector if index not loaded or no match above threshold.
std::vector<RagResult> RagSearch(const std::string& query, int top_k = 3);

// Return the number of entries currently loaded.
int RagEntryCount();

// Extract Be class names from text (B followed by uppercase letter).
std::vector<std::string> RagExtractClasses(const std::string& text);

// Find the default beapi-index.jsonl path. Searches:
//   1. ./data/beapi-index.jsonl  (development)
//   2. ~/config/settings/pluto/beapi-index.jsonl  (user-installed)
//   3. /boot/system/data/pluto/beapi-index.jsonl  (packaged)
// Returns empty string if not found.
std::string RagDefaultPath();

// Format RAG results as a context block suitable for system prompt injection.
// Returns empty string if results is empty.
std::string RagFormatContext(const std::vector<RagResult>& results);

// Load additional entries from a custom JSONL file (user-contributed snippets).
// Merges into the existing index. Returns the number of entries added.
int RagLoadCustom(const std::string& custom_path);

// Find the custom JSONL path: ~/config/settings/pluto/beapi-custom.jsonl
std::string RagCustomPath();

// Export the loaded index as BFS files with attributes in the given directory.
// Creates one file per entry with beapi:topic, beapi:classes attributes.
// Returns the number of files written.
int RagExportBfs(const std::string& dir);

// CLI: `pluto beapi <query|update|export|stats>` — search and manage.
int RunBeapiCli(int argc, char** argv);

}  // namespace pluto
