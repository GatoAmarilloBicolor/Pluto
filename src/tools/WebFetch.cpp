// Tool: web_fetch — fetch a URL and return its content as text.
// HTML pages are stripped of tags to produce readable plain text.

#include "WebFetch.h"
#include "Tool.h"
#include "../json.h"

#include <curl/curl.h>

#include <cctype>
#include <cstdio>
#include <string>

namespace pluto {

namespace {

constexpr size_t kMaxResponseBytes = 512 * 1024;  // 512 KB download cap
constexpr size_t kMaxOutputChars   = 32 * 1024;   // 32 KB returned to model

size_t collect_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
	auto* buf = static_cast<std::string*>(ud);
	size_t bytes = size * nmemb;
	if (buf->size() + bytes > kMaxResponseBytes) {
		// Accept partial to fill up to cap, then stop.
		size_t room = kMaxResponseBytes - buf->size();
		buf->append(ptr, room);
		return 0;  // abort transfer
	}
	buf->append(ptr, bytes);
	return bytes;
}

// Minimal HTML → text: strip tags, decode common entities, collapse whitespace.
std::string strip_html(const std::string& html) {
	std::string out;
	out.reserve(html.size() / 2);
	bool in_tag = false;
	bool in_script = false;
	bool in_style = false;
	size_t i = 0;

	while (i < html.size()) {
		char c = html[i];
		if (c == '<') {
			in_tag = true;
			// Detect <script and <style to skip their body.
			// Compare in-place to avoid substr allocations.
			size_t remain = html.size() - i;
			auto icmp = [&](size_t pos, const char* s, size_t len) -> bool {
				if (remain < len) return false;
				for (size_t j = 0; j < len; ++j)
					if (std::tolower((unsigned char)html[pos + j]) != s[j])
						return false;
				return true;
			};
			if (icmp(i, "<script", 7)) in_script = true;
			if (icmp(i, "</script", 8)) in_script = false;
			if (icmp(i, "<style", 6)) in_style = true;
			if (icmp(i, "</style", 7)) in_style = false;
			++i;
			continue;
		}
		if (c == '>') {
			in_tag = false;
			++i;
			continue;
		}
		if (in_tag || in_script || in_style) { ++i; continue; }

		// Decode common HTML entities.
		if (c == '&') {
			size_t semi = html.find(';', i + 1);
			if (semi != std::string::npos && semi - i < 10) {
				std::string ent = html.substr(i + 1, semi - i - 1);
				++i;  // will advance past ';' below
				if (ent == "amp") { out += '&'; i = semi + 1; continue; }
				if (ent == "lt")  { out += '<'; i = semi + 1; continue; }
				if (ent == "gt")  { out += '>'; i = semi + 1; continue; }
				if (ent == "quot") { out += '"'; i = semi + 1; continue; }
				if (ent == "apos") { out += '\''; i = semi + 1; continue; }
				if (ent == "nbsp") { out += ' '; i = semi + 1; continue; }
				if (ent.size() > 1 && ent[0] == '#') {
					// &#NNN; or &#xHHH;
					i = semi + 1;
					out += ' ';
					continue;
				}
				// Unknown entity — output as-is.
				out += '&';
				continue;
			}
		}

		out += c;
		++i;
	}

	// Collapse runs of whitespace into single spaces/newlines.
	std::string collapsed;
	collapsed.reserve(out.size());
	int blank_lines = 0;
	bool prev_space = false;
	for (char ch : out) {
		if (ch == '\n') {
			++blank_lines;
			if (blank_lines <= 2) collapsed += '\n';
			prev_space = true;
		} else if (ch == ' ' || ch == '\t' || ch == '\r') {
			if (!prev_space) collapsed += ' ';
			prev_space = true;
		} else {
			collapsed += ch;
			prev_space = false;
			blank_lines = 0;
		}
	}
	return collapsed;
}

class WebFetchTool : public Tool {
public:
	std::string name() const override { return "web_fetch"; }
	std::string description() const override {
		return "Fetch a URL and return its content as text. HTML pages are "
		       "automatically stripped of tags. Useful for reading documentation, "
		       "GitHub issues, API references, or any web page. "
		       "Maximum output: 32 KB of text.";
	}
	std::string input_schema_json() const override {
		return R"json({"type":"object","properties":{"url":{"type":"string","description":"The URL to fetch"},"raw":{"type":"boolean","description":"If true, return raw content without HTML stripping. Default: false"}},"required":["url"]})json";
	}
	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			return "web_fetch " + v.str("url");
		} catch (...) { return Tool::preview(input_json); }
	}
	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		std::string url;
		bool raw = false;
		try {
			auto v = json::Value::parse(input_json);
			url = v.str("url");
			const json::Value* rv = v.find("raw");
			if (rv && rv->is_bool()) raw = rv->as_bool();
		} catch (const std::exception& e) {
			r.is_error = true;
			r.output = std::string("args: ") + e.what();
			return r;
		}
		if (url.empty()) {
			r.is_error = true;
			r.output = "url required";
			return r;
		}
		// Only allow http/https schemes.
		if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
			r.is_error = true;
			r.output = "only http:// and https:// URLs are supported";
			return r;
		}
		// Block requests to localhost and private networks (SSRF prevention).
		{
			// Extract host from URL: skip scheme, find host before : or /
			size_t hostStart = url.find("://");
			if (hostStart != std::string::npos) hostStart += 3;
			else hostStart = 0;
			size_t hostEnd = url.find_first_of(":/?#", hostStart);
			std::string host = url.substr(hostStart,
				hostEnd == std::string::npos ? hostEnd : hostEnd - hostStart);
			if (host == "localhost" || host == "127.0.0.1" || host == "::1"
			    || host == "0.0.0.0" || host.rfind("10.", 0) == 0
			    || host.rfind("192.168.", 0) == 0
			    || host.rfind("169.254.", 0) == 0
			    || host.rfind("172.16.", 0) == 0
			    || host.rfind("172.17.", 0) == 0
			    || host.rfind("172.18.", 0) == 0
			    || host.rfind("172.19.", 0) == 0
			    || host.rfind("172.2", 0) == 0
			    || host.rfind("172.3", 0) == 0) {
				r.is_error = true;
				r.output = "requests to local/private network addresses are blocked";
				return r;
			}
		}

		CURL* c = curl_easy_init();
		if (!c) {
			r.is_error = true;
			r.output = "curl init failed";
			return r;
		}

		std::string body;
		std::string content_type;
		curl_easy_setopt(c, CURLOPT_URL, url.c_str());
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, collect_cb);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
		curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
		curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
		curl_easy_setopt(c, CURLOPT_USERAGENT,
			"Mozilla/5.0 (compatible; pluto/0.5.0; +https://haiku-os.org)");
		curl_easy_setopt(c, CURLOPT_CAINFO,
			"/system/data/ssl/CARootCertificates.pem");
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
		// Accept compressed responses.
		curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");

		CURLcode rc = curl_easy_perform(c);

		long http_code = 0;
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);

		char* ct = nullptr;
		curl_easy_getinfo(c, CURLINFO_CONTENT_TYPE, &ct);
		if (ct) content_type = ct;

		curl_easy_cleanup(c);

		if (rc != CURLE_OK && rc != CURLE_WRITE_ERROR) {
			// CURLE_WRITE_ERROR is expected when we hit the size cap.
			r.is_error = true;
			r.output = std::string("fetch failed: ") + curl_easy_strerror(rc);
			return r;
		}
		if (http_code >= 400) {
			r.is_error = true;
			r.output = "HTTP " + std::to_string(http_code);
			if (!body.empty() && body.size() < 500) r.output += ": " + body;
			return r;
		}

		// Strip HTML if content type looks like HTML and raw is not requested.
		bool is_html = !raw && (content_type.find("text/html") != std::string::npos
		    || content_type.find("application/xhtml") != std::string::npos
		    || (!content_type.empty() && body.find("<html") != std::string::npos));
		std::string text = is_html ? strip_html(body) : body;

		// Truncate to output limit.
		if (text.size() > kMaxOutputChars) {
			text.resize(kMaxOutputChars);
			text += "\n\n[truncated — " + std::to_string(body.size()) + " bytes total]";
		}

		r.output = text;
		return r;
	}
};

}  // namespace

std::unique_ptr<Tool> MakeWebFetchTool() {
	return std::make_unique<WebFetchTool>();
}

}  // namespace pluto
