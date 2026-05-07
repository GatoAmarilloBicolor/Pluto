// Generic YAML-style frontmatter parser.

#include "frontmatter.h"

#include "util.h"

#include <sstream>

namespace pluto {


std::string
Frontmatter::get(const std::string& key) const
{
	auto it = fields.find(key);
	if (it != fields.end()) return it->second;
	return {};
}

std::vector<std::string>
Frontmatter::get_list(const std::string& key) const
{
	auto it = fields.find(key);
	if (it == fields.end()) return {};
	return SplitCsv(it->second);
}

bool
Frontmatter::has(const std::string& key) const
{
	return fields.count(key) > 0;
}

Frontmatter
ParseFrontmatter(const std::string& content)
{
	Frontmatter fm;

	// Must start with "---\n".
	if (content.size() < 4 || content.substr(0, 4) != "---\n") {
		fm.body = content;
		return fm;
	}

	// Find closing "---" on its own line.
	// Accept both "\n---\n" and "\n---" at EOF.
	size_t end = content.find("\n---", 4);
	if (end == std::string::npos) {
		fm.body = content;
		return fm;
	}

	std::string header = content.substr(4, end - 4);

	// Body starts after the closing "---" + newline.
	size_t body_start = end + 4; // skip "\n---"
	if (body_start < content.size() && content[body_start] == '\n') body_start++;
	fm.body = content.substr(body_start);

	// Parse key: value lines.
	std::istringstream ss(header);
	std::string line;
	while (std::getline(ss, line)) {
		size_t colon = line.find(':');
		if (colon == std::string::npos) continue;
		std::string key = Trim(line.substr(0, colon));
		std::string val = Trim(line.substr(colon + 1));
		if (!key.empty()) fm.fields[key] = val;
	}

	return fm;
}

std::vector<std::string>
SplitCsv(const std::string& s)
{
	std::vector<std::string> result;
	std::istringstream ss(s);
	std::string tok;
	while (std::getline(ss, tok, ',')) {
		std::string trimmed = Trim(tok);
		if (!trimmed.empty()) result.push_back(trimmed);
	}
	return result;
}


} // namespace pluto
