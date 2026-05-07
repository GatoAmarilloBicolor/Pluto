#include "json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace pluto::json {

namespace {

void serialize_string(std::string& out, const std::string& s) {
	out += '"';
	for (char c : s) {
		switch (c) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			case '\b': out += "\\b";  break;
			case '\f': out += "\\f";  break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) {
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x",
						static_cast<unsigned char>(c));
					out += buf;
				} else {
					out += c;
				}
		}
	}
	out += '"';
}

void serialize_value(std::string& out, const Value& v) {
	if (v.is_null()) { out += "null"; return; }
	if (v.is_bool()) { out += v.as_bool() ? "true" : "false"; return; }
	if (v.is_num()) {
		double n = v.as_num();
		if (std::isnan(n) || std::isinf(n)) { out += "null"; return; }
		char buf[32];
		if (n == std::floor(n) && std::abs(n) < 1e16) {
			std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n));
		} else {
			std::snprintf(buf, sizeof(buf), "%.17g", n);
		}
		out += buf;
		return;
	}
	if (v.is_str()) { serialize_string(out, v.as_str()); return; }
	if (v.is_arr()) {
		out += '[';
		bool first = true;
		for (const auto& e : v.as_arr()) {
			if (!first) out += ',';
			first = false;
			serialize_value(out, e);
		}
		out += ']';
		return;
	}
	if (v.is_obj()) {
		out += '{';
		bool first = true;
		for (const auto& [k, val] : v.as_obj()) {
			if (!first) out += ',';
			first = false;
			serialize_string(out, k);
			out += ':';
			serialize_value(out, val);
		}
		out += '}';
		return;
	}
}

class Parser {
public:
	Parser(const std::string& s) : fSrc(s), fPos(0), fDepth(0) {}

	Value parse_value() {
		skip_ws();
		if (fPos >= fSrc.size()) fail("unexpected end of input");
		if (fDepth > 256) fail("nesting too deep");
		char c = fSrc[fPos];
		if (c == '{') return parse_object();
		if (c == '[') return parse_array();
		if (c == '"') return Value(parse_string());
		if (c == 't' || c == 'f') return parse_bool();
		if (c == 'n') return parse_null();
		if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
		fail("unexpected character");
	}

	void expect_eof() {
		skip_ws();
		if (fPos != fSrc.size()) fail("trailing content");
	}

private:
	const std::string& fSrc;
	size_t fPos;
	int fDepth;

	[[noreturn]] void fail(const char* msg) {
		throw std::runtime_error(std::string("json parse: ") + msg +
			" at offset " + std::to_string(fPos));
	}

	void skip_ws() {
		while (fPos < fSrc.size()) {
			char c = fSrc[fPos];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++fPos;
			else break;
		}
	}

	void expect(char c) {
		if (fPos >= fSrc.size() || fSrc[fPos] != c) fail("expected character");
		++fPos;
	}

	Value parse_object() {
		++fDepth;
		expect('{');
		Object o;
		skip_ws();
		if (fPos < fSrc.size() && fSrc[fPos] == '}') { ++fPos; --fDepth; return Value(std::move(o)); }
		for (;;) {
			skip_ws();
			std::string key = parse_string();
			skip_ws();
			expect(':');
			Value v = parse_value();
			o.emplace(std::move(key), std::move(v));
			skip_ws();
			if (fPos >= fSrc.size()) fail("unterminated object");
			if (fSrc[fPos] == ',') { ++fPos; continue; }
			if (fSrc[fPos] == '}') { ++fPos; --fDepth; return Value(std::move(o)); }
			fail("expected , or }");
		}
	}

	Value parse_array() {
		++fDepth;
		expect('[');
		Array a;
		skip_ws();
		if (fPos < fSrc.size() && fSrc[fPos] == ']') { ++fPos; --fDepth; return Value(std::move(a)); }
		for (;;) {
			Value v = parse_value();
			a.push_back(std::move(v));
			skip_ws();
			if (fPos >= fSrc.size()) fail("unterminated array");
			if (fSrc[fPos] == ',') { ++fPos; continue; }
			if (fSrc[fPos] == ']') { ++fPos; --fDepth; return Value(std::move(a)); }
			fail("expected , or ]");
		}
	}

