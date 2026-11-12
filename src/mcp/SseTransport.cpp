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
#include <chrono>
#include <algorithm>

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

// Check if an IP is private/internal (RFC 1918, RFC 4193, loopback, link-local, etc.)
// This is a duplicate of the check in curl_security.h to avoid circular deps.
static bool is_private_ip(const std::string& ip) {
    // Simple check for common private ranges
    if (ip.rfind("10.", 0) == 0) return true;
    if (ip.rfind("192.168.", 0) == 0) return true;
    if (ip.rfind("172.", 0) == 0) {
        // 172.16.0.0 - 172.31.255.255
        size_t first_dot = ip.find('.');
        size_t second_dot = ip.find('.', first_dot + 1);
        if (second_dot != std::string::npos) {
            std::string second_octet_str = ip.substr(first_dot + 1, second_dot - first_dot - 1);
            try {
                int second_octet = std::stoi(second_octet_str);
                if (second_octet >= 16 && second_octet <= 31) return true;
            } catch (...) {}
        }
    }
    if (ip.rfind("127.", 0) == 0) return true;  // loopback
    if (ip.rfind("169.254.", 0) == 0) return true;  // link-local
    if (ip.rfind("fc", 0) == 0 || ip.rfind("fd", 0) == 0) return true;  // ULA
    if (ip == "::1" || ip == "::") return true;  // IPv6 loopback/unspecified
    return false;
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
            if (is_private_ip(ip)) {
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
                           std::map<std::string, std::string> headers,
                           Config config)
    : fSseUrl(std::move(sse_url)), fHeaders(std::move(headers)), fConfig(std::move(config)) {}

SseTransport::~SseTransport() {
    stop();
}

void SseTransport::stop() {
    stop_heartbeat();
    std::lock_guard<std::mutex> lock(fConnectionMutex);
    fConnected = false;
    fMessageUrl.clear();
    fPinnedIp.clear();
}

bool SseTransport::start(std::string& err) {
    std::lock_guard<std::mutex> lock(fConnectionMutex);
    
    if (fConnected) return true;
    
    for (int attempt = 1; attempt <= fConfig.max_reconnect_attempts; ++attempt) {
        if (!fCircuitBreaker.allow_request()) {
            err = "Circuit breaker open for " + fSseUrl + " (state: " + fCircuitBreaker.state() + ")";
            return false;
        }
        
        if (attempt > 1) {
            long delay = calc_backoff_ms(attempt - 1, fConfig.reconnect_base_delay_ms, fConfig.reconnect_max_delay_ms);
            std::fprintf(stderr, "[mcp/sse] Reconnect attempt %d/%d after %ldms\n", 
                attempt, fConfig.max_reconnect_attempts, delay);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        
        if (do_handshake(err)) {
            fConnected = true;
            fCircuitBreaker.record_success();
            start_heartbeat();
            return true;
        }
        
        fCircuitBreaker.record_failure();
        
        if (attempt < fConfig.max_reconnect_attempts) {
            std::fprintf(stderr, "[mcp/sse] Handshake failed (attempt %d): %s\n", attempt, err.c_str());
        }
    }
    
    return false;
}

bool SseTransport::do_handshake(std::string& err) {
    CURL* curl = curl_easy_init();
    if (!curl) { err = "curl_easy_init failed"; return false; }
    CurlGuard curl_guard(curl);

    // Pre-resolve and pin IP to prevent DNS rebinding
    std::string pinned_ip;
    {
        std::string host = url_host(fSseUrl);
        std::vector<std::string> ips;
        if (!resolve_host(host, ips) || ips.empty()) {
            err = "Failed to resolve host: " + host;
            return false;
        }
        for (const auto& ip : ips) {
            if (is_private_ip(ip)) {
                err = "Resolved IP not allowed: " + ip;
                return false;
            }
        }
        pinned_ip = ips[0];
        // Pin IP for this connection
        std::string resolve_entry = host + ":443:" + pinned_ip;
        struct curl_slist* resolve_list = nullptr;
        resolve_list = curl_slist_append(resolve_list, resolve_entry.c_str());
        if (!resolve_list) {
            err = "OOM: curl_slist_append failed";
            return false;
        }
        curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);
    }

    HandshakeState st;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "accept: text/event-stream");
    if (!headers) {
        err = "OOM: curl_slist_append failed";
        return false;
    }
    for (const auto& [k, v] : fHeaders) {
        std::string line = k + ": " + v;
        headers = curl_slist_append(headers, line.c_str());
        if (!headers) {
            curl_slist_free_all(headers);
            err = "OOM: curl_slist_append failed";
            return false;
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, fSseUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handshake_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, fConfig.handshake_timeout_ms / 1000);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, fConfig.connect_timeout_ms / 1000);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
    ApplyCurlSecurityWithPinning(curl, fSseUrl);

    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(headers);

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
    
    fPinnedIp = pinned_ip;
    std::fprintf(stderr, "[mcp/sse] endpoint: %s (pinned IP: %s)\n", fMessageUrl.c_str(), fPinnedIp.c_str());
    return true;
}

