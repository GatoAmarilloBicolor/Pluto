#include "markdown.h"

#include <cstring>

namespace pluto {

namespace {

// ANSI escape sequences.
const char* kReset     = "\x1b[0m";
const char* kBold      = "\x1b[1m";
const char* kDim       = "\x1b[2m";
const char* kItalic    = "\x1b[3m";
const char* kUnderline = "\x1b[4m";
const char* kCyan      = "\x1b[36m";
const char* kMagenta   = "\x1b[35m";

// Restore the default text color (green) used for assistant output.
const char* kTextColor = "\x1b[32m";

// Check if a line is a horizontal rule (---, ***, ___) with optional spaces.
bool is_horizontal_rule(const std::string& line) {
	if (line.size() < 3) return false;
	char rule_char = 0;
	int count = 0;
	for (char c : line) {
		if (c == ' ' || c == '\t') continue;
		if (c == '-' || c == '*' || c == '_') {
			if (rule_char == 0) rule_char = c;
			else if (c != rule_char) return false;
			++count;
		} else {
			return false;
		}
	}
	return count >= 3;
}

// Check if a line starts with a code fence (``` or ~~~).
bool is_code_fence(const std::string& line) {
	size_t i = 0;
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
	if (i + 3 > line.size()) return false;
	char fc = line[i];
	if (fc != '`' && fc != '~') return false;
	int count = 0;
	while (i < line.size() && line[i] == fc) { ++count; ++i; }
	return count >= 3;
}

// Count leading '#' characters for ATX headers. Returns 0 if not a header.
int header_level(const std::string& line) {
	size_t i = 0;
	while (i < line.size() && line[i] == '#') ++i;
	if (i == 0 || i > 6) return 0;
	// Must be followed by space or end of line.
	if (i < line.size() && line[i] != ' ' && line[i] != '\t') return 0;
	return static_cast<int>(i);
}

// Strip leading "# " from a header line, returning just the text.
std::string header_text(const std::string& line, int level) {
	size_t i = static_cast<size_t>(level);
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
	// Also strip trailing '#' and whitespace.
	std::string text = line.substr(i);
	size_t end = text.size();
	while (end > 0 && (text[end - 1] == '#' || text[end - 1] == ' '))
		--end;
	return text.substr(0, end);
}

// Check for blockquote prefix ("> ").  Returns the text after the prefix.
bool is_blockquote(const std::string& line, std::string& content) {
	size_t i = 0;
	while (i < line.size() && line[i] == ' ') ++i;
	if (i < line.size() && line[i] == '>') {
		++i;
		if (i < line.size() && line[i] == ' ') ++i;
		content = line.substr(i);
		return true;
	}
	return false;
}

// Check for unordered list item (- or * followed by space).
bool is_unordered_list(const std::string& line, size_t& indent, std::string& content) {
	size_t i = 0;
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
	indent = i;
	if (i < line.size() && (line[i] == '-' || line[i] == '*')) {
		if (i + 1 < line.size() && line[i + 1] == ' ') {
			content = line.substr(i + 2);
			return true;
		}
	}
	return false;
}

// Check for ordered list item (digits followed by . and space).
bool is_ordered_list(const std::string& line, size_t& indent,
                     std::string& number, std::string& content) {
	size_t i = 0;
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
	indent = i;
	size_t num_start = i;
	while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
	if (i == num_start || i >= line.size()) return false;
	if (line[i] == '.' && i + 1 < line.size() && line[i + 1] == ' ') {
		number = line.substr(num_start, i - num_start);
		content = line.substr(i + 2);
		return true;
	}
	return false;
}

}  // namespace

std::string TerminalMarkdown::_RenderInline(const std::string& text) {
	std::string out;
	out.reserve(text.size() + 64);
	size_t i = 0;
	size_t len = text.size();

	while (i < len) {
		// Backslash escape: \* \` \_ etc — emit literal char, skip formatting.
		if (text[i] == '\\' && i + 1 < len) {
			char next = text[i + 1];
			if (next == '`' || next == '*' || next == '_' || next == '\\'
				|| next == '[' || next == ']' || next == '~') {
				out += next;
				i += 2;
				continue;
			}
		}

		// Inline code: `...`
		if (text[i] == '`') {
			size_t end = text.find('`', i + 1);
			if (end != std::string::npos) {
				out += kCyan;
				out += text.substr(i + 1, end - i - 1);
				out += kTextColor;
				i = end + 1;
				continue;
			}
		}

		// Bold: **...**
		if (i + 1 < len && text[i] == '*' && text[i + 1] == '*') {
			size_t end = text.find("**", i + 2);
			if (end != std::string::npos && end > i + 2) {
				out += kBold;
				out += _RenderInline(text.substr(i + 2, end - i - 2));
				out += "\x1b[22m";  // reset bold only
				out += kTextColor;
				i = end + 2;
				continue;
			}
		}

		// Bold: __...__
		if (i + 1 < len && text[i] == '_' && text[i + 1] == '_') {
			size_t end = text.find("__", i + 2);
			if (end != std::string::npos && end > i + 2) {
				out += kBold;
				out += _RenderInline(text.substr(i + 2, end - i - 2));
				out += "\x1b[22m";
				out += kTextColor;
				i = end + 2;
				continue;
			}
		}

		// Italic: *...*  (single asterisk, not followed by another)
		if (text[i] == '*' && (i + 1 >= len || text[i + 1] != '*')) {
			size_t end = text.find('*', i + 1);
			if (end != std::string::npos && end > i + 1 &&
			    (end + 1 >= len || text[end + 1] != '*')) {
				out += kItalic;
				out += _RenderInline(text.substr(i + 1, end - i - 1));
				out += "\x1b[23m";  // reset italic only
				out += kTextColor;
				i = end + 1;
				continue;
			}
		}

		// Italic: _..._ (single underscore, not followed by another)
		if (text[i] == '_' && (i + 1 >= len || text[i + 1] != '_')) {
			// Only treat as italic if preceded by space/start and followed
			// by non-underscore content with a closing _.
			bool word_boundary = (i == 0 || text[i - 1] == ' ');
			if (word_boundary) {
				size_t end = text.find('_', i + 1);
				if (end != std::string::npos && end > i + 1 &&
				    (end + 1 >= len || text[end + 1] != '_')) {
					out += kItalic;
					out += _RenderInline(text.substr(i + 1, end - i - 1));
					out += "\x1b[23m";
					out += kTextColor;
					i = end + 1;
					continue;
				}
			}
		}

		out += text[i++];
	}
	return out;
}

std::string TerminalMarkdown::_RenderLine(const std::string& line) {
	std::string out;

	// Code fence toggle.
	if (is_code_fence(line)) {
		fInCodeBlock = !fInCodeBlock;
		if (fInCodeBlock) {
			// Extract language tag if present.
			size_t i = 0;
			while (i < line.size() && (line[i] == ' ' || line[i] == '`' || line[i] == '~')) ++i;
			std::string lang = line.substr(i);
			while (!lang.empty() && (lang.back() == ' ' || lang.back() == '\t'))
				lang.pop_back();

			out += kDim;
			out += "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80";  // ───
			if (!lang.empty()) {
				out += " ";
				out += lang;
			}
			out += kReset;
			out += kTextColor;
		} else {
			out += kDim;
			out += "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80";  // ───
			out += kReset;
			out += kTextColor;
		}
		return out;
	}

	// Inside code block: render as-is with cyan color.
	if (fInCodeBlock) {
		out += kCyan;
		out += "  ";
		out += line;
		out += kTextColor;
		return out;
	}

	// Horizontal rule.
	if (is_horizontal_rule(line)) {
		out += kDim;
		out += "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
		       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
		       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
		       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
		       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80";  // ──── x20
		out += kReset;
		out += kTextColor;
		return out;
	}

	// ATX header.
	int hlevel = header_level(line);
	if (hlevel > 0) {
		std::string htext = header_text(line, hlevel);
		out += kBold;
		if (hlevel <= 2) out += kUnderline;
		out += _RenderInline(htext);
		out += kReset;
		out += kTextColor;
		return out;
	}

	// Blockquote.
	std::string bq_content;
	if (is_blockquote(line, bq_content)) {
		out += kDim;
		out += "\xe2\x94\x82 ";  // │
		out += kReset;
		out += kTextColor;
		out += kItalic;
		out += _RenderInline(bq_content);
		out += "\x1b[23m";
		return out;
	}

	// Unordered list.
	size_t indent;
	std::string item_content;
	if (is_unordered_list(line, indent, item_content)) {
		for (size_t s = 0; s < indent; ++s) out += ' ';
		out += kMagenta;
		out += "\xe2\x80\xa2 ";  // •
		out += kTextColor;
		out += _RenderInline(item_content);
		return out;
	}

	// Ordered list.
	std::string number;
	if (is_ordered_list(line, indent, number, item_content)) {
		for (size_t s = 0; s < indent; ++s) out += ' ';
		out += kMagenta;
		out += number;
		out += ". ";
		out += kTextColor;
		out += _RenderInline(item_content);
		return out;
	}

	// Plain text with inline formatting.
	out += _RenderInline(line);
	return out;
}

std::string TerminalMarkdown::Feed(const std::string& chunk) {
	std::string out;
	fLineBuf += chunk;

	// Process all complete lines.
	size_t pos = 0;
	while (pos < fLineBuf.size()) {
		size_t nl = fLineBuf.find('\n', pos);
		if (nl == std::string::npos) break;  // incomplete line — keep in buffer

		std::string line = fLineBuf.substr(pos, nl - pos);
		// Strip trailing \r.
		if (!line.empty() && line.back() == '\r') line.pop_back();

		out += _RenderLine(line);
		out += '\n';
		pos = nl + 1;
	}

	// Keep any remaining partial line in the buffer.
	fLineBuf = fLineBuf.substr(pos);
	return out;
}

std::string TerminalMarkdown::Flush() {
	std::string out;
	if (!fLineBuf.empty()) {
		out += _RenderLine(fLineBuf);
		fLineBuf.clear();
	}
	// Reset code block state in case stream ended mid-block.
	fInCodeBlock = false;
	return out;
}

void TerminalMarkdown::Reset() {
	fLineBuf.clear();
	fInCodeBlock = false;
}

}  // namespace pluto
