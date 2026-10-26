#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace pluto {
namespace langchain {

// Base class for retrievers.
// A retriever finds relevant documents given a query.
class BaseRetriever {
public:
	virtual ~BaseRetriever() = default;

	// Get relevant documents for a query.
	virtual std::vector<Document> get_relevant_documents(
		const std::string& query) = 0;

	// Get relevant documents with an optional limit.
	virtual std::vector<Document> get_relevant_documents(
		const std::string& query, int limit) {
		auto docs = get_relevant_documents(query);
		if (limit > 0 && (int)docs.size() > limit) {
			docs.resize(limit);
		}
		return docs;
	}
};

// Vector store retriever using BFS attributes for semantic search.
// Uses Haiku's BFS query system to find documents by attribute values.
class VectorStoreRetriever : public BaseRetriever {
public:
	VectorStoreRetriever(std::string collection_name = "langchain_docs")
		: fCollectionName(std::move(collection_name)) {}

	// Add a document to the vector store.
	// Stores the document content and metadata as BFS attributes.
	void add_document(const Document& doc);

	// Add multiple documents.
	void add_documents(const std::vector<Document>& docs);

	// Get relevant documents using keyword matching on attributes.
	std::vector<Document> get_relevant_documents(
		const std::string& query) override;

	// Search documents by a specific attribute key-value pair.
	std::vector<Document> search_by_attribute(
		const std::string& key, const std::string& value);

private:
	std::string fCollectionName;
	std::vector<Document> fDocuments;  // In-memory store for simplicity
};

// Simple in-memory vector store using keyword similarity.
// For production, this would use embeddings and BFS queries.
class VectorStore {
public:
	VectorStore(std::string collection_name = "langchain_docs")
		: fCollectionName(std::move(collection_name)) {}

	// Add texts to the store.
	void add_texts(const std::vector<std::string>& texts,
	               const std::vector<std::map<std::string, std::string>>& metadatas = {});

	// Search for similar documents using keyword overlap.
	std::vector<Document> similarity_search(const std::string& query,
	                                         int k = 4);

	// Get a retriever for this store.
	std::unique_ptr<BaseRetriever> as_retriever();

private:
	std::string fCollectionName;
	std::vector<Document> fDocuments;

	double keyword_overlap(const std::string& a, const std::string& b);
};

}  // namespace langchain
}  // namespace pluto
