// Text indexing utilities — shared between RAG and Wiki modules.
// Provides tokenization, IDF computation, and scoring primitives.

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pluto {


// Lowercase a string (ASCII-safe).
std::string ToLower(const std::string& s);


// Tokenize text into lowercase words. Splits on non-alphanumeric
// characters; underscores are kept as part of tokens.
std::vector<std::string> Tokenize(const std::string& text);


// IDF (Inverse Document Frequency) table.
// Maps each token to its IDF weight: log((N+1) / df).
// The +1 avoids zero-weight when N==df (single-document case).
using IdfTable = std::unordered_map<std::string, double>;


// Build an IDF table from a collection of token sets.
// Each entry in `doc_tokens` represents the unique tokens of one document.
// Returns the IDF map.
IdfTable BuildIdf(const std::vector<std::unordered_set<std::string>>& doc_tokens);


// Score a set of fields against query tokens using an IDF table.
// Each field has a weight multiplier (e.g., topic=4.0, body=1.0).
// Returns the total score (0.0 if no match).
struct ScoredField {
	std::string text;       // the text to tokenize and match against
	double weight;          // multiplier for IDF score (e.g., 4.0 for topic)
};

double ScoreDocument(const std::vector<ScoredField>& fields,
                     const std::vector<std::string>& query_tokens,
                     const IdfTable& idf);


// Convenience: score a single pre-tokenized field set against query.
// Matches query_tokens against `field_tokens` using IDF weights.
double ScoreTokenSet(const std::unordered_set<std::string>& field_tokens,
                     const std::vector<std::string>& query_tokens,
                     const IdfTable& idf,
                     double weight = 1.0);


}  // namespace pluto
