// Tests for OAuth PKCE helpers, token storage, and URL parsing.

#include "../src/oauth.h"

#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

// mkdtemp not available in mock headers on cross-compile
#ifdef __HAIKU__
extern "C" char* mkdtemp(char* template_path);
#endif

static int g_failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		++g_failures; \
	} \
} while (0)

#define CHECK_EQ(a, b) do { \
	if ((a) != (b)) { \
		std::fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", \
			__FILE__, __LINE__, \
			std::string(a).c_str(), std::string(b).c_str()); \
		++g_failures; \
	} \
} while (0)

static std::string make_tmpdir() {
	char tmpl[] = "/tmp/pluto-test-oauth-XXXXXX";
	const char* d = mkdtemp(tmpl);
	if (!d) { std::fprintf(stderr, "mkdtemp failed\n"); std::exit(2); }
	return std::string(d);
}

static void rm_rf(const std::string& path) {
	std::string cmd = "rm -rf '" + path + "'";
	std::system(cmd.c_str());
}

// ---- PKCE tests ----

static void test_code_verifier() {
	std::string v = pluto::GenerateCodeVerifier();
	CHECK(v.size() == 64);
	// Only valid base64url characters.
	for (char c : v) {
		CHECK((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
		      || (c >= '0' && c <= '9') || c == '-' || c == '_');
	}
	// Two calls produce different values.
	std::string v2 = pluto::GenerateCodeVerifier();
	CHECK(v != v2);
}

static void test_code_challenge() {
	std::string v = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
	std::string c = pluto::GenerateCodeChallenge(v);
	CHECK(!c.empty());
	// No padding characters.
	CHECK(c.find('=') == std::string::npos);
	CHECK(c.find('+') == std::string::npos);
	CHECK(c.find('/') == std::string::npos);
	// Deterministic: same input → same output.
	CHECK_EQ(c, pluto::GenerateCodeChallenge(v));
}

static void test_state() {
	std::string s = pluto::GenerateState();
	CHECK(s.size() == 32);
	// Only hex characters.
	for (char c : s) {
		CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
	}
	// Two calls produce different values.
	CHECK(s != pluto::GenerateState());
}

// ---- Token storage tests ----

static void test_token_roundtrip(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);
	std::string settings = dir + "/config/settings/pluto";
	std::string cmd = "mkdir -p '" + settings + "'";
	std::system(cmd.c_str());

	pluto::OAuthTokens orig;
	orig.access_token  = "at_test_12345";
	orig.refresh_token = "rt_test_67890";
	orig.expires_at    = 1700000000;

	CHECK(pluto::SaveOAuthTokens(orig));

	pluto::OAuthTokens loaded = pluto::LoadOAuthTokens();
	CHECK_EQ(loaded.access_token, "at_test_12345");
	CHECK_EQ(loaded.refresh_token, "rt_test_67890");
	CHECK(loaded.expires_at == 1700000000);
}

static void test_token_missing(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);
	// Remove any existing file.
	std::remove(pluto::OAuthTokensPath().c_str());

	pluto::OAuthTokens tokens = pluto::LoadOAuthTokens();
	CHECK(tokens.empty());
}

static void test_token_delete(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);
	pluto::OAuthTokens t;
	t.access_token = "temp";
	t.refresh_token = "temp";
	t.expires_at = 999;
	pluto::SaveOAuthTokens(t);

	pluto::DeleteOAuthTokens();
	pluto::OAuthTokens loaded = pluto::LoadOAuthTokens();
	CHECK(loaded.empty());
}

static void test_token_permissions(const std::string& dir) {
	setenv("HOME", dir.c_str(), 1);
	pluto::OAuthTokens t;
	t.access_token = "secret";
	t.refresh_token = "secret";
	t.expires_at = 999;
	pluto::SaveOAuthTokens(t);

	struct stat st;
	CHECK(stat(pluto::OAuthTokensPath().c_str(), &st) == 0);
	CHECK((st.st_mode & 0777) == 0600);
}

// ---- URL query param tests ----

static void test_ExtractQueryParam() {
	std::string url = "/callback?code=abc123&state=xyz789";
	CHECK_EQ(pluto::ExtractQueryParam(url, "code"), "abc123");
	CHECK_EQ(pluto::ExtractQueryParam(url, "state"), "xyz789");
	CHECK_EQ(pluto::ExtractQueryParam(url, "missing"), "");
}

static void test_extract_single_param() {
	std::string url = "/callback?code=onlyone";
	CHECK_EQ(pluto::ExtractQueryParam(url, "code"), "onlyone");
	CHECK_EQ(pluto::ExtractQueryParam(url, "state"), "");
}

static void test_extract_no_query() {
	CHECK_EQ(pluto::ExtractQueryParam("/callback", "code"), "");
}

// ---- Token expiry tests ----

static void test_token_expired() {
	pluto::OAuthTokens t;
	t.access_token = "test";
	t.expires_at = std::time(nullptr) - 100;  // in the past
	CHECK(t.expired());

	t.expires_at = std::time(nullptr) + 100;  // within 5min margin
	CHECK(t.expired());  // 100 < 300 margin

	t.expires_at = std::time(nullptr) + 600;  // well in the future
	CHECK(!t.expired());
}

int main() {
	std::string tmp = make_tmpdir();
	std::string orig_home = std::getenv("HOME") ? std::getenv("HOME") : "/boot/home";

	test_code_verifier();
	test_code_challenge();
	test_state();
	test_token_roundtrip(tmp);
	test_token_missing(tmp);
	test_token_delete(tmp);
	test_token_permissions(tmp);
	test_ExtractQueryParam();
	test_extract_single_param();
	test_extract_no_query();
	test_token_expired();

	setenv("HOME", orig_home.c_str(), 1);
	rm_rf(tmp);

	if (g_failures == 0) {
		std::fprintf(stderr, "test_oauth: OK (11 tests)\n");
		return 0;
	}
	std::fprintf(stderr, "test_oauth: %d failure(s)\n", g_failures);
	return 1;
}
