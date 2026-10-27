// Tool: web_search — search the web and return results.
// Supports multiple backends: Brave Search, Tavily, SearXNG.

#include "WebSearch.h"
#include "Tool.h"
#include "../json.h"

#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace pluto {

namespace {

constexpr int kMaxResults = 10;

size_t collect_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
	static_cast<std::string*>(ud)->append(ptr, size * nmemb);
	return size * nmemb;
}

std::string http_get(const std::string& url,
                     const std::vector<std::string>& headers) {
	CURL* c = curl_easy_init();
	if (!c) return {};
	std::string resp;
	struct curl_slist* hdrs = nullptr;
	for (const auto& h : headers)
		hdrs = curl_slist_append(hdrs, h.c_str());
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, collect_cb);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(c, CURLOPT_USERAGENT, "pluto/0.5.0 (Haiku)");
	curl_easy_setopt(c, CURLOPT_CAINFO,
		"/system/data/ssl/CARootCertificates.pem");
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
	CURLcode rc = curl_easy_perform(c);
	curl_slist_free_all(hdrs);
	curl_easy_cleanup(c);
	if (rc != CURLE_OK) return {};
	return resp;
}

std::string http_post(const std::string& url, const std::string& body,
                      const std::vector<std::string>& headers) {
	CURL* c = curl_easy_init();
	if (!c) return {};
	std::string resp;
	struct curl_slist* hdrs = nullptr;
	for (const auto& h : headers)
		hdrs = curl_slist_append(hdrs, h.c_str());
	hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, collect_cb);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(c, CURLOPT_USERAGENT, "pluto/0.5.0 (Haiku)");
	curl_easy_setopt(c, CURLOPT_CAINFO,
		"/system/data/ssl/CARootCertificates.pem");
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
	CURLcode rc = curl_easy_perform(c);
	curl_slist_free_all(hdrs);
	curl_easy_cleanup(c);
	if (rc != CURLE_OK) return {};
	return resp;
}

std::string url_encode(const std::string& s) {
	CURL* c = curl_easy_init();
	if (!c) return s;
	char* enc = curl_easy_escape(c, s.c_str(), (int)s.size());
	std::string out(enc);
	curl_free(enc);
	curl_easy_cleanup(c);
	return out;
}

// Format results as readable text.
struct SearchResult {
	std::string title;
	std::string url;
	std::string snippet;
};

std::string format_results(const std::vector<SearchResult>& results,
                           const std::string& query) {
	if (results.empty())
		return "No results found for: " + query;
	std::string out = "Search results for: " + query + "\n\n";
	for (size_t i = 0; i < results.size(); ++i) {
		out += std::to_string(i + 1) + ". " + results[i].title + "\n";
		out += "   " + results[i].url + "\n";
		if (!results[i].snippet.empty())
			out += "   " + results[i].snippet + "\n";
		out += "\n";
	}
	return out;
}

// --- Brave Search API ---
std::string search_brave(const std::string& query, const std::string& api_key) {
	std::string url = "https://api.search.brave.com/res/v1/web/search?q="
		+ url_encode(query) + "&count=" + std::to_string(kMaxResults);
	std::string resp = http_get(url, {
		"Accept: application/json",
		"Accept-Encoding: gzip",
		"X-Subscription-Token: " + api_key
	});
	if (resp.empty()) return "Brave Search API error (no response)";

	try {
		auto v = json::Value::parse(resp);
		const json::Value* web = v.find("web");
		if (!web) return "No web results in response";
		const json::Value* results_arr = web->find("results");
		if (!results_arr || !results_arr->is_arr()) return "No results array";

		std::vector<SearchResult> results;
		for (size_t i = 0; i < results_arr->as_arr().size() && i < kMaxResults; ++i) {
			const auto& item = results_arr->as_arr()[i];
			SearchResult sr;
			sr.title = item.str("title");
			sr.url = item.str("url");
			sr.snippet = item.str("description");
			results.push_back(std::move(sr));
		}
		return format_results(results, query);
	} catch (const std::exception&) {
		return "Failed to parse Brave Search response";
	}
}

