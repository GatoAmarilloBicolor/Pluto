// OAuth 2.0 + PKCE authentication for Claude Max/Pro plans.
// Implements the same flow as Claude Code.

#include "oauth.h"
#include "filelock.h"
#include "json.h"
#include "util.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

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

namespace pluto {

namespace {

const char* kClientId = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
const char* kAuthUrl  = "https://claude.com/cai/oauth/authorize";
const char* kTokenUrl = "https://platform.claude.com/v1/oauth/token";
const char* kProfileUrl = "https://api.anthropic.com/api/oauth/profile";
const char* kScope    = "user:inference user:profile org:create_api_key";

std::string home_dir() {
	return HomeDir();
}

// base64url encode (no padding).
std::string base64url_encode(const unsigned char* data, size_t len) {
	static const char table[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string out;
	out.reserve((len * 4 + 2) / 3);
	for (size_t i = 0; i < len; i += 3) {
		unsigned int b = (unsigned int)data[i] << 16;
		if (i + 1 < len) b |= (unsigned int)data[i + 1] << 8;
		if (i + 2 < len) b |= (unsigned int)data[i + 2];
		out += table[(b >> 18) & 0x3f];
		out += table[(b >> 12) & 0x3f];
		if (i + 1 < len) out += table[(b >> 6) & 0x3f];
		if (i + 2 < len) out += table[b & 0x3f];
	}
	return out;
}

std::string url_encode(const std::string& s) {
	std::string out;
	for (unsigned char c : s) {
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
		    || (c >= '0' && c <= '9') || c == '-' || c == '_'
		    || c == '.' || c == '~') {
			out += (char)c;
		} else {
			char buf[4];
			std::snprintf(buf, sizeof(buf), "%%%02X", c);
			out += buf;
		}
	}
	return out;
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
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(c, CURLOPT_USERAGENT, "pluto/0.3.0 (Haiku)");
	curl_easy_setopt(c, CURLOPT_CAINFO,
		"/system/data/ssl/CARootCertificates.pem");
	CURLcode rc = curl_easy_perform(c);
	curl_slist_free_all(hdrs);
	curl_easy_cleanup(c);
	if (rc != CURLE_OK) return {};
	return resp;
}

void open_url_in_browser(const std::string& url) {
	// Try Haiku's be_roster first.
	const char* argv[] = { url.c_str(), nullptr };
	if (be_roster->Launch("text/html", 1, const_cast<char**>(argv)) == B_OK)
		return;
	// Fallback: `open` command.
	std::string cmd = "open '" + url + "' 2>/dev/null &";
	std::system(cmd.c_str());
}

}  // namespace

// ---- PKCE helpers ----

std::string GenerateCodeVerifier() {
	unsigned char buf[48];
	RAND_bytes(buf, sizeof(buf));
	return base64url_encode(buf, sizeof(buf));  // 64 chars
}

std::string GenerateCodeChallenge(const std::string& verifier) {
	unsigned char hash[32];
	unsigned int hlen = 0;
	EVP_Digest(verifier.data(), verifier.size(),
	           hash, &hlen, EVP_sha256(), nullptr);
	return base64url_encode(hash, hlen);
}

std::string GenerateState() {
	unsigned char buf[16];
	RAND_bytes(buf, sizeof(buf));
	std::string out;
	for (int i = 0; i < 16; ++i) {
		char hex[3];
		std::snprintf(hex, sizeof(hex), "%02x", buf[i]);
		out += hex;
	}
	return out;
}

// ---- Token storage ----

std::string OAuthTokensPath() {
	return home_dir() + "/config/settings/pluto/oauth-tokens";
}

bool SaveOAuthTokens(const OAuthTokens& tokens) {
	std::string path = OAuthTokensPath();
	ScopedFileLock lock(path + ".lock");
	if (!lock.locked()) return false;

	std::string tmp = path + ".tmp." + std::to_string(getpid());
	// Create with restricted permissions from the start (no TOCTOU window).
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
		tokens.access_token  = v.str("access_token");
		tokens.refresh_token = v.str("refresh_token");
		const json::Value* ea = v.find("expires_at");
		if (ea && ea->is_num())
			tokens.expires_at = static_cast<std::time_t>(ea->as_num());
	} catch (const std::exception&) {}
	return tokens;
}

void DeleteOAuthTokens() {
	std::remove(OAuthTokensPath().c_str());
}

// ---- URL helpers ----

std::string ExtractQueryParam(const std::string& url,
                                const std::string& key) {
	size_t qmark = url.find('?');
	if (qmark == std::string::npos) return {};
	std::string query = url.substr(qmark + 1);
	std::string needle = key + "=";
	size_t pos = 0;
	while (pos < query.size()) {
		if (query.compare(pos, needle.size(), needle) == 0) {
			size_t vstart = pos + needle.size();
			size_t vend = query.find('&', vstart);
			if (vend == std::string::npos) vend = query.size();
			return query.substr(vstart, vend - vstart);
		}
		size_t amp = query.find('&', pos);
		if (amp == std::string::npos) break;
		pos = amp + 1;
	}
	return {};
}

// ---- Login flow ----

