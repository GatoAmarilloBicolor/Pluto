#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <ctime>

namespace pluto {
namespace langchain {

// A document with content and associated metadata.
// Used throughout the LangChain pipeline for document processing.
struct Document {
	std::string page_content;   // The actual text content
	std::map<std::string, std::string> metadata;  // key-value metadata
	std::time_t created_at;     // Creation timestamp

	Document() : created_at(std::time(nullptr)) {}
	Document(std::string content,
	         std::map<std::string, std::string> meta = {})
		: page_content(std::move(content)),
		  metadata(std::move(meta)),
		  created_at(std::time(nullptr)) {}
};

// Base class for document loaders.
// Loads documents from various sources (files, URLs, etc.).
class DocumentLoader {
public:
	virtual ~DocumentLoader() = default;

	// Load documents from the source.
	// Returns a vector of Document objects.
	virtual std::vector<Document> load() = 0;

	// Load and split documents using a text splitter.
	// Default implementation loads then splits each document.
	virtual std::vector<Document> load_and_split(
		class TextSplitter& splitter);
};

// Load a single text file as a Document.
class TextFileLoader : public DocumentLoader {
public:
	explicit TextFileLoader(std::string file_path)
		: fFilePath(std::move(file_path)) {}

	std::vector<Document> load() override;

private:
	std::string fFilePath;
};

// Load all .md files from a directory.
class DirectoryLoader : public DocumentLoader {
public:
	DirectoryLoader(std::string dir_path,
	                std::string glob_pattern = "*.md")
		: fDirPath(std::move(dir_path)),
		  fGlobPattern(std::move(glob_pattern)) {}

	std::vector<Document> load() override;

private:
	std::string fDirPath;
	std::string fGlobPattern;
};

}  // namespace langchain
}  // namespace pluto
