// Text indexing utilities — tokenization, IDF, scoring.

#include "text_index.h"

#include <cctype>
#include <cmath>

namespace pluto {


std::string
ToLower(const std::string& s)
{
	std::string r = s;
	for (auto& c : r) c = std::tolower(static_cast<unsigned char>(c));
	return r;
}

std::vector<std::string>
Tokenize(const std::string& text)
{
	std::vector<std::string> tokens;
	std::string tok;
	for (char c : text) {
		if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
			tok += std::tolower(static_cast<unsigned char>(c));
		}
		else {
			if (!tok.empty()) {
				tokens.push_back(tok);
				tok.clear();
			}
		}
	}
	if (!tok.empty()) tokens.push_back(tok);
	return tokens;
}

IdfTable
BuildIdf(const std::vector<std::unordered_set<std::string>>& doc_tokens)
{
	IdfTable idf;
	int N = (int)doc_tokens.size();
	if (N == 0) return idf;

	// Document frequency: how many documents contain each token.
	std::unordered_map<std::string, int> df;
	for (const auto& doc : doc_tokens) {
		for (const auto& t : doc) df[t]++;
	}

	// IDF = log((N+1) / df). The +1 avoids zero when N == df.
	for (const auto& [token, count] : df)
		idf[token] = std::log((double)(N + 1) / (double)count);

	return idf;
}

double
ScoreTokenSet(const std::unordered_set<std::string>& field_tokens,
              const std::vector<std::string>& query_tokens, const IdfTable& idf, double weight)
{
	double score = 0.0;
	for (const auto& qt : query_tokens) {
		if (field_tokens.count(qt)) {
			double w = 1.0;
			auto it = idf.find(qt);
			if (it != idf.end()) w = it->second;
			score += weight * w;
		}
	}
	return score;
}

double
ScoreDocument(const std::vector<ScoredField>& fields,
              const std::vector<std::string>& query_tokens, const IdfTable& idf)
{
	double total = 0.0;
	for (const auto& field : fields) {
		auto toks = Tokenize(field.text);
		std::unordered_set<std::string> tset(toks.begin(), toks.end());
		total += ScoreTokenSet(tset, query_tokens, idf, field.weight);
	}
	return total;
}


} // namespace pluto
