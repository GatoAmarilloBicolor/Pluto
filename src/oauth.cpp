#include "oauth.h"

#ifdef __HAIKU__

// OAuth 2.0 + PKCE authentication for Claude Max/Pro plans.
// Implements same flow as Claude Code.

#include "filelock.h"
#include "json.h"
#include "util.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>

#include <Roster.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace pluto {

namespace oauth_internal {

// Fallback implementations for crypto operations (Haiku doesn't have BHash/BRandom/BBase64)

// Generate cryptographically secure random bytes using OpenSSL
static bool GenerateRandomBytes(void* buf, size_t len) {
    return RAND_bytes(static_cast<unsigned char*>(buf), static_cast<int>(len)) == 1;
}

// SHA256 hash using OpenSSL
static bool SHA256(const void* data, size_t len, unsigned char* out) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;
    bool ok = true;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) ok = false;
    if (ok && EVP_DigestUpdate(ctx, data, len) != 1) ok = false;
    unsigned int outLen = 0;
    if (ok && EVP_DigestFinal_ex(ctx, out, &outLen) != 1) ok = false;
    EVP_MD_CTX_free(ctx);
    return ok && outLen == 32;
}

// Base64 URL encoding (no padding, -_ instead of +/)
static std::string Base64URLEncode(const unsigned char* data, size_t len) {
    static const char alphabet[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) b |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) b |= static_cast<uint32_t>(data[i + 2]);
        
        out += alphabet[(b >> 18) & 0x3f];
        out += alphabet[(b >> 12) & 0x3f];
        if (i + 1 < len) out += alphabet[(b >> 6) & 0x3f];
        if (i + 2 < len) out += alphabet[b & 0x3f];
    }
    return out;
}

// URL encoding (percent encoding)
static std::string URLEncode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_'
            || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

const char* kClientId = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
const char* kAuthUrl  = "https://claude.com/cai/oauth/authorize";
const char* kTokenUrl = "https://platform.claude.com/v1/oauth/token";
const char* kProfileUrl = "https://api.anthropic.com/api/oauth/profile";
const char* kScope    = "user:inference user:profile org:create_api_key";

std::string home_dir() {
    return HomeDir();
}

// PKCE code verifier generation (48 random bytes -> 64 char base64url)
std::string GenerateCodeVerifier() {
    unsigned char buf[48];
    if (!GenerateRandomBytes(buf, sizeof(buf))) {
        // Fallback to less secure random if OpenSSL fails
        for (size_t i = 0; i < sizeof(buf); ++i) buf[i] = static_cast<unsigned char>(rand());
    }
    return Base64URLEncode(buf, sizeof(buf));
}

// PKCE code challenge (SHA256 of verifier -> base64url)
std::string GenerateCodeChallenge(const std::string& verifier) {
    unsigned char hash[32];
    if (!SHA256(verifier.data(), verifier.size(), hash)) {
        return "";
    }
    return Base64URLEncode(hash, sizeof(hash));
}

// State parameter (16 random bytes -> base64url)
std::string GenerateState() {
    unsigned char buf[16];
    if (!GenerateRandomBytes(buf, sizeof(buf))) {
        for (size_t i = 0; i < sizeof(buf); ++i) buf[i] = static_cast<unsigned char>(rand());
    }
    return Base64URLEncode(buf, sizeof(buf));
}

size_t curl_collect_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
    static_cast<std::string*>(ud)->append(ptr, size * nmemb);
    return size * nmemb;
}

// Simple synchronous HTTP POST returning body. Returns empty on error.
std::string http_post_json(const std::string& url, const std::string& body) {
    CURL* c = curl_easy_init();
    if (!c) return {};
    std::string resp;
    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_collect_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "pluto/0.3.0 (Haiku)");
    curl_easy_setopt(c, CURLOPT_CAINFO,
        "/system/data/ssl/CARootCertificates.pem");
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    CURLcode rc = curl_easy_perform(c);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK) {
        std::fprintf(stderr, "oauth: POST %s failed: %s\n",
            url.c_str(), curl_easy_strerror(rc));
        return {};
    }
    return resp;
}

