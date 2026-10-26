#include "VectorStore.h"
#include "Document.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#ifdef __HAIKU__
#include <Query.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <Entry.h>
#include <Path.h>
#include <Node.h>
#include <fs_attr.h>
#include <String.h>
#endif

namespace pluto {
namespace langchain {

// ---------- VectorStore ----------

double VectorStore::keyword_overlap(const std::string& a, const std::string& b) {
	std::string la = a;
	std::string lb = b;
	std::transform(la.begin(), la.end(), la.begin(),
		[](unsigned char c) { return std::tolower(c); });
	std::transform(lb.begin(), lb.end(), lb.begin(),
		[](unsigned char c) { return std::tolower(c); });

	// Tokenize into words.
	std::vector<std::string> ta, tb;
	std::string word;
	std::istringstream sa(la);
	while (sa >> word) {
		std::string clean;
		for (char c : word) {
			if (std::isalnum(static_cast<unsigned char>(c))) clean += c;
		}
		if (!clean.empty()) ta.push_back(clean);
	}
	std::istringstream sb(lb);
	while (sb >> word) {
		std::string clean;
		for (char c : word) {
			if (std::isalnum(static_cast<unsigned char>(c))) clean += c;
		}
		if (!clean.empty()) tb.push_back(clean);
	}

	if (ta.empty() || tb.empty()) return 0.0;

	int intersection = 0;
	for (const auto& w : ta) {
		for (const auto& v : tb) {
			if (w == v) { intersection++; break; }
		}
	}

	return static_cast<double>(intersection) /
	       static_cast<double>(std::min(ta.size(), tb.size()));
}

void VectorStore::add_texts(
	const std::vector<std::string>& texts,
	const std::vector<std::map<std::string, std::string>>& metadatas) {
	for (size_t i = 0; i < texts.size(); i++) {
		std::map<std::string, std::string> meta;
		if (i < metadatas.size()) meta = metadatas[i];
		fDocuments.emplace_back(texts[i], meta);
	}
}

std::vector<Document> VectorStore::similarity_search(const std::string& query,
                                                      int k) {
	// Score each document by keyword overlap with the query.
	struct ScoredDoc {
		size_t index;
		double score;
	};
	std::vector<ScoredDoc> scored;
	for (size_t i = 0; i < fDocuments.size(); i++) {
		double score = keyword_overlap(query, fDocuments[i].page_content);
		if (score > 0.0) {
			scored.push_back({i, score});
		}
	}

	// Sort by score descending.
	std::sort(scored.begin(), scored.end(),
		[](const ScoredDoc& a, const ScoredDoc& b) {
			return a.score > b.score;
		});

	// Return top k.
	std::vector<Document> result;
	size_t limit = (k > 0 && (size_t)k < scored.size())
		? (size_t)k : scored.size();
	for (size_t i = 0; i < limit; i++) {
		result.push_back(fDocuments[scored[i].index]);
	}
	return result;
}

std::unique_ptr<BaseRetriever> VectorStore::as_retriever() {
	return std::make_unique<VectorStoreRetriever>(fCollectionName);
}

// ---------- VectorStoreRetriever ----------

void VectorStoreRetriever::add_document(const Document& doc) {
	fDocuments.push_back(doc);
}

void VectorStoreRetriever::add_documents(const std::vector<Document>& docs) {
	fDocuments.insert(fDocuments.end(), docs.begin(), docs.end());
}

std::vector<Document> VectorStoreRetriever::get_relevant_documents(
	const std::string& query) {
	// Simple keyword-based retrieval.
	struct ScoredDoc {
		size_t index;
		double score;
	};
	std::vector<ScoredDoc> scored;
	for (size_t i = 0; i < fDocuments.size(); i++) {
		// Score by content overlap.
		std::string la = query;
		std::string lb = fDocuments[i].page_content;
		std::transform(la.begin(), la.end(), la.begin(),
			[](unsigned char c) { return std::tolower(c); });
		std::transform(lb.begin(), lb.end(), lb.begin(),
			[](unsigned char c) { return std::tolower(c); });

		// Count keyword matches.
		double score = 0.0;
		std::istringstream sa(la);
		std::string word;
		while (sa >> word) {
			if (lb.find(word) != std::string::npos) score += 1.0;
		}
		score /= std::max(1.0, (double)la.size() / 10.0);

		if (score > 0.0) {
			scored.push_back({i, score});
		}
	}

	std::sort(scored.begin(), scored.end(),
		[](const ScoredDoc& a, const ScoredDoc& b) {
			return a.score > b.score;
		});

	std::vector<Document> result;
	size_t limit = std::min<size_t>(10, scored.size());
	for (size_t i = 0; i < limit; i++) {
		result.push_back(fDocuments[scored[i].index]);
	}
	return result;
}

std::vector<Document> VectorStoreRetriever::search_by_attribute(
	const std::string& key, const std::string& value) {
	std::vector<Document> result;
	for (const auto& doc : fDocuments) {
		auto it = doc.metadata.find(key);
		if (it != doc.metadata.end() && it->second == value) {
			result.push_back(doc);
		}
	}
	return result;
}

}  // namespace langchain
}  // namespace pluto
