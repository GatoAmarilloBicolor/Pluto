#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace pluto {

// Persistent background subscriber to an SSE (Server-Sent Events) stream.
// Opens a long-lived HTTP GET on a worker thread, parses SSE events, and
// buffers them in a thread-safe queue for draining from the main thread.
//
// Used by pluto to receive asynchronous notifications like Pippo's
// query_update events (BFS B_QUERY_UPDATE messages pushed when live queries
// match). Events are queued while the agent is idle or mid-turn and drained
// before the next user turn.
class SseSubscriber {
public:
	struct Event {
		std::string name;  // SSE event: field, or "message" if absent
		std::string data;  // SSE data: field(s), concatenated with '\n'
	};

	explicit SseSubscriber(std::string url);
	~SseSubscriber();

	bool start(std::string& err);
	void stop();
	bool running() const { return fRunning.load(); }
	const std::string& url() const { return fUrl; }

	// Drains all pending events, clearing the queue. Thread-safe.
	std::vector<Event> drain();

	// Used by the libcurl callbacks (public because the C callbacks live in
	// file-scope and can't access private members directly).
	void feed(const char* bytes, size_t len);
	bool should_stop() const { return fStop.load(); }

private:
	std::string fUrl;
	std::thread fThread;
	std::mutex fMutex;
	std::deque<Event> fQueue;
	std::atomic<bool> fStop{false};
	std::atomic<bool> fRunning{false};

	// Parser state, worker-thread only.
	std::string fBuffer;
	std::string fEventName;
	std::string fEventData;

	void run();
	void dispatch();
	void push(Event ev);
};

}  // namespace pluto
