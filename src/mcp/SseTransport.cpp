// MCP SSE transport: connects to an SSE endpoint, reads the "endpoint"
// event to discover the JSON-RPC message URL, then uses HTTP POST for
// all subsequent requests.

#include "SseTransport.h"
#include "../version.h"

#include <curl/curl.h>

#include <cstdio>
#include <string>
#include <utility>

namespace pluto {

namespace {

struct HandshakeState {
	std::string buffer;
	std::string event_name;
	std::string data;
	std::string endpoint;
	bool found = false;
};

size_t handshake_write_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
	auto* st = static_cast<HandshakeState*>(ud);
	if (st->found) return 0;  // abort curl — we have what we need

	size_t bytes = size * nmemb;
	st->buffer.append(ptr, bytes);

	// Parse SSE events line by line.
	for (;;) {
		size_t nl = st->buffer.find('\n');
		if (nl == std::string::npos) break;
		std::string line = st->buffer.substr(0, nl);
		st->buffer.erase(0, nl + 1);
		while (!line.empty() && line.back() == '\r') line.pop_back();

		if (line.empty()) {
			// End of event block.
			if (st->event_name == "endpoint" && !st->data.empty()) {
				st->endpoint = st->data;
				st->found = true;
				return 0;
			}
			st->event_name.clear();
			st->data.clear();
			continue;
		}
		if (line.rfind("event:", 0) == 0) {
			size_t s = 6;
			while (s < line.size() && line[s] == ' ') ++s;
			st->event_name = line.substr(s);
		} else if (line.rfind("data:", 0) == 0) {
			size_t s = 5;
			while (s < line.size() && line[s] == ' ') ++s;
			if (!st->data.empty()) st->data += '\n';
			st->data += line.substr(s);
		}
	}
	return bytes;
}

size_t response_write_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
	auto* out = static_cast<std::string*>(ud);
	out->append(ptr, size * nmemb);
	return size * nmemb;
}

// Extract origin (scheme + host + port) from a URL.
std::string url_origin(const std::string& url) {
	size_t proto = url.find("://");
	if (proto == std::string::npos) return {};
	size_t path = url.find('/', proto + 3);
	return (path != std::string::npos) ? url.substr(0, path) : url;
}

// If `url` is relative, resolve it against `base_url`.
// If `url` is absolute, it must share the same origin as `base_url`
// to prevent server-supplied endpoint redirects (SSRF).
std::string resolve_url(const std::string& base_url, const std::string& url,
                        bool& origin_ok) {
	origin_ok = true;

	if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
		// Absolute URL — validate same origin.
		std::string base_origin = url_origin(base_url);
		std::string ep_origin = url_origin(url);
		if (base_origin.empty() || base_origin != ep_origin) {
			origin_ok = false;
			return {};
		}
		return url;
	}

	// Relative URL — resolve against base origin.
	std::string origin = url_origin(base_url);
	if (origin.empty()) { origin_ok = false; return url; }

	std::string resolved = url;
	if (!resolved.empty() && resolved[0] != '/') resolved = "/" + resolved;
	return origin + resolved;
}

}  // namespace

SseTransport::SseTransport(std::string sse_url,
                           std::map<std::string, std::string> headers)
	: fSseUrl(std::move(sse_url)), fHeaders(std::move(headers)) {}

bool SseTransport::start(std::string& err) {
	CURL* curl = curl_easy_init();
	if (!curl) { err = "curl_easy_init failed"; return false; }

	HandshakeState st;

	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "accept: text/event-stream");
	for (const auto& [k, v] : fHeaders) {
		std::string line = k + ": " + v;
		headers = curl_slist_append(headers, line.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, fSseUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handshake_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);
	// Handshake timeout: connect in 5s, wait up to 30s for the endpoint
	// event. The old 10s total was too tight for slow MCP servers or
	// first-time TLS negotiations.
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_CAINFO,
		"/system/data/ssl/CARootCertificates.pem");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);

	CURLcode rc = curl_easy_perform(curl);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	// CURLE_WRITE_ERROR is expected — the write callback returns 0 to abort
	// once the endpoint event is found.
	if (!st.found) {
		if (rc != CURLE_OK && rc != CURLE_WRITE_ERROR)
			err = "SSE handshake: " + std::string(curl_easy_strerror(rc));
		else
			err = "no 'endpoint' event from " + fSseUrl;
		return false;
	}

	bool origin_ok = false;
	fMessageUrl = resolve_url(fSseUrl, st.endpoint, origin_ok);
	if (!origin_ok) {
		err = "SSE endpoint origin mismatch: server returned '"
		    + st.endpoint + "' which differs from '" + fSseUrl + "'";
		return false;
	}
	std::fprintf(stderr, "[mcp/sse] endpoint: %s\n", fMessageUrl.c_str());
	return true;
}

std::string SseTransport::post(const std::string& body, std::string& err) {
	CURL* curl = curl_easy_init();
	if (!curl) { err = "curl_easy_init failed"; return {}; }

	std::string out;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "content-type: application/json");
	headers = curl_slist_append(headers, "accept: application/json");
	for (const auto& [k, v] : fHeaders) {
		std::string line = k + ": " + v;
		headers = curl_slist_append(headers, line.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, fMessageUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_CAINFO,
		"/system/data/ssl/CARootCertificates.pem");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);

	CURLcode rc = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK) {
		err = std::string("curl: ") + curl_easy_strerror(rc);
		return {};
	}
	if (http_code != 200 && http_code != 202) {
		err = "HTTP " + std::to_string(http_code) + ": " + out;
		return {};
	}
	return out;
}

std::string SseTransport::request(const std::string& body, std::string& err) {
	if (fMessageUrl.empty()) {
		err = "SSE transport not started — call start() first";
		return {};
	}
	return post(body, err);
}

void SseTransport::notify(const std::string& body) {
	std::string err;
	(void)post(body, err);
}

}  // namespace pluto