void SseTransport::ensure_connection(std::string& err) {
    if (fConnected.load()) return;
    
    std::lock_guard<std::mutex> lock(fConnectionMutex);
    if (fConnected.load()) return;  // Double-check after lock
    
    if (!start(err)) {
        fConnected = false;
    }
}

std::string SseTransport::request(const std::string& body, std::string& err) {
    ensure_connection(err);
    if (!fConnected.load()) {
        return {};
    }
    
    for (int attempt = 1; attempt <= fConfig.max_reconnect_attempts; ++attempt) {
        if (!fCircuitBreaker.allow_request()) {
            err = "Circuit breaker open for " + fMessageUrl + " (state: " + fCircuitBreaker.state() + ")";
            fConnected = false;
            return {};
        }
        
        std::string response;
        if (do_post(body, response, err)) {
            fCircuitBreaker.record_success();
            err.clear();  // Clear error on success
            return response;
        }
        
        fCircuitBreaker.record_failure();
        
        // On failure, mark connection as potentially dead and try to reconnect
        {
            std::lock_guard<std::mutex> lock(fConnectionMutex);
            fConnected = false;
        }
        stop_heartbeat();
        
        if (attempt < fConfig.max_reconnect_attempts) {
            std::fprintf(stderr, "[mcp/sse] Request failed (attempt %d): %s, reconnecting...\n", 
                attempt, err.c_str());
            
            if (start(err)) {
                continue;  // Retry with new connection
            }
        }
    }
    
    return {};
}

bool SseTransport::do_post(const std::string& body, std::string& response, std::string& err) {
    CURL* curl = curl_easy_init();
    if (!curl) { err = "curl_easy_init failed"; return false; }
    CurlGuard curl_guard(curl);

    // Use pinned IP from handshake to prevent DNS rebinding
    if (fPinnedIp.empty()) {
        err = "No pinned IP available";
        return false;
    }
    
    std::string message_host = url_host(fMessageUrl);
    std::string resolve_entry = message_host + ":443:" + fPinnedIp;
    struct curl_slist* resolve_list = nullptr;
    resolve_list = curl_slist_append(resolve_list, resolve_entry.c_str());
    if (!resolve_list) {
        err = "OOM: curl_slist_append failed";
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);

    std::string out;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "content-type: application/json");
    if (!headers) {
        curl_slist_free_all(resolve_list);
        err = "OOM: curl_slist_append failed";
        return false;
    }
    headers = curl_slist_append(headers, "accept: application/json");
    if (!headers) {
        curl_slist_free_all(resolve_list);
        err = "OOM: curl_slist_append failed";
        return false;
    }
    for (const auto& [k, v] : fHeaders) {
        std::string line = k + ": " + v;
        headers = curl_slist_append(headers, line.c_str());
        if (!headers) {
            curl_slist_free_all(resolve_list);
            err = "OOM: curl_slist_append failed";
            return false;
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, fMessageUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, fConfig.request_timeout_ms / 1000);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
    ApplyCurlSecurityWithPinning(curl, fMessageUrl);

    CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_slist_free_all(resolve_list);

    if (rc != CURLE_OK) {
        err = std::string("curl: ") + curl_easy_strerror(rc);
        return false;
    }
    if (http_code != 200 && http_code != 202) {
        err = "HTTP " + std::to_string(http_code) + ": " + out;
        return false;
    }
    response = std::move(out);
    return true;
}