// Simple synchronous HTTP GET with auth header.
std::string http_get_auth(const std::string& url,
    const std::string& bearer_token) {
    CURL* c = curl_easy_init();
    if (!c) return {};
    std::string resp;
    struct curl_slist* hdrs = nullptr;
    std::string auth = "Authorization: Bearer " + bearer_token;
    hdrs = curl_slist_append(hdrs, auth.c_str());
    hdrs = curl_slist_append(hdrs, "anthropic-beta: oauth-2025-04-20");
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_collect_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "pluto/0.3.0 (Haiku)");
    curl_easy_setopt(c, CURLOPT_CAINFO,
        "/system/data/ssl/CARootCertificates.pem");
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    CURLcode rc = curl_easy_perform(c);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK) {
        return {};
    }
    return resp;
}

std::string OAuthTokensPath() {
    return HomeDir() + "/config/settings/pluto/oauth-tokens";
}

bool SaveOAuthTokens(const OAuthTokens& tokens) {
    std::string path = OAuthTokensPath();
    ScopedFileLock lock(path + ".lock");
    if (!lock.locked()) return false;

    std::string tmp = path + ".tmp." + std::to_string(getpid());
    // Create with restricted permissions from start (no TOCTOU window).
    int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    json::Value v;
    v.set("access_token", tokens.access_token);
    v.set("refresh_token", tokens.refresh_token);
    v.set("expires_at", static_cast<double>(tokens.expires_at));
    std::string data = v.serialize();
    ssize_t written = ::write(fd, data.c_str(), data.size());
    ::close(fd);
    if (written != (ssize_t)data.size()) {
        std::remove(tmp.c_str());
        return false;
    }

    if (::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

OAuthTokens LoadOAuthTokens() {
    OAuthTokens tokens;
    std::ifstream f(OAuthTokensPath());
    if (!f) return tokens;
    std::ostringstream os;
    os << f.rdbuf();
    std::string body = os.str();
    if (body.empty()) return tokens;
    try {
        json::Value v = json::Value::parse(body);
        tokens.access_token = v.str("access_token");
        tokens.refresh_token = v.str("refresh_token");
        const json::Value* ea = v.find("expires_at");
        if (ea && ea->is_num()) tokens.expires_at = static_cast<time_t>(ea->as_num());
    } catch (...) {
        return tokens;
    }
    return tokens;
}

void DeleteOAuthTokens() {
    std::remove(OAuthTokensPath().c_str());
}

std::string ExtractQueryParam(const std::string& url, const std::string& name) {
    size_t qmark = url.find('?');
    if (qmark == std::string::npos) return {};
    std::string query = url.substr(qmark + 1);
    std::string needle = name + "=";
    for (size_t pos = 0; pos < query.size(); ) {
        if (query.compare(pos, needle.size(), needle) == 0) {
            size_t vstart = pos + needle.size();
            size_t vend = query.find('&', vstart);
            return query.substr(vstart, vend == std::string::npos ? query.size() - vstart : vend - vstart);
        }
        pos = query.find('&', pos) + 1;
        if (pos == 0 || pos > query.size()) break;
    }
    return {};
}

bool OAuthLogin() {
    std::string verifier = GenerateCodeVerifier();
    std::string challenge = GenerateCodeChallenge(verifier);
    std::string state = GenerateState();

    // Bind local socket on random port.
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        std::fprintf(stderr, "oauth: cannot create socket: %s\n", strerror(errno));
        return false;
    }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = 0; // OS picks a free port
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::fprintf(stderr, "oauth: bind: %s\n", strerror(errno));
        close(srv);
        return false;
    }
    listen(srv, 1);

    // Read assigned port.
    socklen_t alen = sizeof(addr);
    getsockname(srv, (struct sockaddr*)&addr, &alen);
    int port = ntohs(addr.sin_port);

    std::string redirect_uri = "http://localhost:" + std::to_string(port) + "/callback";

    // Build authorization URL.
    std::string auth_url = std::string(kAuthUrl)
        + "?client_id=" + kClientId
        + "&response_type=code"
        + "&redirect_uri=" + URLEncode(redirect_uri)
        + "&scope=" + URLEncode(kScope)
        + "&state=" + state
        + "&code_challenge=" + challenge
        + "&code_challenge_method=S256"
        + "&code=true";

    std::fprintf(stderr, "Opening browser for Claude login...\n");
    std::fprintf(stderr, " -> %s\n", auth_url.c_str());

    // Try Haiku's be_roster first.
    char* argv[] = {const_cast<char*>("open"), const_cast<char*>(auth_url.c_str()), nullptr};
    if (BRoster::be_roster && BRoster::be_roster->Launch("text/html", 1, const_cast<const char**>(argv)) == B_OK) {
        // Success
    } else {
        // Fallback to system() with open command
        std::string cmd = "open \"" + auth_url + "\" >/dev/null 2>&1 &";
        std::system(cmd.c_str());
    }

    // Wait for callback with timeout.
    struct timeval tv = {120, 0};
    setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client = accept(srv, (struct sockaddr*)&client_addr, &client_len);
    close(srv);
    if (client < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::fprintf(stderr, "oauth: timeout waiting for callback\n");
        } else {
            std::fprintf(stderr, "oauth: accept: %s\n", strerror(errno));
        }
        return false;
    }

    // Read HTTP request (headers only).
    char buf[4096];
    ssize_t total = 0;
    while (total < (ssize_t)sizeof(buf) - 1) {
        ssize_t n = read(client, buf + total, sizeof(buf) - 1 - total);
        if (n <= 0) break;
        total += n;
        // Stop when headers complete.
        if (std::strstr(buf, "\r\n\r\n") || std::strstr(buf, "\n\n"))
            break;
    }
    if (total <= 0) {
        close(client);
        std::fprintf(stderr, "oauth: empty callback request\n");
        return false;
    }
    buf[total] = '\0';

    // Send response page before processing.
    const char* response_html =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html><body style='font-family:sans-serif;"
        "text-align:center;padding:60px'>"
        "<h2>Login successful!</h2>"
        "<p>You close tab and return to pluto.</p>"
        "</body></html>";
    write(client, response_html, strlen(response_html));
    close(client);

    // Parse GET line: "GET /callback?code=...&state=... HTTP/1.1"
    std::string request(buf, total);
    size_t get_end = request.find(" HTTP/");
    if (get_end == std::string::npos) {
        std::fprintf(stderr, "oauth: malformed callback request\n");
        return false;
    }
    std::string get_path = request.substr(4, get_end - 4); // skip "GET "

    std::string code = ExtractQueryParam(get_path, "code");
    std::string cb_state = ExtractQueryParam(get_path, "state");

    if (code.empty()) {
        std::fprintf(stderr, "oauth: no authorization code in callback\n");
        std::string error = ExtractQueryParam(get_path, "error");
        if (!error.empty())
            std::fprintf(stderr, " error: %s\n", error.c_str());
        return false;
    }

    // CSRF check
    if (cb_state != state) {
        std::fprintf(stderr, "oauth: state mismatch (CSRF?)\n");
        return false;
    }

    // Exchange code for tokens.
    json::Value token_req;
    token_req.set("grant_type", std::string("authorization_code"));
    token_req.set("code", code);
    token_req.set("client_id", std::string(kClientId));
    token_req.set("code_verifier", verifier);
    token_req.set("redirect_uri", redirect_uri);
    token_req.set("state", state);

    std::string resp = http_post_json(kTokenUrl, token_req.serialize());
    if (resp.empty()) {
        std::fprintf(stderr, "oauth: token exchange failed (network error)\n");
        return false;
    }

    try {
        json::Value v = json::Value::parse(resp);
        std::string at = v.str("access_token");
        std::string rt = v.str("refresh_token");
        std::string err = v.str("error");
        if (at.empty()) {
            std::fprintf(stderr, "oauth: token error: %s\n",
                err.empty() ? resp.c_str() : err.c_str());
            return false;
        }

        const json::Value* ei = v.find("expires_in");
        time_t expires_in = (ei && ei->is_num()) ? static_cast<time_t>(ei->as_num()) : 3600;

        OAuthTokens tokens;
        tokens.access_token = at;
        tokens.refresh_token = rt;
        tokens.expires_at = std::time(nullptr) + expires_in;

        if (!pluto::SaveOAuthTokens(tokens)) {
            std::fprintf(stderr, "oauth: failed to save tokens\n");
            return false;
        }
    } catch (const std::exception&) {
        std::fprintf(stderr, "oauth: failed to parse token response\n");
        return false;
    }
    return true;
}

