#include "TextSplitter.h"

#include <algorithm>
#include <cctype>

namespace pluto {
namespace langchain {

std::vector<std::string> CharacterTextSplitter::split_text(
	const std::string& text) {
	std::vector<std::string> chunks;
	if (text.empty()) return chunks;

	size_t start = 0;
	while (start < text.size()) {
		size_t end = start + fChunkSize;
		if (end > text.size()) end = text.size();

		// Try to break at a separator near the end.
		if (end < text.size()) {
			size_t sep_pos = text.rfind(fSeparator, end);
			if (sep_pos != std::string::npos && sep_pos >= start) {
				end = sep_pos + 1;
			}
		}

		chunks.push_back(text.substr(start, end - start));

		if (end >= text.size()) break;

		// Move start forward, accounting for overlap.
		start = end;
		if (start < text.size() && fChunkOverlap > 0) {
			size_t overlap_start = start > fChunkOverlap
				? start - fChunkOverlap : 0;
			start = overlap_start;
		}
	}

	return chunks;
}

std::vector<std::string> RecursiveCharacterTextSplitter::split_text(
	const std::string& text) {
	std::vector<std::string> chunks;
	if (text.empty()) return chunks;

	// If text is small enough, return as-is.
	if (text.size() <= fChunkSize) {
		chunks.push_back(text);
		return chunks;
	}

	// Try each separator in order.
	for (const auto& sep : fSeparators) {
		if (sep.empty()) {
			// Last resort: split by character.
			for (size_t i = 0; i < text.size(); i += fChunkSize) {
				size_t end = std::min(i + fChunkSize, text.size());
				chunks.push_back(text.substr(i, end - i));
				if (end >= text.size()) break;
			}
			break;
		}

		// Split by this separator.
		std::vector<std::string> parts;
		size_t start = 0;
		while (start < text.size()) {
			size_t sep_pos = text.find(sep, start);
			if (sep_pos == std::string::npos) {
				parts.push_back(text.substr(start));
				break;
			}
			parts.push_back(text.substr(start, sep_pos - start + sep.size()));
			start = sep_pos + sep.size();
		}

		// Recursively split parts that are too large.
		std::vector<std::string> result;
		for (auto& part : parts) {
			if (part.size() <= fChunkSize) {
				result.push_back(std::move(part));
			} else {
				// Create a sub-splitter without this separator to avoid
				// infinite recursion.
				std::vector<std::string> remaining_seps;
				for (const auto& s : fSeparators) {
					if (s != sep) remaining_seps.push_back(s);
				}
				if (remaining_seps.empty()) {
					// Character-level split.
					for (size_t i = 0; i < part.size(); i += fChunkSize) {
						size_t end = std::min(i + fChunkSize, part.size());
						result.push_back(part.substr(i, end - i));
						if (end >= part.size()) break;
					}
				} else {
					RecursiveCharacterTextSplitter sub_splitter(
						remaining_seps, fChunkSize, fChunkOverlap);
					auto sub_chunks = sub_splitter.split_text(part);
					result.insert(result.end(),
					              std::make_move_iterator(sub_chunks.begin()),
					              std::make_move_iterator(sub_chunks.end()));
				}
			}
		}

		if (!result.empty()) return result;
	}

	return chunks;
}

}  // namespace langchain
}  // namespace pluto
