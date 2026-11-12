#pragma once

#include "curl_security.h"
#include <curl/curl.h>
#include <map>
#include <string>
#include <optional>

namespace pluto {

struct HttpPostOptions {
    std::string url;
    std::string body;
    std::map<std::string, std::string> headers;
    long timeout_sec = 60;
    long connect_timeout_sec = 5;
    std::optional<std::string> resolve_entry;  // "host:port:ip" for DNS pinning
    std::string user_agent = PLUTO_USER_AGENT;
};

struct HttpPostResult {
    std::string body;
    long http_code = 0;
    std::string error;
    bool success = false;
};

// Core HTTP POST with unified error handling, security, and cleanup
// Returns result with success flag, response body, HTTP code, and error message
HttpPostResult http_post(const HttpPostOptions& opts);

}  // namespace pluto