// ---- Token refresh ----

std::string OAuthEnsureValidToken(OAuthTokens& tokens) {
    if (tokens.empty()) return {};
    if (!tokens.expired()) return tokens.access_token;

    // Need refresh.
    if (tokens.refresh_token.empty()) return {};

    json::Value req;
    req.set("grant_type", std::string("refresh_token"));
    req.set("refresh_token", tokens.refresh_token);
    req.set("client_id", std::string(kClientId));
    req.set("scope", std::string(kScope));

    std::string resp = http_post_json(kTokenUrl, req.serialize());
    if (resp.empty()) {
        std::fprintf(stderr, "oauth: refresh failed (network error)\n"
            " Try 'pluto login' again.\n");
        return {};
    }

    try {
        json::Value v = json::Value::parse(resp);
        std::string at = v.str("access_token");
        if (at.empty()) {
            std::fprintf(stderr, "oauth: refresh error: %s\n"
                " Try 'pluto login' again.\n", resp.c_str());
            return {};
        }
        std::string rt = v.str("refresh_token");
        const json::Value* ei = v.find("expires_in");
        int expires_in = (ei && ei->is_num()) ? static_cast<int>(ei->as_num()) : 3600;

        tokens.access_token = at;
        if (!rt.empty()) tokens.refresh_token = rt;
        tokens.expires_at = std::time(nullptr) + expires_in;
pluto::SaveOAuthTokens(tokens);
        return tokens.access_token;
    } catch (...) {
        std::fprintf(stderr, "oauth: failed to parse refresh response\n");
        return {};
    }
}

