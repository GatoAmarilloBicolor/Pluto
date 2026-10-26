// AsyncStream.cpp — libcurl multi-based concurrent streaming.

#include "AsyncStream.h"

#include <cstdio>
#include <cstring>
#include <thread>

namespace pluto {

AsyncStreamManager::AsyncStreamManager() {
	fMulti = curl_multi_init();
	if (fMulti) {
		curl_multi_setopt(fMulti, CURLMOPT_MAXCONNECTS, 10L);
		curl_multi_setopt(fMulti, CURLMOPT_MAX_HOST_CONNECTIONS, 6L);
	}
}

AsyncStreamManager::~AsyncStreamManager() {
	fRunning = false;
	if (fMulti) {
		// Remove and cleanup all remaining handles.
		std::lock_guard<std::mutex> lk(fMutex);
		for (auto& [id, hd] : fHandles) {
			if (hd.easy && fMulti) {
				curl_multi_remove_handle(fMulti, hd.easy);
				curl_easy_cleanup(hd.easy);
			}
		}
		fHandles.clear();
		curl_multi_cleanup(fMulti);
		fMulti = nullptr;
	}
}

size_t AsyncStreamManager::WriteCb(char* p, size_t sz, size_t nm, void* ud) {
	auto* hd = static_cast<HandleData*>(ud);
	size_t total = sz * nm;
	hd->write_buffer.append(p, total);

	// Process SSE lines.
	for (;;) {
		size_t nl = hd->write_buffer.find('\n');
		if (nl == std::string::npos) break;
		std::string line = hd->write_buffer.substr(0, nl);
		hd->write_buffer.erase(0, nl + 1);
		if (!line.empty() && line.back() == '\r') line.pop_back();

		if (line.empty()) {
			if (!hd->event_data.empty()) {
				// Dispatch event.
				AsyncStreamEvent ev;
				ev.req_id = hd->req_id;
				ev.done = true;
				hd->cb(ev);
				hd->event_data.clear();
			}
			continue;
		}
		if (line.rfind("data:", 0) == 0) {
			size_t s = 5;
			if (s < line.size() && line[s] == ' ') ++s;
			if (!hd->event_data.empty()) hd->event_data += '\n';
			hd->event_data.append(line, s, std::string::npos);
		}
	}
	return total;
}

int AsyncStreamManager::XferInfoCb(void* userdata,
                                   curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                                   curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
	auto* hd = static_cast<HandleData*>(userdata);
	if (hd->cancel && hd->cancel->load(std::memory_order_relaxed)) return 1;
	return 0;
}

void AsyncStreamManager::ProcessSSE(HandleData& hd) {
	if (hd.event_data.empty()) return;
	// Try to parse as JSON to extract tokens / stop reason.
	// We just forward the raw text as a text delta.
	AsyncStreamEvent ev;
	ev.req_id = hd.req_id;
	ev.text = hd.event_data;  // raw SSE data — caller parses if needed
	ev.done = true;
	hd.cb(ev);
	hd.event_data.clear();
}

int64_t AsyncStreamManager::PostStream(const std::string& url,
                                        const std::string& body,
                                        std::vector<std::string> headers,
                                        AsyncStreamCallback cb,
                                        std::atomic<bool>* cancel) {
	if (!fMulti) return 0;

	CURL* easy = curl_easy_init();
	if (!easy) return 0;

	curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
	curl_easy_setopt(easy, CURLOPT_POST, 1L);
	curl_easy_setopt(easy, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)body.size());
	curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, WriteCb);
	curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, XferInfoCb);
	curl_easy_setopt(easy, CURLOPT_XFERINFODATA, nullptr);  // set below
	curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(easy, CURLOPT_TIMEOUT, 120L);
	curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(easy, CURLOPT_USERAGENT, "pluto/0.7.1 (Haiku)");

	struct curl_slist* hlist = nullptr;
	for (const auto& h : headers)
		hlist = curl_slist_append(hlist, h.c_str());
	curl_easy_setopt(easy, CURLOPT_HTTPHEADER, hlist);

	int64_t id = fNextId.fetch_add(1, std::memory_order_relaxed);

	HandleData hd;
	hd.easy = easy;
	hd.req_id = id;
	hd.cb = std::move(cb);
	hd.cancel = cancel;

	curl_easy_setopt(easy, CURLOPT_WRITEDATA, &fHandles[id]);
	curl_easy_setopt(easy, CURLOPT_XFERINFODATA, &fHandles[id]);

	fHandles[id] = std::move(hd);

	CURLMcode mc = curl_multi_add_handle(fMulti, easy);
	if (mc != CURLM_OK) {
		curl_easy_cleanup(easy);
		curl_slist_free_all(hlist);
		std::lock_guard<std::mutex> lk(fMutex);
		fHandles.erase(id);
		return 0;
	}
	curl_slist_free_all(hlist);

	return id;
}

int AsyncStreamManager::Perform(int timeout_ms) {
	if (!fMulti) return 0;

	int running = 0;
	CURLMcode mc = curl_multi_perform(fMulti, &running);
	if (mc != CURLM_OK && mc != CURLM_CALL_MULTI_PERFORM)
		return (int)fHandles.size();

	// Check for completed / failed handles.
	int msgs_left = 0;
	while (true) {
		CURLMsg* msg = curl_multi_info_read(fMulti, &msgs_left);
		if (!msg) break;
		if (msg->msg == CURLMSG_DONE) {
			CURL* easy = msg->easy_handle;
			CURLcode res = msg->data.result;

			std::lock_guard<std::mutex> lk(fMutex);
			auto it = fHandles.begin();
			while (it != fHandles.end()) {
				if (it->second.easy == easy) {
					int64_t id = it->first;
					auto hd = std::move(it->second);
					fHandles.erase(it++);

					AsyncStreamEvent ev;
					ev.req_id = id;
					if (res != CURLE_OK) {
						ev.error = std::string("curl: ") + curl_easy_strerror(res);
					}
					ev.done = true;
					hd.cb(ev);

					curl_multi_remove_handle(fMulti, easy);
					curl_easy_cleanup(easy);
					break;
				} else {
					++it;
				}
			}
		} else {
			break;
		}
	}

	// Wait for activity on sockets.
	struct timeval timeout = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
	int numfds = 0;
	curl_multi_wait(fMulti, nullptr, 0, timeout_ms, &numfds);

	return (int)fHandles.size();
}

void AsyncStreamManager::Drain() {
	while (ActiveCount() > 0 && fRunning)
		Perform(100);
}

int AsyncStreamManager::ActiveCount() const {
	std::lock_guard<std::mutex> lk(fMutex);
	return (int)fHandles.size();
}

}  // namespace pluto
