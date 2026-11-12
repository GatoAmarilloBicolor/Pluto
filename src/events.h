#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <variant>

#include "config.h"
#include "provider/Provider.h"
#include "tools/Tool.h"

namespace pluto {

enum class EventMode {
    EMIT,       // Fire and forget, all listeners run
    WATERFALL,  // Each listener can short-circuit by returning a decision
    PARALLEL,   // All listeners run concurrently, results collected
    SERIAL,     // Listeners run in order, each must complete before next
    BAIL        // Stop on first non-null result
};

template <typename... Args>
class Event {
public:
    using Listener = std::function<void(Args...)>;
    using WaterfallListener = std::function<bool(Args...)>; // return true to continue, false to stop

    struct Registration {
        size_t id;
        EventMode mode;
        std::shared_ptr<void> listener; // type-erased
    };

    Event() : next_id_(1) {}

    // Register a listener for EMIT/PARALLEL/SERIAL/BAIL mode
    size_t on(Listener listener, EventMode mode = EventMode::EMIT) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t id = next_id_++;
        listeners_.push_back({id, mode, std::make_shared<Listener>(std::move(listener))});
        return id;
    }

    // Register a waterfall listener (can short-circuit)
    size_t on_waterfall(WaterfallListener listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t id = next_id_++;
        listeners_.push_back({id, EventMode::WATERFALL, std::make_shared<WaterfallListener>(std::move(listener))});
        return id;
    }

    void off(size_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.erase(
            std::remove_if(listeners_.begin(), listeners_.end(),
                          [id](const Registration& r) { return r.id == id; }),
            listeners_.end());
    }

    void off_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.clear();
        next_id_ = 1;
    }

    // Emit to all listeners (EMIT mode)
    void emit(Args... args) const {
        std::vector<Listener> to_call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& reg : listeners_) {
                if (reg.mode == EventMode::EMIT) {
                    to_call.push_back(*static_cast<Listener*>(reg.listener.get()));
                }
            }
        }
        for (auto& fn : to_call) {
            fn(args...);
        }
    }

    // Waterfall: each listener can return false to stop the chain
    bool waterfall(Args... args) const {
        std::vector<WaterfallListener> to_call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& reg : listeners_) {
                if (reg.mode == EventMode::WATERFALL) {
                    to_call.push_back(*static_cast<WaterfallListener*>(reg.listener.get()));
                }
            }
        }
        for (auto& fn : to_call) {
            if (!fn(args...)) return false; // short-circuited
        }
        return true; // all listeners continued
    }

    // Parallel: all listeners run, results collected
    template <typename Result>
    std::vector<Result> parallel(Args... args, std::function<Result(Args...)> mapper) const {
        std::vector<Listener> to_call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& reg : listeners_) {
                if (reg.mode == EventMode::PARALLEL) {
                    to_call.push_back(*static_cast<Listener*>(reg.listener.get()));
                }
            }
        }
        std::vector<Result> results;
        results.reserve(to_call.size());
        for (auto& fn : to_call) {
            results.push_back(mapper(args...));
        }
        return results;
    }

    // Serial: listeners run in order
    void serial(Args... args) const {
        std::vector<Listener> to_call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& reg : listeners_) {
                if (reg.mode == EventMode::SERIAL) {
                    to_call.push_back(*static_cast<Listener*>(reg.listener.get()));
                }
            }
        }
        for (auto& fn : to_call) {
            fn(args...);
        }
    }

    // Bail: stop on first non-null result
    template <typename Result>
    std::optional<Result> bail(Args... args, std::function<std::optional<Result>(Args...)> mapper) const {
        std::vector<std::function<std::optional<Result>(Args...)>> to_call;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& reg : listeners_) {
                if (reg.mode == EventMode::BAIL) {
                    to_call.push_back(*static_cast<std::function<std::optional<Result>(Args...)>*>(reg.listener.get()));
                }
            }
        }
        for (auto& fn : to_call) {
            if (auto result = fn(args...); result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    size_t listener_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return listeners_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<Registration> listeners_;
    std::atomic<size_t> next_id_;
};

// Typed event keys for the harness
struct ToolPreExecuteEvent {
    const Tool* tool;
    const std::string* input_json;
    std::string* preview; // can be modified
    bool* allow;          // set to false to deny
    std::string* deny_reason; // optional reason when denied
};

struct ToolExecuteEvent {
    const Tool* tool;
    const std::string* input_json;
    ToolResult* result; // output
    bool* skip_execution; // set to true to skip default execution
};

struct ToolPostExecuteEvent {
    const Tool* tool;
    const std::string* input_json;
    ToolResult* result; // can be modified
};

struct ToolResultEvent {
    const Tool* tool;
    const std::string* input_json;
    const ToolResult* result; // immutable final result
};

struct SessionEvent {
    enum class Type {
        TURN_START,
        TURN_END,
        TOOL_START,
        TOOL_END,
        TOOL_RESULT,
        MESSAGE,
        ERROR,
        COMPACTION,
        USAGE
    };
    Type type;
    std::string data; // JSON or formatted string
};

struct AgentTurnEvent {
    std::vector<Message>* messages; // mutable during turn
    const Config* config;
    std::atomic<bool>* cancel;
};

// Event bus - aggregate of all event types for plugin context
struct EventBus {
    Event<ToolPreExecuteEvent> tool_pre_execute;
    Event<ToolExecuteEvent> tool_execute;
    Event<ToolPostExecuteEvent> tool_post_execute;
    Event<ToolResultEvent> tool_result;
    Event<SessionEvent> session;
    Event<AgentTurnEvent> agent_turn;

    // Generic named events for DSH contexts
    using GenericCallback = std::function<void(const void*)>;
    mutable std::mutex generic_mutex_;
    std::unordered_map<std::string, std::vector<std::pair<size_t, GenericCallback>>> generic_events_;
    std::atomic<size_t> generic_next_id_{1};

    template <typename T>
    size_t subscribe(const std::string& name, std::function<void(const T&)> cb) {
        std::lock_guard<std::mutex> lock(generic_mutex_);
        size_t id = generic_next_id_++;
        generic_events_[name].push_back({id, [cb = std::move(cb)](const void* data) {
            cb(*static_cast<const T*>(data));
        }});
        return id;
    }

    void unsubscribe(const std::string& name, size_t id) {
        std::lock_guard<std::mutex> lock(generic_mutex_);
        auto it = generic_events_.find(name);
        if (it != generic_events_.end()) {
            it->second.erase(
                std::remove_if(it->second.begin(), it->second.end(),
                              [id](const auto& entry) { return entry.first == id; }),
                it->second.end());
        }
    }

    template <typename T>
    void publish(const std::string& name, const T& event) {
        std::lock_guard<std::mutex> lock(generic_mutex_);
        auto it = generic_events_.find(name);
        if (it != generic_events_.end()) {
            for (auto& entry : it->second) {
                entry.second(&event);
            }
        }
    }
};

} // namespace pluto