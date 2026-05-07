// Wiki LLM — markdown-based knowledge base with BFS attribute indexing.
// Each entry is a .md file with YAML frontmatter in the wiki directory.
// Files are indexed in-memory using TF-IDF for keyword search, and can
// be queried via BFS attributes (wiki:topic, wiki:tags, wiki:source).

#pragma once

#include <string>
#include <vector>

namespace pluto {

struct WikiEntry {
	std::string path;           // absolute file path
	std::string slug;           // filename without extension
	std::string topic;          // from frontmatter: topic
	std::vector<std::string> tags;   // from frontmatter: tags
	std::string source;         // from frontmatter: source (URL or path)
	std::string date;           // from frontmatter: date (ISO)
	std::string body;           // markdown body after frontmatter
};

struct WikiResult {
	const WikiEntry* entry;
	double score;
};

// Initialize the wiki index from a directory of .md files.
// Parses frontmatter and builds IDF table. Returns entry count.
int WikiInit(const std::string& dir);

// Search the wiki for entries matching the query.
// Returns up to top_k results sorted by descending score.
std::vector<WikiResult> WikiSearch(const std::string& query, int top_k = 3);

// Return the number of entries currently loaded.
int WikiEntryCount();

// List all loaded entries (for `pluto wiki list`).
const std::vector<WikiEntry>& WikiEntries();

// Save a new wiki entry. Writes a .md file with frontmatter and sets
// BFS attributes. Returns the file path on success, empty on failure.
std::string WikiSave(const std::string& dir, const std::string& topic,
                     const std::vector<std::string>& tags,
                     const std::string& source, const std::string& body);

// Default wiki directory. Searches:
//   1. ./wiki/  (project-local)
//   2. ~/config/settings/pluto/wiki/  (user-global)
// Returns empty string if neither exists (caller should create user-global).
std::string WikiDefaultDir();

// Ensure the wiki directory exists (creates if needed). Returns path.
std::string WikiEnsureDir();

// Format wiki results as context block for system prompt injection.
std::string WikiFormatContext(const std::vector<WikiResult>& results);

// CLI: `pluto wiki <ingest|search|list|stats>` — manage the wiki.
int RunWikiCli(int argc, char** argv);

// Generate the ingest prompt for analyzing content. This is the system
// prompt sent to the LLM when `pluto wiki ingest` is invoked.
std::string WikiIngestPrompt();

}  // namespace pluto
