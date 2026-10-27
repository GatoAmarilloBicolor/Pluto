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
    static std::optional<std::string> GetPinForHost(const std::string& host) {
        if (host == "api.anthropic.com") return "sha256/REPLACE_WITH_ACTUAL_PIN";
        if (host == "api.openai.com") return "sha256/REPLACE_WITH_ACTUAL_PIN";
        if (host == "generativelanguage.googleapis.com") return "sha256/REPLACE_WITH_ACTUAL_PIN";
        if (host == "api.x.ai") return "sha256/REPLACE_WITH_ACTUAL_PIN";
        if (host == "openrouter.ai") return "sha256/REPLACE_WITH_ACTUAL_PIN";
        if (host == "api.brave.com") return "sha256/REPLACE_WITH_ACTUAL_PIN";
        if (host == "api.tavily.com") return "sha256/REPLACE_WITH_ACTUAL_PIN";
        if (host == "api.x.ai") return "sha256/REPLACE_WITH_ACTUAL_PIN";
        return std::nullopt;
    }
};

// Apply all security settings to a curl handle for HTTPS URLs
// Returns true if settings applied (HTTPS), false if HTTP (no SSL)
inline bool ApplyCurlSecurity(CURL* curl, const std::string& url) {
    if (url.rfind("https://", 0) != 0) return false;

    // Core TLS verification
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/system/data/ssl/CARootCertificates.pem");

    // OCSP Stapling verification
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);

    // Connection limits
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 10L);
    curl_easy_setopt(curl, CURLOPT_MAX_HOST_CONNECTIONS, 5L);
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

// Extract host from URL for pinning
inline std::string ExtractHost(const std::string& url) {
    size_t proto = url.find("://");
    if (proto == std::string::npos) return "";
    size_t start = proto + 3;
    size_t end = url.find_first_of("/?#", start);
    return (end == std::string::npos) ? url.substr(start) : url.substr(start, end - start);
}

// Apply security with pinning for known providers
inline bool ApplyCurlSecurityWithPinning(CURL* curl, const std::string& url) {
    if (url.rfind("https://", 0) != 0) return false;

    if (!ApplyCurlSecurity(curl, url)) return false;

    // Certificate pinning for known providers
    std::string host = ExtractHost(url);
    if (auto pin = CurlSecurityConfig::GetPinForHost(host)) {
        curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, pin->c_str());
    }

    return true;
}

// Secure zeroization of sensitive memory
inline void SecureZero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile unsigned char* p = static_cast<volatile unsigned char*>(ptr);
    while (len--) *p++ = 0;
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