bool OAuthLogin() {
	std::string verifier  = GenerateCodeVerifier();
	std::string challenge = GenerateCodeChallenge(verifier);
	std::string state     = GenerateState();

	// Bind a local socket on a random port.
	int srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv < 0) {
		std::fprintf(stderr, "oauth: cannot create socket: %s\n",
			strerror(errno));
		return false;
	}
	int opt = 1;
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = 0;  // OS picks a free port
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		std::fprintf(stderr, "oauth: bind: %s\n", strerror(errno));
		close(srv);
		return false;
	}
	listen(srv, 1);

	// Read the assigned port.
	socklen_t alen = sizeof(addr);
	getsockname(srv, (struct sockaddr*)&addr, &alen);
	int port = ntohs(addr.sin_port);

	std::string redirect_uri = "http://localhost:"
		+ std::to_string(port) + "/callback";

	// Build the authorization URL.
	std::string auth_url = std::string(kAuthUrl)
		+ "?client_id=" + kClientId
		+ "&response_type=code"
		+ "&redirect_uri=" + url_encode(redirect_uri)
		+ "&scope=" + url_encode(kScope)
		+ "&state=" + state
		+ "&code_challenge=" + challenge
		+ "&code_challenge_method=S256"
		+ "&code=true";

	std::fprintf(stderr, "Opening browser for Claude login...\n");
	std::fprintf(stderr, "  → %s\n", auth_url.c_str());
	open_url_in_browser(auth_url);
	std::fprintf(stderr, "Waiting for callback (120s timeout)...\n");

	// Set socket timeout to 120s.
	struct timeval tv = { 120, 0 };
	setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	int client = accept(srv, nullptr, nullptr);
	close(srv);
	if (client < 0) {
		std::fprintf(stderr, "oauth: timeout waiting for browser callback\n");
		return false;
	}

	// Read the HTTP request. Loop until we see the end of the HTTP
	// header (\r\n\r\n) or the buffer is full — a single read() may
	// return a partial result on slow connections.
	char buf[4096] = {};
	int total = 0;
	while (total < (int)sizeof(buf) - 1) {
		int n = read(client, buf + total, sizeof(buf) - 1 - total);
		if (n <= 0) break;
		total += n;
		buf[total] = '\0';
		// We only need the request line; stop once headers are complete.
		if (std::strstr(buf, "\r\n\r\n") || std::strstr(buf, "\n\n"))
			break;
	}
	if (total <= 0) {
		close(client);
		std::fprintf(stderr, "oauth: empty callback request\n");
		return false;
	}
	int n = total;

	// Send a response page before processing.
	const char* response_html =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Connection: close\r\n"
		"\r\n"
		"<!DOCTYPE html><html><body style='font-family:sans-serif;"
		"text-align:center;padding:60px'>"
		"<h2>Login successful!</h2>"
		"<p>You can close this tab and return to pluto.</p>"
		"</body></html>";
	write(client, response_html, strlen(response_html));
	close(client);

	// Parse the GET line: "GET /callback?code=...&state=... HTTP/1.1"
	std::string request(buf, n);
	size_t get_end = request.find(" HTTP/");
	if (get_end == std::string::npos) {
		std::fprintf(stderr, "oauth: malformed callback request\n");
		return false;
	}
	std::string get_path = request.substr(4, get_end - 4);  // skip "GET "

	std::string code = ExtractQueryParam(get_path, "code");
	std::string cb_state = ExtractQueryParam(get_path, "state");

	if (code.empty()) {
		std::fprintf(stderr, "oauth: no authorization code in callback\n");
		std::string error = ExtractQueryParam(get_path, "error");
		if (!error.empty())
			std::fprintf(stderr, "  error: %s\n", error.c_str());
		return false;
	}
	if (cb_state != state) {
		std::fprintf(stderr, "oauth: state mismatch (CSRF protection)\n");
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
		if (at.empty()) {
			std::string err = v.str("error");
			std::fprintf(stderr, "oauth: token exchange error: %s\n",
				err.empty() ? resp.c_str() : err.c_str());
			return false;
		}
		const json::Value* ei = v.find("expires_in");
		int expires_in = (ei && ei->is_num()) ? (int)ei->as_num() : 3600;

		OAuthTokens tokens;
		tokens.access_token  = at;
		tokens.refresh_token = rt;
		tokens.expires_at    = std::time(nullptr) + expires_in;

		if (!SaveOAuthTokens(tokens)) {
			std::fprintf(stderr, "oauth: failed to save tokens\n");
			return false;
		}
		return true;
	} catch (const std::exception&) {
		std::fprintf(stderr, "oauth: failed to parse token response\n");
		return false;
	}
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
			"  Try 'pluto login' again.\n");
		return {};
	}

	try {
		json::Value v = json::Value::parse(resp);
		std::string at = v.str("access_token");
		if (at.empty()) {
			std::fprintf(stderr, "oauth: refresh error: %s\n"
				"  Try 'pluto login' again.\n", resp.c_str());
			return {};
		}
		std::string rt = v.str("refresh_token");
		const json::Value* ei = v.find("expires_in");
		int expires_in = (ei && ei->is_num()) ? (int)ei->as_num() : 3600;

		tokens.access_token = at;
		if (!rt.empty()) tokens.refresh_token = rt;
		tokens.expires_at = std::time(nullptr) + expires_in;
		SaveOAuthTokens(tokens);
		return tokens.access_token;
	} catch (const std::exception&) {
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
	} catch (const std::exception&) {}
	return prof;
}

}  // namespace pluto
