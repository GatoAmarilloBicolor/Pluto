#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace pluto {

inline std::string base64_encode(const void* data, size_t len) {
	static const char kTable[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const auto* src = static_cast<const uint8_t*>(data);
	std::string out;
	out.reserve(((len + 2) / 3) * 4);
	size_t i = 0;
	while (i + 2 < len) {
		uint32_t v = (uint32_t(src[i]) << 16)
		           | (uint32_t(src[i+1]) << 8)
		           |  uint32_t(src[i+2]);
		out += kTable[(v >> 18) & 0x3F];
		out += kTable[(v >> 12) & 0x3F];
		out += kTable[(v >>  6) & 0x3F];
		out += kTable[ v        & 0x3F];
		i += 3;
	}
	if (i < len) {
		uint32_t v = uint32_t(src[i]) << 16;
		if (i + 1 < len) v |= uint32_t(src[i+1]) << 8;
		out += kTable[(v >> 18) & 0x3F];
		out += kTable[(v >> 12) & 0x3F];
		out += (i + 1 < len) ? kTable[(v >> 6) & 0x3F] : '=';
		out += '=';
	}
	return out;
}

inline std::string base64_encode(const std::string& s) {
	return base64_encode(s.data(), s.size());
}

inline std::string base64_decode(const std::string& encoded) {
	static const uint8_t kDecodeTable[256] = {
		// 0-42: invalid
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,
		62, // '+'
		64,64,64,
		63, // '/'
		52,53,54,55,56,57,58,59,60,61, // '0'-'9'
		64,64,64,
		0,  // '=' treated as 0
		64,64,64,
		0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25, // A-Z
		64,64,64,64,64,64,
		26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51, // a-z
		64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
	};
	std::string out;
	out.reserve(encoded.size() * 3 / 4);
	size_t i = 0;
	while (i < encoded.size()) {
		// Skip whitespace/newlines.
		if (encoded[i] == '\n' || encoded[i] == '\r' || encoded[i] == ' ') {
			++i; continue;
		}
		if (i + 3 >= encoded.size()) break;
		uint8_t a = kDecodeTable[(uint8_t)encoded[i]];
		uint8_t b = kDecodeTable[(uint8_t)encoded[i+1]];
		uint8_t c = kDecodeTable[(uint8_t)encoded[i+2]];
		uint8_t d = kDecodeTable[(uint8_t)encoded[i+3]];
		if (a > 63 || b > 63) { i += 4; continue; }
		uint32_t v = (uint32_t(a) << 18) | (uint32_t(b) << 12)
		           | (uint32_t(c) << 6) | uint32_t(d);
		out += char((v >> 16) & 0xFF);
		if (encoded[i+2] != '=') out += char((v >> 8) & 0xFF);
		if (encoded[i+3] != '=') out += char(v & 0xFF);
		i += 4;
	}
	return out;
}

}  // namespace pluto