// ---- Profile ----

OAuthProfile OAuthGetProfile(const std::string& access_token) {
    OAuthProfile prof;
    std::string resp = http_get_auth(kProfileUrl, access_token);
    if (resp.empty()) return prof;

    try {
        json::Value v = json::Value::parse(resp);
        prof.display_name = v.str("display_name");
        if (prof.display_name.empty()) prof.display_name = v.str("name");
        prof.organization_type = v.str("organization_type");
    } catch (...) {}
    return prof;
}

} // namespace oauth_internal

// Public API

OAuthTokens LoadOAuthTokens() {
    return pluto::oauth_internal::LoadOAuthTokens();
}

bool SaveOAuthTokens(const OAuthTokens& tokens) {
    return pluto::oauth_internal::SaveOAuthTokens(tokens);
}

void DeleteOAuthTokens() {
    pluto::oauth_internal::DeleteOAuthTokens();
}

std::string OAuthEnsureValidToken(OAuthTokens& tokens) {
    return pluto::oauth_internal::OAuthEnsureValidToken(tokens);
}

OAuthProfile OAuthGetProfile(const std::string& access_token) {
    return pluto::oauth_internal::OAuthGetProfile(access_token);
}

bool OAuthLogin() {
    return pluto::oauth_internal::OAuthLogin();
}

} // namespace pluto

#endif // __HAIKU__