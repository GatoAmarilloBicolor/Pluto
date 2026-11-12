#include "SseSubscriber.h"
#include "../version.h"
#include "curl_security.h"

#include <curl/curl.h>

#include <cstdio>
#include <utility>

namespace pluto {

namespace {

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto* self = static_cast<SseSubscriber*>(userdata);
	self->feed(ptr, size * nmemb);
	return size * nmemb;
}

int xferinfo_cb(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
	auto* self = static_cast<SseSubscriber*>(userdata);
	return self->should_stop() ? 1 : 0;
}

}  // namespace

SseSubscriber::SseSubscriber(std::string url) : fUrl(std::move(url)) {}

SseSubscriber::~SseSubscriber() { stop(); }

bool SseSubscriber::start(std::string& err) {
	if (fRunning.load()) { err = "already running"; return false; }
	fStop.store(false);
	fRunning.store(true);
	try {
		fThread = std::thread([this] { run(); });
	} catch (const std::exception& e) {
		fRunning.store(false);
		err = e.what();
		return false;
	}
	return true;
}

void SseSubscriber::stop() {
	fStop.store(true);
	if (fThread.joinable()) fThread.join();
	fRunning.store(false);
}

std::vector<SseSubscriber::Event> SseSubscriber::drain() {
	std::lock_guard<std::mutex> lock(fMutex);
	std::vector<Event> out(
		std::make_move_iterator(fQueue.begin()),
		std::make_move_iterator(fQueue.end()));
	fQueue.clear();
	return out;
}

void SseSubscriber::push(Event ev) {
	std::lock_guard<std::mutex> lock(fMutex);
	// Drop oldest events past a generous cap to bound memory growth if
	// drain() is never called.
	if (fQueue.size() >= 1000) fQueue.pop_front();
	fQueue.push_back(std::move(ev));
}

void SseSubscriber::feed(const char* bytes, size_t len) {
	fBuffer.append(bytes, len);
	for (;;) {
		size_t nl = fBuffer.find('\n');
		if (nl == std::string::npos) break;
		std::string line = fBuffer.substr(0, nl);
		fBuffer.erase(0, nl + 1);
		if (!line.empty() && line.back() == '\r') line.pop_back();

		if (line.empty()) { dispatch(); continue; }
		if (line[0] == ':') continue;  // SSE comment

		if (line.rfind("event:", 0) == 0) {
			size_t s = 6;
			if (s < line.size() && line[s] == ' ') ++s;
			fEventName.assign(line, s, std::string::npos);
		} else if (line.rfind("data:", 0) == 0) {
			size_t s = 5;
			if (s < line.size() && line[s] == ' ') ++s;
			if (!fEventData.empty()) fEventData += '\n';
			fEventData.append(line, s, std::string::npos);
		}
		// id:/retry: ignored
	}
}

void SseSubscriber::dispatch() {
	if (fEventName.empty() && fEventData.empty()) return;
	Event ev;
	ev.name = fEventName.empty() ? "message" : fEventName;
	ev.data = fEventData;
	// Cap individual event payload to keep context/buffer bounded.
	const size_t kMax = 8 * 1024;
	if (ev.data.size() > kMax) {
		ev.data.resize(kMax);
		ev.data += "... [truncated]";
	}
	push(std::move(ev));
	fEventName.clear();
	fEventData.clear();
}

void SseSubscriber::run() {
	int reconnect_attempt = 0;
	while (!fStop.load()) {
		CURL* curl = curl_easy_init();
		if (!curl) {
			fRunning.store(false);
			return;
		}

		struct curl_slist* headers = nullptr;
		headers = curl_slist_append(headers, "accept: text/event-stream");
		headers = curl_slist_append(headers, "cache-control: no-cache");

		curl_easy_setopt(curl, CURLOPT_URL, fUrl.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo_cb);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);        // no overall deadline
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
		ApplyCurlSecurityWithPinning(curl, fUrl);

		CURLcode rc = curl_easy_perform(curl);

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (fStop.load()) {
			fRunning.store(false);
			return;
		}

		if (rc != CURLE_OK && rc != CURLE_ABORTED_BY_CALLBACK) {
			++reconnect_attempt;
			if (reconnect_attempt > 10) {
				std::fprintf(stderr, "[sse] %s: %s, giving up reconnect\n",
					fUrl.c_str(), curl_easy_strerror(rc));
				break;
			}
			long delay = 1000L * (1L << (reconnect_attempt - 1));
			if (delay > 30000L) delay = 30000L;
			std::fprintf(stderr, "[sse] %s: %s, reconnecting in %ldms\n",
				fUrl.c_str(), curl_easy_strerror(rc), delay);
			std::this_thread::sleep_for(std::chrono::milliseconds(delay));
			continue;
		}

		// Clean disconnect (not an error), but we may want to reconnect
		// if not explicitly stopped
		if (!fStop.load()) {
			++reconnect_attempt;
			if (reconnect_attempt > 10) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			continue;
		}

		break;
	}

	fRunning.store(false);
}

}  // namespace pluto
