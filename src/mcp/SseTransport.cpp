// MCP SSE transport: connects to an SSE endpoint, reads the "endpoint"
// event to discover the JSON-RPC message URL, then uses HTTP POST for
// all subsequent requests.

#include "SseTransport.h"
#include "../version.h"
#include "curl_security.h"
#include <cstdio>
#include <curl/curl.h>
#include <string>
#include <utility>
#include <vector>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

// Extract hostname from URL (without scheme, port, path)
std::string url_host(const std::string& url) {
	size_t proto = url.find("://");
	if (proto == std::string::npos) return {};
	size_t start = proto + 3;
	size_t end = url.find_first_of(":/", start);
	if (end == std::string::npos) end = url.size();
	return url.substr(start, end - start);
}

// Resolve hostname to IP addresses
static bool resolve_host(const std::string& host, std::vector<std::string>& ips) {
	char buf[INET6_ADDRSTRLEN];
	struct addrinfo hints = {}, *res = nullptr, *p;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0) return false;
	for (p = res; p; p = p->ai_next) {
		void* addr = (p->ai_family == AF_INET) 
			? (void*)&((struct sockaddr_in*)p->ai_addr)->sin_addr
			: (void*)&((struct sockaddr_in6*)p->ai_addr)->sin6_addr;
		if (inet_ntop(p->ai_family, addr, buf, sizeof(buf))) ips.push_back(buf);
	}
	freeaddrinfo(res);
	return !ips.empty();
}



// If `url` is relative, resolve it against `base_url`.
// If `url` is absolute, it must share the same origin as `base_url`
// to prevent server-supplied endpoint redirects (SSRF).
// Additionally, resolve and pin IP at connection time to prevent DNS rebinding.
std::string resolve_url(const std::string& base_url, const std::string& url,
                        bool& origin_ok, std::string* pinned_ip_out) {
	origin_ok = true;

	if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
		// Absolute URL — validate same origin.
		std::string base_origin = url_origin(base_url);
		std::string ep_origin = url_origin(url);
		if (base_origin.empty() || base_origin != ep_origin) {
			origin_ok = false;
			return {};
		}
		
		// DNS rebinding protection: resolve hostname to IP and pin it
		std::string host = url_host(url);
		std::vector<std::string> ips;
		if (!resolve_host(host, ips)) {
			origin_ok = false;
			return {};
		}
		
		// Validate first IP is allowed (public, not private/internal)
		for (const auto& ip : ips) {
			if (pluto::IsPrivateIP(ip)) {
				origin_ok = false;
				return {};
			}
		}
		
		if (pinned_ip_out && !ips.empty()) {
			*pinned_ip_out = ips[0];
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

	// Pre-resolve and pin IP to prevent DNS rebinding
	std::string pinned_ip;
	{
		std::string host = url_host(fSseUrl);
		std::vector<std::string> ips;
		if (!resolve_host(host, ips) || ips.empty()) {
			curl_easy_cleanup(curl);
			err = "Failed to resolve host: " + host;
			return false;
		}
		for (const auto& ip : ips) {
			if (pluto::IsPrivateIP(ip)) {
				curl_easy_cleanup(curl);
				err = "Resolved IP not allowed: " + ip;
				return false;
			}
		}
		pinned_ip = ips[0];
		// Pin IP for this connection
		std::string resolve_entry = host + ":443:" + pinned_ip;
		struct curl_slist* resolve_list = nullptr;
		resolve_list = curl_slist_append(resolve_list, resolve_entry.c_str());
		curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);
	}

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
	ApplyCurlSecurityWithPinning(curl, fSseUrl);

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
	fMessageUrl = resolve_url(fSseUrl, st.endpoint, origin_ok, &pinned_ip);
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

	// Pre-resolve and pin IP for message URL to prevent DNS rebinding
	std::string message_host = url_host(fMessageUrl);
	std::vector<std::string> message_ips;
	if (!resolve_host(message_host, message_ips) || message_ips.empty()) {
		curl_easy_cleanup(curl);
		err = "Failed to resolve message host: " + message_host;
		return {};
	}
	for (const auto& ip : message_ips) {
		if (!pluto::IsPrivateIP(ip)) {
			curl_easy_cleanup(curl);
			err = "Message host IP not allowed: " + ip;
			return {};
		}
	}
	
	std::string resolve_entry = message_host + ":443:" + message_ips[0];
	struct curl_slist* resolve_list = nullptr;
	resolve_list = curl_slist_append(resolve_list, resolve_entry.c_str());
	curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);

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
	curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
	ApplyCurlSecurityWithPinning(curl, fMessageUrl);

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
