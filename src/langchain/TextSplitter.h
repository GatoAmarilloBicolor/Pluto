#pragma once

#include <string>
#include <vector>

namespace pluto {
namespace langchain {

// Splits text into smaller chunks for processing.
// Used to break large documents into manageable pieces for embedding
// and retrieval.
class TextSplitter {
public:
	virtual ~TextSplitter() = default;

	// Split text into chunks.
	// Returns a vector of text chunks.
	virtual std::vector<std::string> split_text(
		const std::string& text) = 0;

	// Split multiple documents, preserving metadata.
	virtual std::vector<std::string> split_documents(
		const std::vector<std::string>& texts) {
		std::vector<std::string> result;
		for (const auto& text : texts) {
			auto chunks = split_text(text);
			result.insert(result.end(),
			              std::make_move_iterator(chunks.begin()),
			              std::make_move_iterator(chunks.end()));
		}
		return result;
	}
};

// Splits text by a separator character (e.g., "\n\n" for paragraphs).
class CharacterTextSplitter : public TextSplitter {
public:
	CharacterTextSplitter(char separator = '\n',
	                      size_t chunk_size = 1000,
	                      size_t chunk_overlap = 200)
		: fSeparator(separator),
		  fChunkSize(chunk_size),
		  fChunkOverlap(chunk_overlap) {}

	std::vector<std::string> split_text(
		const std::string& text) override;

private:
	char fSeparator;
	size_t fChunkSize;
	size_t fChunkOverlap;
};

// Splits text recursively using a list of separators.
// Tries to split by larger separators first, then falls back to smaller ones.
class RecursiveCharacterTextSplitter : public TextSplitter {
public:
	RecursiveCharacterTextSplitter(
		std::vector<std::string> separators = {"\n\n", "\n", " ", ""},
		size_t chunk_size = 1000,
		size_t chunk_overlap = 200)
		: fSeparators(std::move(separators)),
		  fChunkSize(chunk_size),
		  fChunkOverlap(chunk_overlap) {}

	std::vector<std::string> split_text(
		const std::string& text) override;

private:
	std::vector<std::string> fSeparators;
	size_t fChunkSize;
	size_t fChunkOverlap;
};

}  // namespace langchain
}  // namespace pluto
