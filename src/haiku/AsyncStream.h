// AsyncStreamManager — concurrent streaming provider requests using libcurl multi.
// Allows multiple LLM streams to run concurrently without blocking the main
// thread. Each request gets its own easy handle managed by a multi stack.

#pragma once

#include <curl/curl.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pluto {

struct AsyncStreamEvent {
	int64_t      req_id = 0;
	std::string  text;
	std::string  error;
	bool         done = false;
	int          input_tokens = 0;
	int          output_tokens = 0;
};

using AsyncStreamCallback = std::function<void(const AsyncStreamEvent&)>;

// Submit a streaming POST request. Returns a request id.
// The callback is invoked on the caller's thread when Perform() is called.
class AsyncStreamManager {
public:
	AsyncStreamManager();
	~AsyncStreamManager();

	// Submit a streaming POST. Returns req_id (>0 on success).
	int64_t PostStream(const std::string& url,
	                   const std::string& body,
	                   std::vector<std::string> headers,
	                   AsyncStreamCallback cb,
	                   std::atomic<bool>* cancel = nullptr);

	// Pump the multi stack. Call regularly (e.g. from a loop or thread).
	// Returns number of active handles.
	int Perform(int timeout_ms = 100);

	// Wait until all handles complete (or cancel is set).
	void Drain();

	int ActiveCount() const;

private:
	struct HandleData {
		CURL*          easy = nullptr;
		int64_t        req_id = 0;
		AsyncStreamCallback cb;
		std::string    write_buffer;
		std::string    event_data;
		std::atomic<bool>* cancel = nullptr;
		bool done = false;
	};

	CURLM* fMulti = nullptr;
	mutable std::mutex fMutex;
	std::map<int64_t, HandleData> fHandles;
	std::atomic<int64_t> fNextId{1};
	bool fRunning = true;

	static size_t WriteCb(char* p, size_t sz, size_t nm, void* ud);
	static int XferInfoCb(void* userdata,
	                      curl_off_t dltotal, curl_off_t dlnow,
	                      curl_off_t ultotal, curl_off_t ulnow);
	void ProcessSSE(HandleData& hd);
};

}  // namespace pluto
