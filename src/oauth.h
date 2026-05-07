#pragma once

#include <ctime>
#include <string>

namespace pluto {

// OAuth token storage.
struct OAuthTokens {
	std::string access_token;
	std::string refresh_token;
	std::time_t expires_at = 0;

	bool empty() const { return access_token.empty(); }
	bool expired() const { return std::time(nullptr) >= (expires_at - 300); }
};

// PKCE helpers.
std::string GenerateCodeVerifier();                       // 64 char random
std::string GenerateCodeChallenge(const std::string& v);  // SHA256 + base64url
std::string GenerateState();                               // 16 byte random hex

// Token file: ~/config/settings/pluto/oauth-tokens (chmod 600).
std::string OAuthTokensPath();
bool        SaveOAuthTokens(const OAuthTokens& tokens);
OAuthTokens LoadOAuthTokens();
void        DeleteOAuthTokens();

// Extract a query parameter value from a URL or query string.
std::string ExtractQueryParam(const std::string& url, const std::string& key);

// Full OAuth login flow: opens browser, waits for callback, exchanges code.
// Blocks until complete or timeout (120s). Returns true on success.
bool OAuthLogin();

// Ensure the access token is valid; refresh if expired.
// Returns a valid access_token or empty string on failure.
std::string OAuthEnsureValidToken(OAuthTokens& tokens);

// Fetch the user profile to verify account type.
struct OAuthProfile {
	std::string display_name;
	std::string organization_type;  // "max", "pro", etc.
};
OAuthProfile OAuthGetProfile(const std::string& access_token);

}  // namespace pluto
