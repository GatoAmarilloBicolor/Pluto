#include "markdown.h"

#include <algorithm>
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

// Box-drawing characters used for tables.
const char* kBoxH  = "\xe2\x94\x80";  // ─
const char* kBoxV  = "\xe2\x94\x82";  // │
const char* kBoxTL = "\xe2\x94\x8c";  // ┌
const char* kBoxTR = "\xe2\x94\x90";  // ┐
const char* kBoxBL = "\xe2\x94\x94";  // └
const char* kBoxBR = "\xe2\x94\x98";  // ┘
const char* kBoxVR = "\xe2\x94\x9c";  // ├
const char* kBoxVL = "\xe2\x94\xa4";  // ┤
const char* kBoxTD = "\xe2\x94\xac";  // ┬
const char* kBoxTU = "\xe2\x94\xb4";  // ┴
const char* kBoxX  = "\xe2\x94\xbc";  // ┼

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

// A candidate table row: a trimmed line containing at least one '|'
// outside of a code span.  Loosely matched here; _RenderTable() falls
// back to plain lines if the buffered run doesn't actually look like
// a table (no valid separator row).
bool is_table_row(const std::string& line) {
	size_t i = 0;
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
	if (i >= line.size()) return false;
	return line.find('|', i) != std::string::npos;
}

// Check whether a row is a valid table separator, e.g. "|---|:--:|---|".
bool is_table_separator(const std::string& row) {
	std::string s = row;
	size_t b = 0, e = s.size();
	while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
	while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
	s = s.substr(b, e - b);
	if (s.empty()) return false;
	if (s.front() == '|') s.erase(0, 1);
	if (!s.empty() && s.back() == '|') s.pop_back();
	if (s.empty()) return false;

	bool any_dash = false;
	for (char c : s) {
		if (c == '-') any_dash = true;
		else if (c != ':' && c != '|' && c != ' ') return false;
	}
	return any_dash;
}

// Split a "| a | b |" row into trimmed cell strings.
std::vector<std::string> split_table_row(const std::string& row) {
	std::string s = row;
	size_t b = 0, e = s.size();
	while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
	while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
	s = s.substr(b, e - b);
	if (!s.empty() && s.front() == '|') s.erase(0, 1);
	if (!s.empty() && s.back() == '|') s.pop_back();

	std::vector<std::string> cells;
	size_t start = 0;
	for (size_t i = 0; i <= s.size(); ++i) {
		if (i == s.size() || s[i] == '|') {
			std::string cell = s.substr(start, i - start);
			size_t cb = 0, ce = cell.size();
			while (cb < ce && cell[cb] == ' ') ++cb;
			while (ce > cb && cell[ce - 1] == ' ') --ce;
			cells.push_back(cell.substr(cb, ce - cb));
			start = i + 1;
		}
	}
	return cells;
}

