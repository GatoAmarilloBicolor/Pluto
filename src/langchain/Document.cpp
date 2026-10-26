#include "Document.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>

namespace pluto {
namespace langchain {

std::vector<Document> DocumentLoader::load_and_split(
	TextSplitter& splitter) {
	auto docs = load();
	std::vector<Document> result;
	for (auto& doc : docs) {
		auto chunks = splitter.split_text(doc.page_content);
		for (auto& chunk : chunks) {
			result.emplace_back(std::move(chunk), doc.metadata);
		}
	}
	return result;
}

std::vector<Document> TextFileLoader::load() {
	std::vector<Document> docs;
	std::ifstream f(fFilePath);
	if (!f) return docs;

	std::ostringstream ss;
	ss << f.rdbuf();
	std::string content = ss.str();

	std::map<std::string, std::string> metadata;
	metadata["source"] = fFilePath;

	struct stat st;
	if (stat(fFilePath.c_str(), &st) == 0) {
		metadata["size"] = std::to_string(st.st_size);
		metadata["modified"] = std::to_string(st.st_mtime);
	}

	docs.emplace_back(std::move(content), std::move(metadata));
	return docs;
}

std::vector<Document> DirectoryLoader::load() {
	std::vector<Document> docs;
	DIR* dir = opendir(fDirPath.c_str());
	if (!dir) return docs;

	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		std::string name = entry->d_name;
		if (name == "." || name == "..") continue;

		// Simple glob match: check if name ends with the pattern
		if (!fGlobPattern.empty() && fGlobPattern[0] == '*') {
			std::string ext = fGlobPattern.substr(1);
			if (name.size() < ext.size() ||
			    name.compare(name.size() - ext.size(), ext.size(), ext) != 0) {
				continue;
			}
		}

		std::string path = fDirPath + "/" + name;
		TextFileLoader loader(path);
		auto file_docs = loader.load();
		docs.insert(docs.end(),
		            std::make_move_iterator(file_docs.begin()),
		            std::make_move_iterator(file_docs.end()));
	}
	closedir(dir);
	return docs;
}

}  // namespace langchain
}  // namespace pluto
