#pragma once

// Minimal syntax highlighter for fenced code blocks in Pluto-gui.
// Produces a sequence of (text, token_type) spans suitable for
// SetFontAndColor in a BTextView.

#include <string>
#include <vector>

namespace pluto {
namespace gui {

enum class TokenType {
	Default,
	Keyword,
	String,
	Comment,
	Number,
	Preprocessor,
	Type,
};

struct Token {
	std::string text;
	TokenType   type;
};

// Tokenize source code for syntax highlighting.
// `lang` is the fenced code block language tag (e.g. "cpp", "python", "sh").
// Returns a flat list of tokens covering the entire input.
inline std::vector<Token> highlight(const std::string& code,
                                    const std::string& lang) {
	std::vector<Token> tokens;
	if (code.empty()) return tokens;

	// Determine language family.
	enum Lang { LANG_NONE, LANG_CPP, LANG_PY, LANG_SH, LANG_JS };
	Lang family = LANG_NONE;

	if (lang == "cpp" || lang == "c++" || lang == "c" || lang == "h"
	    || lang == "hpp" || lang == "cxx" || lang == "cc")
		family = LANG_CPP;
	else if (lang == "python" || lang == "py")
		family = LANG_PY;
	else if (lang == "sh" || lang == "bash" || lang == "shell" || lang == "zsh")
		family = LANG_SH;
	else if (lang == "js" || lang == "javascript" || lang == "ts"
	         || lang == "typescript" || lang == "json")
		family = LANG_JS;

	if (family == LANG_NONE) {
		// No highlighting, return as-is.
		tokens.push_back({code, TokenType::Default});
		return tokens;
	}

	// Simple keyword sets.
	auto is_cpp_keyword = [](const std::string& w) -> bool {
		static const char* kw[] = {
			"auto","break","case","catch","class","const","constexpr",
			"continue","default","delete","do","else","enum","explicit",
			"extern","false","for","friend","goto","if","inline",
			"namespace","new","noexcept","nullptr","operator","override",
			"private","protected","public","return","sizeof","static",
			"static_cast","dynamic_cast","reinterpret_cast","const_cast",
			"struct","switch","template","this","throw","true","try",
			"typedef","typeid","typename","union","using","virtual",
			"void","volatile","while","final",nullptr
		};
		for (const char** p = kw; *p; ++p) if (w == *p) return true;
		return false;
	};
	auto is_cpp_type = [](const std::string& w) -> bool {
		static const char* kw[] = {
			"bool","char","double","float","int","long","short",
			"signed","unsigned","size_t","int8_t","int16_t","int32_t",
			"int64_t","uint8_t","uint16_t","uint32_t","uint64_t",
			"string","vector","map","set","unique_ptr","shared_ptr",
			"std","BView","BWindow","BMessage","BMessenger","BApplication",
			"BLooper","BHandler","BRect","BPoint","BFont","BButton",
			"BTextView","BTextControl","BMenuField","BMenuItem","BMenu",
			"BAlert","BNotification","BQuery","BNode","BVolume",
			"status_t","thread_id","team_id","bigtime_t","rgb_color",
			nullptr
		};
		for (const char** p = kw; *p; ++p) if (w == *p) return true;
		return false;
	};
	auto is_py_keyword = [](const std::string& w) -> bool {
		static const char* kw[] = {
			"and","as","assert","async","await","break","class",
			"continue","def","del","elif","else","except","False",
			"finally","for","from","global","if","import","in","is",
			"lambda","None","nonlocal","not","or","pass","raise",
			"return","True","try","while","with","yield",nullptr
		};
		for (const char** p = kw; *p; ++p) if (w == *p) return true;
		return false;
	};
	auto is_sh_keyword = [](const std::string& w) -> bool {
		static const char* kw[] = {
			"if","then","else","elif","fi","for","while","do","done",
			"case","esac","in","function","return","exit","local",
			"export","source","echo","read","set","unset","shift",
			"true","false","test",nullptr
		};
		for (const char** p = kw; *p; ++p) if (w == *p) return true;
		return false;
	};
	auto is_js_keyword = [](const std::string& w) -> bool {
		static const char* kw[] = {
			"async","await","break","case","catch","class","const",
			"continue","debugger","default","delete","do","else",
			"export","extends","false","finally","for","from",
			"function","if","import","in","instanceof","let","new",
			"null","of","return","static","super","switch","this",
			"throw","true","try","typeof","undefined","var","void",
			"while","with","yield",nullptr
		};
		for (const char** p = kw; *p; ++p) if (w == *p) return true;
		return false;
	};

	auto is_keyword = [&](const std::string& w) -> bool {
		switch (family) {
			case LANG_CPP: return is_cpp_keyword(w);
			case LANG_PY:  return is_py_keyword(w);
			case LANG_SH:  return is_sh_keyword(w);
			case LANG_JS:  return is_js_keyword(w);
			default: return false;
		}
	};
	auto is_type = [&](const std::string& w) -> bool {
		if (family == LANG_CPP) return is_cpp_type(w);
		return false;
	};

	// Character helpers.
	auto is_ident_start = [](char c) { return (c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z') || c == '_'; };
	auto is_ident = [](char c) { return (c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; };
	auto is_digit = [](char c) { return c >= '0' && c <= '9'; };

	size_t i = 0;
	std::string pending;

	auto flush = [&]() {
		if (!pending.empty()) {
			tokens.push_back({pending, TokenType::Default});
			pending.clear();
		}
	};

	while (i < code.size()) {
		char c = code[i];

		// Line comment: // (cpp/js) or # (py/sh)
		if ((family == LANG_CPP || family == LANG_JS)
		    && i + 1 < code.size() && c == '/' && code[i+1] == '/') {
			flush();
			size_t end = code.find('\n', i);
			if (end == std::string::npos) end = code.size();
			else ++end;  // include newline
			tokens.push_back({code.substr(i, end - i), TokenType::Comment});
			i = end;
			continue;
		}
		if ((family == LANG_PY || family == LANG_SH) && c == '#') {
			flush();
			size_t end = code.find('\n', i);
			if (end == std::string::npos) end = code.size();
			else ++end;
			tokens.push_back({code.substr(i, end - i), TokenType::Comment});
			i = end;
			continue;
		}

		// Block comment: /* ... */ (cpp/js)
		if ((family == LANG_CPP || family == LANG_JS)
		    && i + 1 < code.size() && c == '/' && code[i+1] == '*') {
			flush();
			size_t end = code.find("*/", i + 2);
			if (end == std::string::npos) end = code.size();
			else end += 2;
			tokens.push_back({code.substr(i, end - i), TokenType::Comment});
			i = end;
			continue;
		}

		// Preprocessor: # at start of line (cpp)
		if (family == LANG_CPP && c == '#') {
			// Check if at start of line (or after whitespace).
			bool at_start = (i == 0);
			if (!at_start && i > 0 && (code[i-1] == '\n'))
				at_start = true;
			if (at_start) {
				flush();
				size_t end = code.find('\n', i);
				if (end == std::string::npos) end = code.size();
				else ++end;
				tokens.push_back({code.substr(i, end - i), TokenType::Preprocessor});
				i = end;
				continue;
			}
		}

		// String literals: "..." or '...'
		if (c == '"' || c == '\'') {
			// In shell, single quotes don't escape.
			flush();
			char quote = c;
			size_t start = i++;
			while (i < code.size()) {
				if (code[i] == '\\' && family != LANG_SH && quote == '\'') {
					++i;  // skip escaped char
				} else if (code[i] == '\\' && (family != LANG_SH || quote == '"')) {
					i += 2;
					continue;
				}
				if (code[i] == quote) { ++i; break; }
				++i;
			}
			tokens.push_back({code.substr(start, i - start), TokenType::String});
			continue;
		}

		// Numbers: digits (possibly with . or x for hex)
		if (is_digit(c) || (c == '.' && i + 1 < code.size() && is_digit(code[i+1]))) {
			flush();
			size_t start = i;
			if (c == '0' && i + 1 < code.size()
			    && (code[i+1] == 'x' || code[i+1] == 'X')) {
				i += 2;
				while (i < code.size() && ((code[i] >= '0' && code[i] <= '9')
				       || (code[i] >= 'a' && code[i] <= 'f')
				       || (code[i] >= 'A' && code[i] <= 'F')))
					++i;
			} else {
				while (i < code.size() && (is_digit(code[i]) || code[i] == '.'
				       || code[i] == 'e' || code[i] == 'E'))
					++i;
			}
			// Suffix: u, l, f, etc.
			while (i < code.size() && (code[i] == 'u' || code[i] == 'U'
			       || code[i] == 'l' || code[i] == 'L' || code[i] == 'f'
			       || code[i] == 'F'))
				++i;
			tokens.push_back({code.substr(start, i - start), TokenType::Number});
			continue;
		}

		// Identifiers and keywords.
		if (is_ident_start(c)) {
			flush();
			size_t start = i;
			while (i < code.size() && is_ident(code[i])) ++i;
			// Namespaced: std::string — treat :: as part of ident.
			while (i + 1 < code.size() && code[i] == ':' && code[i+1] == ':') {
				i += 2;
				while (i < code.size() && is_ident(code[i])) ++i;
			}
			std::string word = code.substr(start, i - start);
			// Check for namespace-qualified: only check the last segment.
			size_t last_colon = word.rfind("::");
			std::string check = (last_colon != std::string::npos)
				? word.substr(last_colon + 2) : word;
			if (is_keyword(check))
				tokens.push_back({word, TokenType::Keyword});
			else if (is_type(check))
				tokens.push_back({word, TokenType::Type});
			else
				tokens.push_back({word, TokenType::Default});
			continue;
		}

		// Everything else: accumulate as default.
		pending += c;
		++i;
	}
	flush();
	return tokens;
}

}  // namespace gui
}  // namespace pluto
