#include "HttpTransport.h"
#include "../version.h"
#include "curl_security.h"

#include <curl/curl.h>

#include <string>
#include <utility>

namespace pluto {

namespace {

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto* out = static_cast<std::string*>(userdata);
	out->append(ptr, size * nmemb);
	return size * nmemb;
}

}  // namespace

HttpTransport::HttpTransport(std::string url, std::map<std::string, std::string> headers)
	: fUrl(std::move(url)), fHeaders(std::move(headers)) {}

std::string HttpTransport::post(const std::string& body, std::string& err) {
	CURL* curl = curl_easy_init();
	if (!curl) { err = "curl_easy_init failed"; return {}; }
	std::string out;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "content-type: application/json");
	headers = curl_slist_append(headers, "accept: application/json");
	for (const auto& [k, v] : fHeaders) {
		std::string line = k + ": " + v;
		headers = curl_slist_append(headers, line.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, fUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
	ApplyCurlSecurityWithPinning(curl, fUrl);

	CURLcode rc = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK) { err = std::string("curl: ") + curl_easy_strerror(rc); return {}; }
	if (http_code != 200 && http_code != 202) {
		err = "HTTP " + std::to_string(http_code) + ": " + out;
		return {};
	}
	return out;
}

std::string HttpTransport::request(const std::string& body, std::string& err) {
	return post(body, err);
}

void HttpTransport::notify(const std::string& body) {
	std::string err;
	(void)post(body, err);
}

}  // namespace pluto
