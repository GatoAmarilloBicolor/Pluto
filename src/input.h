#pragma once

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "base64.h"
#include "provider/Provider.h"
#include "workspace.h"

#include <sys/stat.h>

namespace pluto {

// Detect image files by extension and return the MIME type.
// Returns empty string if not an image.
inline std::string image_media_type(const std::string& path) {
	auto ends_with = [](const std::string& s, const std::string& sfx) {
		if (sfx.size() > s.size()) return false;
		return s.compare(s.size() - sfx.size(), sfx.size(), sfx) == 0;
	};
	std::string lower = path;
	for (auto& c : lower)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (ends_with(lower, ".png"))  return "image/png";
	if (ends_with(lower, ".jpg"))  return "image/jpeg";
	if (ends_with(lower, ".jpeg")) return "image/jpeg";
	if (ends_with(lower, ".gif"))  return "image/gif";
	if (ends_with(lower, ".webp")) return "image/webp";
	return {};
}

// Detect audio files by extension. Returns "mp3" or "wav", or empty.
inline std::string audio_format(const std::string& path) {
	auto ends_with = [](const std::string& s, const std::string& sfx) {
		if (sfx.size() > s.size()) return false;
		return s.compare(s.size() - sfx.size(), sfx.size(), sfx) == 0;
	};
	std::string lower = path;
	for (auto& c : lower)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (ends_with(lower, ".mp3"))  return "mp3";
	if (ends_with(lower, ".wav"))  return "wav";
	return {};
}

// Detect PDF files by extension.
inline bool is_pdf(const std::string& path) {
	if (path.size() < 4) return false;
	std::string ext = path.substr(path.size() - 4);
	for (auto& c : ext)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return ext == ".pdf";
}

// Read a binary file into a string. Returns false on error or if > 20 MB.
inline bool read_binary_file(const std::string& path, std::string& out) {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) return false;
	if (st.st_size > 20 * 1024 * 1024) return false;
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	out.resize(static_cast<size_t>(st.st_size));
	f.read(out.data(), st.st_size);
	return f.good();
}

// Build content blocks from user input text. Text goes into TEXT blocks,
// @image.png mentions become IMAGE blocks with base64 data, @file.txt
// mentions are expanded inline. Returns true if any image blocks were created.
inline bool build_user_content(const std::string& input,
                               std::vector<ContentBlock>& blocks) {
	bool has_images = false;
	std::string text_accum;

	auto flush_text = [&]() {
		if (text_accum.empty()) return;
		ContentBlock tb;
		tb.type = ContentBlock::TEXT;
		tb.text = std::move(text_accum);
		blocks.push_back(std::move(tb));
		text_accum.clear();
	};

	size_t i = 0;
	while (i < input.size()) {
		bool boundary = (i == 0) ||
			std::isspace(static_cast<unsigned char>(input[i - 1]));
		if (boundary && input[i] == '@') {
			size_t end = i + 1;
			while (end < input.size()
			    && !std::isspace(static_cast<unsigned char>(input[end])))
				++end;
			if (end > i + 1) {
				std::string path = input.substr(i + 1, end - i - 1);
				// Security: block @file reads outside workspace.
				if (!WorkspaceAllows(path)) {
					text_accum += "[error: ";
					text_accum += path;
					text_accum += " is outside the workspace]";
					i = end;
					continue;
				}
				// Image files → IMAGE block (vision).
				std::string mime = image_media_type(path);
				if (!mime.empty()) {
					std::string raw;
					if (read_binary_file(path, raw)) {
						flush_text();
						ContentBlock ib;
						ib.type = ContentBlock::IMAGE;
						ib.image_media_type = mime;
						ib.image_data = base64_encode(raw);
						blocks.push_back(std::move(ib));
						has_images = true;
						i = end;
						continue;
					}
				}
				// Audio files → AUDIO block (OpenAI gpt-4o-audio).
				std::string afmt = audio_format(path);
				if (!afmt.empty()) {
					std::string raw;
					if (read_binary_file(path, raw)) {
						flush_text();
						ContentBlock ab;
						ab.type = ContentBlock::AUDIO;
						ab.audio_format = afmt;
						ab.audio_data = base64_encode(raw);
						blocks.push_back(std::move(ab));
						has_images = true;  // reuse flag to indicate non-text content
						i = end;
						continue;
					}
				}
				// PDF files → DOCUMENT block (Claude vision / OpenAI).
				if (is_pdf(path)) {
					std::string raw;
					if (read_binary_file(path, raw)) {
						flush_text();
						ContentBlock db;
						db.type = ContentBlock::DOCUMENT;
						db.doc_media_type = "application/pdf";
						db.doc_data = base64_encode(raw);
						blocks.push_back(std::move(db));
						has_images = true;
						i = end;
						continue;
					}
				}
				// Not a media file — try as text file mention
				std::ifstream f(path);
				if (f) {
					std::ostringstream os;
					os << f.rdbuf();
					text_accum += "\n[file: ";
					text_accum += path;
					text_accum += "]\n";
					text_accum += os.str();
					text_accum += "\n[/file]\n";
					i = end;
					continue;
				}
			}
		}
		text_accum += input[i++];
	}
	flush_text();
	return has_images;
}

}  // namespace pluto
