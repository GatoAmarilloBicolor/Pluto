// Generic YAML-style frontmatter parser for markdown files.
// Parses the `---` delimited header and returns key-value pairs.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace pluto {


// Parsed frontmatter: a map of key -> value (strings).
// For comma-separated fields, the raw string is stored; use
// SplitCsv() to break it into a vector.
struct Frontmatter {
	std::unordered_map<std::string, std::string> fields;
	std::string body;   // content after the frontmatter block

	// Get a field value, or empty string if not present.
	std::string get(const std::string& key) const;

	// Get a field as a comma-separated list of trimmed strings.
	std::vector<std::string> get_list(const std::string& key) const;

	// Check if a field exists.
	bool has(const std::string& key) const;
};


// Parse frontmatter from markdown content.
// If no frontmatter is found (no leading `---`), returns a Frontmatter
// with empty fields and body == content.
Frontmatter ParseFrontmatter(const std::string& content);


// Split a comma-separated string into trimmed values.
// Empty values are skipped.
std::vector<std::string> SplitCsv(const std::string& s);


}  // namespace pluto
