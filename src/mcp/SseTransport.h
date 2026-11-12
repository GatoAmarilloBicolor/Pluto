#pragma once

#include "Transport.h"
#include "../provider/CurlUtil.h"

#include <map>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

namespace pluto {

// MCP SSE transport (2024-11-05 spec).
//
// Protocol:
//   1. start(): GET the SSE URL, read the first "endpoint" event to learn
//      the message URL for JSON-RPC POSTs.
//   2. request(): POST JSON-RPC body to the message URL, return response.
//   3. Server-initiated events arrive on the SSE stream (not yet used).
//
// Used by Pippo and other SSE-based MCP servers.
class SseTransport : public Transport {
public:
    struct Config {
        int max_reconnect_attempts = 3;
        long reconnect_base_delay_ms = 1000;
        long reconnect_max_delay_ms = 30000;
        long heartbeat_interval_ms = 30000;  // Send ping every 30s
        long request_timeout_ms = 60000;
        long connect_timeout_ms = 5000;
        long handshake_timeout_ms = 30000;
    };
    
    SseTransport(std::string sse_url,
                 std::map<std::string, std::string> headers,
                 Config config = {});
    ~SseTransport() override;

    bool start(std::string& err) override;
    void stop() override;
    std::string request(const std::string& body, std::string& err) override;
    void notify(const std::string& body) override;

    // Get current connection state
    bool is_connected() const { return fConnected.load(); }
    const std::string& message_url() const { return fMessageUrl; }
    const Config& config() const { return fConfig; }

private:
    bool do_handshake(std::string& err);
    bool do_post(const std::string& body, std::string& response, std::string& err);
    void ensure_connection(std::string& err);
    void start_heartbeat();
    void stop_heartbeat();
    static void heartbeat_thread_func(SseTransport* self);

    std::string fSseUrl;
    std::string fMessageUrl;
    std::string fPinnedIp;
    std::map<std::string, std::string> fHeaders;
    Config fConfig;
    
    std::atomic<bool> fConnected{false};
    std::atomic<bool> fStopHeartbeat{false};
    std::thread fHeartbeatThread;
    std::mutex fConnectionMutex;
    
    CircuitBreaker fCircuitBreaker;
};

}  // namespace pluto
