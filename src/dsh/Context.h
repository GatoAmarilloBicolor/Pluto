// DSH Context — Cordis-compatible service context with effect/on semantics
// 100% Haiku API via Pluto's HaikuAbstraction layer

#pragma once

#include "../plugin.h"
#include "../events.h"
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace pluto::dsh {

using Disposable = std::function<void()>;

// Event subscription token for cleanup
struct SubscriptionToken {
    std::string event_name;
    uint64_t id;
};

// Context extends PluginContext with DSH-style effect/on API
class Context : public PluginContext {
public:
    Context() = default;
    virtual ~Context() {
        // Run all cleanup effects on destruction
        for (auto& d : disposables_) {
            if (d) d();
        }
    }

    // ctx.effect(cleanup_fn) — register cleanup to run on context disposal
    void effect(Disposable d) {
        disposables_.push_back(std::move(d));
    }

    // ctx.on(event_name, callback) — subscribe to event bus with auto-cleanup
    template<typename T>
    void on(const std::string& event_name, std::function<void(const T&)> callback) {
        auto token = event_bus()->subscribe<T>(event_name, callback);
        effect([this, event_name, token]() {
            event_bus()->unsubscribe(event_name, token);
        });
    }

    // ctx.once(event_name, callback) — one-shot subscription
    template<typename T>
    void once(const std::string& event_name, std::function<void(const T&)> callback) {
        std::function<void(const T&)> wrapper = [this, event_name, callback = std::move(callback)](const T& event) mutable {
            callback(event);
            event_bus()->unsubscribe(event_name, current_token_);
        };
        auto token = event_bus()->subscribe<T>(event_name, wrapper);
        current_token_ = token;
        effect([this, event_name, token]() {
            event_bus()->unsubscribe(event_name, token);
        });
    }

    // Emit event to all subscribers in this context (and parent contexts)
    template<typename T>
    void emit(const std::string& event_name, const T& event) {
        event_bus()->publish(event_name, event);
        // Also emit to parent context if exists
        if (auto parent_ctx = parent()) {
            auto dsh_parent = std::dynamic_pointer_cast<Context>(parent_ctx);
            if (dsh_parent) {
                dsh_parent->emit(event_name, event);
            }
        }
    }

    // Service registration with effect cleanup (DSH-style)
    template<typename T>
    void provide_service(const std::string& name, std::shared_ptr<T> instance) {
        provide<T>(name, instance);
        effect([this, name]() {
            remove<T>(name);
        });
    }

    // Factory registration with effect cleanup
    template<typename T>
    void provide_factory(const std::string& name, std::function<std::shared_ptr<T>()> factory) {
        PluginContext::provide_factory<T>(name, std::move(factory));
        effect([this, name]() {
            remove<T>(name);
        });
    }

    // Create isolated child context (for subagents, tabs, etc.)
    std::shared_ptr<Context> isolate(const std::vector<std::string>& service_names = {}) {
        auto child = std::make_shared<Context>();
        // Copy services from parent (PluginContext::isolate handles this)
        auto plugin_child = PluginContext::isolate(service_names);
        // Merge our disposables tracking into the child
        child->parent_ = plugin_child->parent_;
        child->services_ = plugin_child->services_;
        child->factories_ = plugin_child->factories_;
        return child;
    }

    // Create extended child context (inherits all parent services)
    std::shared_ptr<Context> extend() {
        auto child = std::make_shared<Context>();
        auto plugin_child = PluginContext::extend();
        child->parent_ = plugin_child->parent_;
        child->services_ = plugin_child->services_;
        child->factories_ = plugin_child->factories_;
        return child;
    }

    // Get parent context
    std::shared_ptr<Context> parent() const {
        auto p = PluginContext::parent();
        return std::dynamic_pointer_cast<Context>(p);
    }

protected:
    std::vector<Disposable> disposables_;
    uint64_t current_token_ = 0;
};

// Root context singleton for the application
class RootContext {
public:
    static Context& instance() {
        static std::unique_ptr<Context> ctx = []() {
            auto c = std::make_shared<Context>();
            c->weak_this_ = c;
            return c;
        }();
        return *ctx;
    }

private:
    RootContext() = default;
};

} // namespace pluto::dsh