// --- Tavily Search API ---
std::string search_tavily(const std::string& query, const std::string& api_key) {
	json::Value req;
	req.set("api_key", api_key);
	req.set("query", query);
	req.set("max_results", (double)kMaxResults);

	std::string resp = http_post(
		"https://api.tavily.com/search",
		req.serialize(), {});
	if (resp.empty()) return "Tavily API error (no response)";

	try {
		auto v = json::Value::parse(resp);
		const json::Value* results_arr = v.find("results");
		if (!results_arr || !results_arr->is_arr()) return "No results";

		std::vector<SearchResult> results;
		for (size_t i = 0; i < results_arr->as_arr().size() && i < kMaxResults; ++i) {
			const auto& item = results_arr->as_arr()[i];
			SearchResult sr;
			sr.title = item.str("title");
			sr.url = item.str("url");
			sr.snippet = item.str("content");
			results.push_back(std::move(sr));
		}
		return format_results(results, query);
	} catch (const std::exception&) {
		return "Failed to parse Tavily response";
	}
}

// --- SearXNG (self-hosted) ---
std::string search_searxng(const std::string& query,
                           const std::string& base_url) {
	std::string url = base_url + "/search?q=" + url_encode(query)
		+ "&format=json&categories=general";
	std::string resp = http_get(url, {"Accept: application/json"});
	if (resp.empty()) return "SearXNG error (no response from " + base_url + ")";

	try {
		auto v = json::Value::parse(resp);
		const json::Value* results_arr = v.find("results");
		if (!results_arr || !results_arr->is_arr()) return "No results";

		std::vector<SearchResult> results;
		for (size_t i = 0; i < results_arr->as_arr().size() && i < kMaxResults; ++i) {
			const auto& item = results_arr->as_arr()[i];
			SearchResult sr;
			sr.title = item.str("title");
			sr.url = item.str("url");
			sr.snippet = item.str("content");
			results.push_back(std::move(sr));
		}
		return format_results(results, query);
	} catch (const std::exception&) {
		return "Failed to parse SearXNG response";
	}
}

class WebSearchTool : public Tool {
public:
	std::string name() const override { return "web_search"; }
	std::string description() const override {
		return "Search the web for a query and return a list of results with "
		       "titles, URLs, and snippets. Use web_fetch to read full pages "
		       "from the returned URLs. Requires BRAVE_API_KEY, TAVILY_API_KEY, "
		       "or SEARXNG_URL environment variable.";
	}
	std::string input_schema_json() const override {
		return R"({"type":"object",)"
		       R"("properties":{)"
		       R"("query":{"type":"string","description":"The search query"})"
		       R"(},)"
		       R"("required":["query"]})";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "web_search \"" + v.str("query") + "\"";
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string query;
		try {
			auto v = json::Value::parse(input_json);
			query = v.str("query");
		} catch (const std::exception& e) {
			r.is_error = true;
			r.output = std::string("args: ") + e.what();
			return r;
		}
		if (query.empty()) {
			r.is_error = true;
			r.output = "query required";
			return r;
		}

		// Try engines in order of preference.
		const char* brave_key = std::getenv("BRAVE_API_KEY");
		if (brave_key && brave_key[0]) {
			r.output = search_brave(query, brave_key);
			return r;
		}
		const char* tavily_key = std::getenv("TAVILY_API_KEY");
		if (tavily_key && tavily_key[0]) {
			r.output = search_tavily(query, tavily_key);
			return r;
		}
		const char* searxng_url = std::getenv("SEARXNG_URL");
		if (searxng_url && searxng_url[0]) {
			r.output = search_searxng(query, searxng_url);
			return r;
		}

		r.is_error = true;
		r.output = "No search engine configured. Set one of:\n"
		           "  export BRAVE_API_KEY=...    (Brave Search)\n"
		           "  export TAVILY_API_KEY=...   (Tavily)\n"
		           "  export SEARXNG_URL=...      (SearXNG instance URL)";
		return r;
	}
};

}  // namespace

std::unique_ptr<Tool> MakeWebSearchTool() {
	return std::make_unique<WebSearchTool>();
}

}  // namespace pluto
