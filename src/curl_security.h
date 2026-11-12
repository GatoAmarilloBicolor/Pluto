#pragma once

#include <curl/curl.h>
#include <string>
#include <optional>

namespace pluto {

// Centralized curl security configuration
// Applies consistent, strong TLS settings across all HTTPS requests

struct CurlSecurityConfig {
    // Known provider public key pins (SHA256 base64)
    // Format: "sha256/<base64>"
    // Certificate pins for known providers (SHA256 base64)
    // Obtain with: openssl s_client -servername HOST -connect HOST:443 < /dev/null | openssl x509 -pubkey -noout | openssl pkey -pubin -outform der | openssl dgst -sha256 -binary | base64
    // NOTE: Pins rotate with certificate renewal (~90 days). Update via fetch_pins.sh script.
    // Return const char* to string literals (static storage duration) to avoid dangling pointers
    // when passed to CURLOPT_PINNEDPUBLICKEY (libcurl may not copy immediately on all platforms).
    static std::optional<const char*> GetPinForHost(const std::string& host) {
        if (host == "api.anthropic.com") return "sha256/n+OFHb16YVI8KEux43Lk4jjFsK76xLolavlcACQEXq4=";
        if (host == "api.openai.com") return "sha256/zgJl30S+m6z88y1sEqdP+1tyIuSuyEXkT1J6dAdEZXw=";
        if (host == "generativelanguage.googleapis.com") return "sha256/udzO2pA9ijQ04FhrbsPAtyxOJF/gGKyaFnGZHBIERu8=";
        if (host == "api.x.ai") return "sha256/5qFXq18qBWOnQeki85TW6rVzfkFFBH6Zgqzw4PJ6F/Y=";
        if (host == "api.brave.com") return "sha256/OpiUzqarJTOGR4tXR0aXXIbYiyXi1+aSHPq/o5Ol6dM=";
        if (host == "api.tavily.com") return "sha256/0lfaR8dbvRshBzvIOonl+CP49hbiDOP2I8wULXoGxus=";
        return std::nullopt;
    }
};

// Forward declarations — defined below.
inline std::string ExtractHost(const std::string& url);
inline bool IsBlockedHost(const std::string& host);

// Apply all security settings to a curl handle for HTTPS URLs
// Returns true if settings applied (HTTPS), false if HTTP (no SSL)
inline bool ApplyCurlSecurity(CURL* curl, const std::string& url) {
    if (url.rfind("https://", 0) != 0) return false;

    // Core TLS verification
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/boot/system/data/ssl/CARootCertificates.pem");

    // NOTE: OCSP Stapling verification (CURLOPT_SSL_VERIFYSTATUS) intentionally
    // disabled — many MCP/proxy servers don't support OCSP stapling, causing
    // spurious "SSL certificate status verification failed" errors. Basic peer
    // and host verification remain enforced.

    // For localhost/private IPs (self-hosted models, local Ollama, etc.),
    // allow self-signed certificates — the CA bundle won't have them.
    if (IsBlockedHost(ExtractHost(url))) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // Connection limits
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 10L);
    // CURLOPT_MAX_HOST_CONNECTIONS is a multi handle option (CURLMOPT_MAX_HOST_CONNECTIONS)
    // For easy handles, use CURLOPT_MAXCONNECTS
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    // Timeouts (fail fast)
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);

    // Size limits (prevent DoS via large responses)
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)50 * 1024 * 1024); // 50MB max

    return true;
}

// Extract host from URL for pinning (strips port if present)
inline std::string ExtractHost(const std::string& url) {
    size_t proto = url.find("://");
    if (proto == std::string::npos) return "";
    size_t start = proto + 3;
    size_t end = url.find_first_of("/?#", start);
    std::string host = (end == std::string::npos) ? url.substr(start) : url.substr(start, end - start);
    // Strip port if present
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
        host = host.substr(0, colon);
    }
    return host;
}

// Apply security with pinning for known providers
inline bool ApplyCurlSecurityWithPinning(CURL* curl, const std::string& url) {
    if (url.rfind("https://", 0) != 0) return false;

    if (!ApplyCurlSecurity(curl, url)) return false;

    // Certificate pinning for known providers
    std::string host = ExtractHost(url);
    if (auto pin = CurlSecurityConfig::GetPinForHost(host)) {
        curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, *pin);
    }

    return true;
}

// Secure zeroization of sensitive memory
inline void SecureZero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile unsigned char* p = static_cast<volatile unsigned char*>(ptr);
    while (len--) *p++ = 0;
}

// SSRF protection: check if an IP address is private/internal
inline bool IsPrivateIP(const std::string& ip) {
    if (ip == "127.0.0.1" || ip == "::1" || ip == "0.0.0.0") return true;
    if (ip.rfind("10.", 0) == 0) return true;                    // 10.0.0.0/8
    if (ip.rfind("192.168.", 0) == 0) return true;              // 192.168.0.0/16
    if (ip.rfind("169.254.", 0) == 0) return true;              // 169.254.0.0/16 (link-local)
    // 172.16.0.0/12 = 172.16.0.0 - 172.31.255.255
    if (ip.rfind("172.", 0) == 0) {
        size_t dot1 = ip.find('.', 4);
        if (dot1 != std::string::npos) {
            std::string second = ip.substr(4, dot1 - 4);
            int octet = 0;
            try { octet = std::stoi(second); } catch (...) {}
            if (octet >= 16 && octet <= 31) return true;
        }
    }
    if (ip.rfind("fc00:", 0) == 0 || ip.rfind("fd00:", 0) == 0) return true; // ULA
    return false;
}

// SSRF protection: check if a host is blocked (localhost or private IP)
inline bool IsBlockedHost(const std::string& host) {
    // Strip port if present
    std::string h = host;
    size_t colon = h.find(':');
    if (colon != std::string::npos) {
        h = h.substr(0, colon);
    }
    
    if (h == "localhost" || h == "localhost.localdomain") return true;
    // Check if host is an IP address
    if (!h.empty() && (std::isdigit(h[0]) || h[0] == '[')) {
        std::string ip = h;
        if (ip[0] == '[') { // IPv6 with brackets
            size_t end = ip.find(']');
            if (end != std::string::npos) ip = ip.substr(1, end - 1);
        }
        return IsPrivateIP(ip);
    }
    return false;
}

// Secure string that zeroizes on destruction
class SecureString {
    std::string data_;
public:
    SecureString() = default;
    explicit SecureString(std::string&& s) : data_(std::move(s)) {}
    ~SecureString() { SecureZero(data_.data(), data_.size()); }
    
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;
    SecureString(SecureString&& other) noexcept : data_(std::move(other.data_)) {}
    SecureString& operator=(SecureString&& other) noexcept {
        SecureZero(data_.data(), data_.size());
        data_ = std::move(other.data_);
        return *this;
    }
    
    const std::string& get() const { return data_; }
    std::string& get() { return data_; }
    const char* c_str() const { return data_.c_str(); }
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void clear() { SecureZero(data_.data(), data_.size()); data_.clear(); }
};

} // namespace pluto