void SseTransport::notify(const std::string& body) {
    std::string err;
    (void)request(body, err);
}

void SseTransport::start_heartbeat() {
    stop_heartbeat();
    fStopHeartbeat = false;
    fHeartbeatThread = std::thread(heartbeat_thread_func, this);
}

void SseTransport::stop_heartbeat() {
    fStopHeartbeat = true;
    if (fHeartbeatThread.joinable()) {
        fHeartbeatThread.join();
    }
}

void SseTransport::heartbeat_thread_func(SseTransport* self) {
    while (!self->fStopHeartbeat.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(self->fConfig.heartbeat_interval_ms));
        
        if (self->fStopHeartbeat.load()) break;
        
        // Check connection state without holding mutex during network I/O
        bool connected = self->fConnected.load();
        if (!connected) break;
        
        // Send a simple ping request to keep connection alive
        // Use a minimal JSON-RPC notification
        std::string ping = R"({"jsonrpc":"2.0","method":"ping","params":{}})";
        std::string err;
        std::string response;
        
        // Do the network request WITHOUT holding the mutex
        // Use a local copy of the pinned IP and message URL
        std::string pinned_ip;
        std::string message_url;
        {
            std::lock_guard<std::mutex> lock(self->fConnectionMutex);
            if (!self->fConnected.load()) break;
            pinned_ip = self->fPinnedIp;
            message_url = self->fMessageUrl;
        }
        
        if (pinned_ip.empty() || message_url.empty()) {
            std::lock_guard<std::mutex> lock(self->fConnectionMutex);
            self->fConnected = false;
            break;
        }
        
        CURL* curl = curl_easy_init();
        if (!curl) continue;
        CurlGuard curl_guard(curl);
        
        std::string message_host = url_host(message_url);
        std::string resolve_entry = message_host + ":443:" + pinned_ip;
        struct curl_slist* resolve_list = nullptr;
        resolve_list = curl_slist_append(resolve_list, resolve_entry.c_str());
        if (!resolve_list) {
            continue;
        }
        curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "content-type: application/json");
        if (!headers) {
            curl_slist_free_all(resolve_list);
            continue;
        }
        headers = curl_slist_append(headers, "accept: application/json");
        if (!headers) {
            curl_slist_free_all(resolve_list);
            curl_slist_free_all(headers);
            continue;
        }
        for (const auto& [k, v] : self->fHeaders) {
            std::string line = k + ": " + v;
            headers = curl_slist_append(headers, line.c_str());
            if (!headers) {
                curl_slist_free_all(resolve_list);
                curl_slist_free_all(headers);
                continue;
            }
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, message_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ping.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)ping.size());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
        ApplyCurlSecurityWithPinning(curl, message_url);
        
        // We don't care about the response, just that the connection works
        CURLcode rc = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_slist_free_all(headers);
        curl_slist_free_all(resolve_list);
        
        if (rc != CURLE_OK || http_code < 200 || http_code >= 400) {
            std::fprintf(stderr, "[mcp/sse] Heartbeat failed (rc=%d, http=%ld), marking disconnected\n", rc, http_code);
            std::lock_guard<std::mutex> lock(self->fConnectionMutex);
            self->fConnected = false;
            break;
        }
    }
}

}  // namespace pluto