	std::string parse_string() {
		expect('"');
		std::string out;
		while (fPos < fSrc.size()) {
			char c = fSrc[fPos++];
			if (c == '"') return out;
			if (c != '\\') { out += c; continue; }
			if (fPos >= fSrc.size()) fail("unterminated escape");
			char esc = fSrc[fPos++];
			switch (esc) {
				case '"':  out += '"';  break;
				case '\\': out += '\\'; break;
				case '/':  out += '/';  break;
				case 'b':  out += '\b'; break;
				case 'f':  out += '\f'; break;
				case 'n':  out += '\n'; break;
				case 'r':  out += '\r'; break;
				case 't':  out += '\t'; break;
				case 'u': {
					auto read_hex4 = [&]() -> unsigned {
						if (fPos + 4 > fSrc.size()) fail("truncated \\u");
						unsigned code = 0;
						for (int i = 0; i < 4; ++i) {
							char h = fSrc[fPos++];
							code <<= 4;
							if (h >= '0' && h <= '9')      code |= (h - '0');
							else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
							else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
							else fail("bad \\u hex");
						}
						return code;
					};
					unsigned code = read_hex4();
					// Handle UTF-16 surrogate pairs (U+10000..U+10FFFF).
					if (code >= 0xD800 && code <= 0xDBFF) {
						if (fPos + 2 <= fSrc.size()
						    && fSrc[fPos] == '\\' && fSrc[fPos + 1] == 'u') {
							fPos += 2;
							unsigned low = read_hex4();
							if (low >= 0xDC00 && low <= 0xDFFF) {
								code = 0x10000
								     + ((code - 0xD800) << 10)
								     + (low - 0xDC00);
							} else {
								fail("bad surrogate pair");
							}
						} else {
							fail("lone high surrogate");
						}
					}
					// Encode as UTF-8.
					if (code < 0x80) {
						out += static_cast<char>(code);
					} else if (code < 0x800) {
						out += static_cast<char>(0xC0 | (code >> 6));
						out += static_cast<char>(0x80 | (code & 0x3F));
					} else if (code < 0x10000) {
						out += static_cast<char>(0xE0 | (code >> 12));
						out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
						out += static_cast<char>(0x80 | (code & 0x3F));
					} else {
						out += static_cast<char>(0xF0 | (code >> 18));
						out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
						out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
						out += static_cast<char>(0x80 | (code & 0x3F));
					}
					break;
				}
				default: fail("bad escape");
			}
		}
		fail("unterminated string");
	}

	Value parse_number() {
		size_t start = fPos;
		if (fSrc[fPos] == '-') ++fPos;
		// JSON requires at least one digit after an optional minus sign.
		if (fPos >= fSrc.size() || fSrc[fPos] < '0' || fSrc[fPos] > '9')
			fail("invalid number: expected digit after '-'");
		while (fPos < fSrc.size() && fSrc[fPos] >= '0' && fSrc[fPos] <= '9') ++fPos;
		if (fPos < fSrc.size() && fSrc[fPos] == '.') {
			++fPos;
			while (fPos < fSrc.size() && fSrc[fPos] >= '0' && fSrc[fPos] <= '9') ++fPos;
		}
		if (fPos < fSrc.size() && (fSrc[fPos] == 'e' || fSrc[fPos] == 'E')) {
			++fPos;
			if (fPos < fSrc.size() && (fSrc[fPos] == '+' || fSrc[fPos] == '-')) ++fPos;
			while (fPos < fSrc.size() && fSrc[fPos] >= '0' && fSrc[fPos] <= '9') ++fPos;
		}
		return Value(std::strtod(fSrc.substr(start, fPos - start).c_str(), nullptr));
	}

	Value parse_bool() {
		if (fSrc.compare(fPos, 4, "true") == 0) { fPos += 4; return Value(true); }
		if (fSrc.compare(fPos, 5, "false") == 0) { fPos += 5; return Value(false); }
		fail("expected true/false");
	}

	Value parse_null() {
		if (fSrc.compare(fPos, 4, "null") == 0) { fPos += 4; return Value(nullptr); }
		fail("expected null");
	}
};

}  // namespace

std::string Value::serialize() const {
	std::string out;
	serialize_value(out, *this);
	return out;
}

Value Value::parse(const std::string& s) {
	Parser p(s);
	Value v = p.parse_value();
	p.expect_eof();
	return v;
}

}  // namespace pluto::json