// Count visible (non-ANSI, non-UTF8-continuation) characters, so
// column padding lines up even after inline formatting is applied.
size_t visible_len(const std::string& s) {
	size_t n = 0, i = 0;
	while (i < s.size()) {
		if (s[i] == '\x1b') {
			while (i < s.size() && s[i] != 'm') ++i;
			if (i < s.size()) ++i;
			continue;
		}
		unsigned char c = static_cast<unsigned char>(s[i]);
		if ((c & 0xC0) != 0x80) ++n;
		++i;
	}
	return n;
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

		// Link: [text](url)
		if (text[i] == '[') {
			size_t close = text.find(']', i + 1);
			if (close != std::string::npos && close + 1 < len && text[close + 1] == '(') {
				size_t paren_close = text.find(')', close + 2);
				if (paren_close != std::string::npos) {
					std::string link_text = text.substr(i + 1, close - i - 1);
					std::string url = text.substr(close + 2, paren_close - close - 2);
					out += kUnderline;
					out += kCyan;
					out += _RenderInline(link_text);
					out += "\x1b[24m";  // underline off
					out += kTextColor;
					out += kDim;
					out += " (";
					out += url;
					out += ")";
					out += kReset;
					out += kTextColor;
					i = paren_close + 1;
					continue;
				}
			}
		}

		// Strikethrough: ~~...~~
		if (i + 1 < len && text[i] == '~' && text[i + 1] == '~') {
			size_t end = text.find("~~", i + 2);
			if (end != std::string::npos && end > i + 2) {
				out += "\x1b[9m";  // strikethrough on
				out += _RenderInline(text.substr(i + 2, end - i - 2));
				out += "\x1b[29m";  // strikethrough off
				out += kTextColor;
				i = end + 2;
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

	// Unordered list. Vary the bullet glyph by nesting depth (2 spaces
	// per level) so nested lists are visually distinguishable.
	size_t indent;
	std::string item_content;
	if (is_unordered_list(line, indent, item_content)) {
		for (size_t s = 0; s < indent; ++s) out += ' ';
		out += kMagenta;
		int depth = static_cast<int>(indent / 2);
		switch (depth % 3) {
			case 0:  out += "\xe2\x80\xa2 "; break;  // •
			case 1:  out += "\xe2\x97\xa6 "; break;  // ◦
			default: out += "\xe2\x96\xaa "; break;  // ▪
		}
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

std::string TerminalMarkdown::_RenderTable(const std::vector<std::string>& rows) {
	std::string out;

	// Need a header + separator row to treat this as a real table;
	// otherwise fall back to rendering the buffered lines as-is.
	if (rows.size() < 2 || !is_table_separator(rows[1]) || fInCodeBlock) {
		for (const auto& r : rows) {
			out += _RenderInline(r);
			out += '\n';
		}
		return out;
	}

	std::vector<std::vector<std::string>> data;
	data.push_back(split_table_row(rows[0]));
	for (size_t i = 2; i < rows.size(); ++i)
		data.push_back(split_table_row(rows[i]));

	size_t ncols = data[0].size();
	for (auto& row : data)
		if (row.size() < ncols) row.resize(ncols);

	// Render each cell's inline formatting up front so column widths
	// are measured on the final visible text.
	std::vector<std::vector<std::string>> rendered(data.size());
	std::vector<size_t> widths(ncols, 0);
	for (size_t r = 0; r < data.size(); ++r) {
		rendered[r].resize(ncols);
		for (size_t c = 0; c < ncols; ++c) {
			rendered[r][c] = _RenderInline(data[r][c]);
			widths[c] = std::max(widths[c], visible_len(rendered[r][c]));
		}
	}

	auto pad = [](const std::string& cell, size_t width) {
		size_t len = visible_len(cell);
		return cell + std::string(width > len ? width - len : 0, ' ');
	};
	auto border = [&](const char* left, const char* mid, const char* right) {
		out += kDim;
		out += left;
		for (size_t c = 0; c < ncols; ++c) {
			for (size_t i = 0; i < widths[c] + 2; ++i) out += kBoxH;
			out += (c + 1 < ncols) ? mid : right;
		}
		out += kReset;
		out += kTextColor;
		out += '\n';
	};

	border(kBoxTL, kBoxTD, kBoxTR);
	for (size_t r = 0; r < rendered.size(); ++r) {
		out += kDim; out += kBoxV; out += kReset; out += kTextColor;
		for (size_t c = 0; c < ncols; ++c) {
			out += ' ';
			out += (r == 0) ? kBold : "";
			out += pad(rendered[r][c], widths[c]);
			out += (r == 0) ? "\x1b[22m" : "";
			out += kTextColor;
			out += ' ';
			out += kDim; out += kBoxV; out += kReset; out += kTextColor;
		}
		out += '\n';
		if (r == 0) border(kBoxVR, kBoxX, kBoxVL);
	}
	border(kBoxBL, kBoxTU, kBoxBR);

	return out;
}

void TerminalMarkdown::_ProcessLine(const std::string& line, std::string& out,
                                     bool add_newline) {
	if (!fInCodeBlock && is_table_row(line)) {
		fTableBuf.push_back(line);
		return;
	}
	if (!fTableBuf.empty()) {
		out += _RenderTable(fTableBuf);
		fTableBuf.clear();
	}
	out += _RenderLine(line);
	if (add_newline) out += '\n';
}

std::string TerminalMarkdown::Feed(const std::string& chunk) {
	fLineBuf += chunk;

	// Fast path: no newline in buffer, nothing to render yet.
	if (fLineBuf.find('\n') == std::string::npos)
		return {};

	std::string out;
	out.reserve(fLineBuf.size() + 64);

	// Process all complete lines.
	size_t pos = 0;
	while (pos < fLineBuf.size()) {
		size_t nl = fLineBuf.find('\n', pos);
		if (nl == std::string::npos) break;

		std::string line = fLineBuf.substr(pos, nl - pos);
		if (!line.empty() && line.back() == '\r') line.pop_back();

		_ProcessLine(line, out);
		pos = nl + 1;
	}

	// Erase processed content, keep partial line.
	if (pos > 0)
		fLineBuf.erase(0, pos);
	return out;
}

std::string TerminalMarkdown::Flush() {
	std::string out;
	if (!fLineBuf.empty()) {
		_ProcessLine(fLineBuf, out, /*add_newline=*/false);
		fLineBuf.clear();
	}
	if (!fTableBuf.empty()) {
		out += _RenderTable(fTableBuf);
		fTableBuf.clear();
	}
	// Reset code block state in case stream ended mid-block.
	fInCodeBlock = false;
	return out;
}

void TerminalMarkdown::Reset() {
	fLineBuf.clear();
	fInCodeBlock = false;
	fTableBuf.clear();
}

}  // namespace pluto
