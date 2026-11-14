#include "HttpPostHelper.h"
#include "curl_security.h"
#include <curl/curl.h>

namespace pluto {

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

HttpPostResult http_post(const HttpPostOptions& opts) {
    HttpPostResult res;

    if (is_blocked_url(opts.url)) {
        res.error = "URL blocked by security policy: " + opts.url;
        return res;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        res.error = "Failed to initialize CURL";
        return res;
    }

    struct curl_slist* headers_list = nullptr;
    for (const auto& [key, value] : opts.headers) {
        std::string header = key + ": " + value;
        headers_list = curl_slist_append(headers_list, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, opts.url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, opts.body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, opts.body.size());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, opts.timeout_sec);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, opts.connect_timeout_sec);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, opts.user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);

    if (opts.resolve_entry) {
        struct curl_slist* resolve_list = curl_slist_append(nullptr, opts.resolve_entry->c_str());
        curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);
    }

    configure_secure_curl(curl, opts.url);

    if (headers_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_list);
    }

    CURLcode curl_res = curl_easy_perform(curl);
    if (curl_res != CURLE_OK) {
        res.error = curl_easy_strerror(curl_res);
        res.success = false;
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res.http_code);
        res.success = (res.http_code >= 200 && res.http_code < 300);
        if (!res.success && res.error.empty()) {
            res.error = "HTTP error code: " + std::to_string(res.http_code);
        }
    }

    if (headers_list) curl_slist_free_all(headers_list);
    curl_easy_cleanup(curl);
    return res;
}

}  // namespace pluto