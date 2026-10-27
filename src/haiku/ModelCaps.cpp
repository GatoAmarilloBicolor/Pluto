// ModelCaps.cpp — dynamic model capability discovery.
// Fetches /v1/models from provider base URLs and extracts per-model
// context_window and max_output_tokens when advertised by the provider.
// Falls back to hardcoded defaults when the endpoint is unreachable
// or doesn't advertise limits.

#include "ModelCaps.h"

#include <curl/curl.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

#include "json.h"

namespace pluto {

namespace {

struct CurlStr {
	std::string data;
	static size_t cb(char* p, size_t sz, size_t nm, void* ud) {
		auto* s = static_cast<CurlStr*>(ud);
		s->data.append(p, sz * nm);
		return sz * nm;
	}
};

std::string http_get_json(const std::string& url) {
	CURL* c = curl_easy_init();
	if (!c) return {};
	CurlStr out;
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, CurlStr::cb);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 8L);
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 3L);
	curl_easy_setopt(c, CURLOPT_USERAGENT, "pluto/0.7.1 (Haiku)");
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	// Enable SSL verification for HTTPS URLs
	if (url.rfind("https://", 0) == 0) {
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYSTATUS, 1L);
		curl_easy_setopt(c, CURLOPT_CAINFO,
			"/system/data/ssl/CARootCertificates.pem");
	}
	CURLcode rc = curl_easy_perform(c);
	curl_easy_cleanup(c);
	if (rc != CURLE_OK) return {};
	return out.data;
}

// Provider-specific model list paths.
std::string models_url(const std::string& base_url) {
	if (base_url.empty()) return {};
	std::string b = base_url;
	while (!b.empty() && b.back() == '/') b.pop_back();
	if (b.find("api.anthropic.com") != std::string::npos)
		return b + "/v1/models";
	if (b.find("api.openai.com") != std::string::npos)
		return b + "/v1/models";
	if (b.find("openrouter.ai") != std::string::npos)
		return b + "/api/v1/models";
	if (b.find("generativelanguage.googleapis.com") != std::string::npos)
		return b + "/v1beta/models";
	if (b.find("api.x.ai") != std::string::npos)
		return b + "/v1/models";
	if (b.find("localhost:11434") != std::string::npos)
		return b + "/api/tags";
	return b + "/v1/models";
}

int extract_int(const json::Value& v, const char* key, int def) {
	const json::Value* f = v.find(key);
	if (!f || !f->is_num()) return def;
	return static_cast<int>(f->as_num());
}

// Hardcoded fallback guesses (same as config.cpp defaults).
int guess_max_output(const std::string& model) {
	if (model.find("opus") != std::string::npos)     return 32000;
	if (model.find("sonnet") != std::string::npos)   return 64000;
	if (model.find("haiku") != std::string::npos)    return 64000;
	if (model.find("claude") != std::string::npos)   return 64000;
	if (model.find("gpt-4o") != std::string::npos)   return 16384;
	if (model.find("gpt-4") != std::string::npos)    return 16384;
	if (model.find("o1") != std::string::npos)       return 100000;
	if (model.find("o3") != std::string::npos)       return 100000;
	if (model.find("o4") != std::string::npos)       return 100000;
	if (model.find("gemini") != std::string::npos)   return 65536;
	if (model.find("deepseek") != std::string::npos) return 8192;
	return 8192;
}

int guess_context(const std::string& model) {
	if (model.find("opus") != std::string::npos)     return 1000000;
	if (model.find("sonnet") != std::string::npos)   return 200000;
	if (model.find("haiku") != std::string::npos)    return 200000;
	if (model.find("claude") != std::string::npos)   return 200000;
	if (model.find("gpt-4") != std::string::npos)    return 128000;
	if (model.find("gpt-oss") != std::string::npos)  return 128000;
	if (model.find("gemini") != std::string::npos)   return 1000000;
	if (model.find("gemma-4") != std::string::npos)  return 128000;
	if (model.find("gemma-3") != std::string::npos)  return 32000;
	if (model.find("llama") != std::string::npos)    return 131072;
	if (model.find("qwen") != std::string::npos)     return 131072;
	if (model.find("deepseek") != std::string::npos) return 64000;
	if (model.find("mistral") != std::string::npos)  return 32000;
	return 128000;
}

// Global cache keyed by "provider:model".
std::map<std::string, ModelCaps> g_caps_cache;
std::mutex g_caps_mutex;

}  // namespace

ModelCaps LookupModelCaps(const std::string& provider,
                          const std::string& base_url,
                          const std::string& model) {
	std::lock_guard<std::mutex> lk(g_caps_mutex);
	std::string key = provider + ":" + model;
	auto it = g_caps_cache.find(key);
	if (it != g_caps_cache.end())
		return it->second;

	ModelCaps caps;
	caps.max_output_tokens = guess_max_output(model);
	caps.context_window    = guess_context(model);
	caps.known = true;

	// Try to fetch from provider endpoint.
	std::string url = models_url(base_url);
	if (!url.empty()) {
		std::string body = http_get_json(url);
		if (!body.empty()) {
			try {
				json::Value root = json::Value::parse(body);
				const json::Value* arr = root.find("data");
				if (!arr || !arr->is_arr())
					arr = root.find("models");
				if (arr && arr->is_arr()) {
					for (const auto& m : arr->as_arr()) {
						std::string mid = m.str("id");
						if (mid == model) {
							caps.max_output_tokens = extract_int(m, "max_output_tokens", caps.max_output_tokens);
							caps.context_window    = extract_int(m, "context_window", caps.context_window);
							break;
						}
					}
				}
			} catch (const std::exception&) {}
		}
	}

	g_caps_cache[key] = caps;
	return caps;
}

void PrefetchModelCaps(const std::string& provider,
                       const std::string& base_url,
                       const std::string& model) {
	LookupModelCaps(provider, base_url, model);
}

}  // namespace pluto
