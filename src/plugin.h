#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <typeindex>
#include <typeinfo>

#include "events.h"
#include "tools/Tool.h"
#include "provider/Provider.h"
#include "storage/SessionStore.h"

namespace pluto {

// Capability Definition - the interface/contract for a capability
template <typename T>
class CapabilityDef {
public:
    virtual ~CapabilityDef() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
};

// Provider - an implementation of a capability
template <typename T>
class CapabilityProvider {
public:
    virtual ~CapabilityProvider() = default;
    virtual std::shared_ptr<T> create() const = 0;
    virtual std::string provider_name() const = 0;
};

// Consumer - declares dependency on a capability
template <typename T>
class CapabilityConsumer {
public:
    virtual ~CapabilityConsumer() = default;
    virtual void inject(std::shared_ptr<T> provider) = 0;
    virtual std::string capability_name() const = 0;
};

// Service key for the registry - stable identifier for a capability
class ServiceKey {
public:
    ServiceKey() = delete;
    ServiceKey(const ::std::string& n, const ::std::type_index& t) : name_(n), type_(t) {}
    
    bool operator==(const ServiceKey& other) const {
        return name_ == other.name_ && type_ == other.type_;
    }
    
    size_t hash() const {
        return ::std::hash<::std::string>()(name_) ^ ::std::hash<size_t>()(type_.hash_code());
    }
    
    ::std::string name() const { return name_; }
    const ::std::type_index& type() const { return type_; }
    
private:
    ::std::string name_;
    ::std::type_index type_;
};

// Hash specialization for ServiceKey (must be in global namespace std)
} // namespace pluto

namespace std {
    template<>
    struct hash<pluto::ServiceKey> {
        size_t operator()(const pluto::ServiceKey& k) const noexcept {
            return k.hash();
        }
    };
}

namespace pluto {

// Plugin context - the shared context where services are registered and resolved
class PluginContext {
public:
    PluginContext() = default;
    virtual ~PluginContext() = default;
    
    // Provide a service implementation
    template <typename T>
    void provide(const ::std::string& name, ::std::shared_ptr<T> instance) {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        ServiceKey key(name, ::std::type_index(typeid(T)));
        services_[key] = ::std::static_pointer_cast<void>(instance);
    }
    
    // Provide a factory for lazy instantiation
    template <typename T>
    void provide_factory(const ::std::string& name, ::std::function<::std::shared_ptr<T>()> factory) {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        ServiceKey key(name, ::std::type_index(typeid(T)));
        factories_[key] = [factory = ::std::move(factory)]() -> ::std::shared_ptr<void> {
            return ::std::static_pointer_cast<void>(factory());
        };
    }
    
    // Get a service (throws if not found)
    template <typename T>
    ::std::shared_ptr<T> get(const ::std::string& name) {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        ServiceKey key(name, ::std::type_index(typeid(T)));
        
        // Check direct services first
        auto it = services_.find(key);
        if (it != services_.end()) {
            return ::std::static_pointer_cast<T>(it->second);
        }
        
        // Check factories
        auto fit = factories_.find(key);
        if (fit != factories_.end()) {
            auto instance = ::std::static_pointer_cast<T>(fit->second());
            // Cache the created instance
            services_[key] = instance;
            return instance;
        }
        
        throw ::std::runtime_error("Service not found: " + name);
    }
    
    // Try to get a service (returns null if not found)
    template <typename T>
    ::std::shared_ptr<T> try_get(const ::std::string& name) {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        ServiceKey key(name, ::std::type_index(typeid(T)));
        
        auto it = services_.find(key);
        if (it != services_.end()) {
            return ::std::static_pointer_cast<T>(it->second);
        }
        
        auto fit = factories_.find(key);
        if (fit != factories_.end()) {
            auto instance = ::std::static_pointer_cast<T>(fit->second());
            services_[key] = instance;
            return instance;
        }
        
        return nullptr;
    }
    
    // Remove a service
    template <typename T>
    void remove(const ::std::string& name) {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        ServiceKey key(name, ::std::type_index(typeid(T)));
        services_.erase(key);
        factories_.erase(key);
    }
    
    // Check if a service exists
    template <typename T>
    bool has(const ::std::string& name) const {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        ServiceKey key(name, ::std::type_index(typeid(T)));
        return services_.count(key) || factories_.count(key);
    }
    
    // Create a child context with isolation (for subagents)
    ::std::shared_ptr<PluginContext> isolate(const ::std::vector<::std::string>& service_names = {}) {
        auto child = ::std::make_shared<PluginContext>();
        {
            ::std::lock_guard<::std::mutex> lock(mutex_);
            // Copy all services by default
            child->services_ = services_;
            child->factories_ = factories_;
            
            // If isolation list provided, remove those services from child
            // so they must be provided locally
            for (const auto& name : service_names) {
                for (auto it = child->services_.begin(); it != child->services_.end();) {
                    if (it->first.name() == name) {
                        it = child->services_.erase(it);
                    } else {
                        ++it;
                    }
                }
                for (auto it = child->factories_.begin(); it != child->factories_.end();) {
                    if (it->first.name() == name) {
                        it = child->factories_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        child->parent_ = ::std::const_pointer_cast<PluginContext>(shared_from_this());
        return child;
    }
    
    // Extend with additional services (child inherits parent's services)
    ::std::shared_ptr<PluginContext> extend() {
        auto child = ::std::make_shared<PluginContext>();
        child->parent_ = ::std::const_pointer_cast<PluginContext>(shared_from_this());
        return child;
    }
    
    // Get parent context
    ::std::shared_ptr<PluginContext> parent() const { return parent_; }
    
    // Event bus for this context
    EventBus* event_bus() { return &event_bus_; }
    const EventBus* event_bus() const { return &event_bus_; }

private:
    mutable ::std::mutex mutex_;
    ::std::unordered_map<ServiceKey, ::std::shared_ptr<void>> services_;
    ::std::unordered_map<ServiceKey, ::std::function<::std::shared_ptr<void>()>> factories_;
    ::std::shared_ptr<PluginContext> parent_;
    EventBus event_bus_;
    
    // For isolate/extend to work
    ::std::shared_ptr<PluginContext> shared_from_this() const {
        return ::std::static_pointer_cast<PluginContext>(weak_from_this().lock());
    }
    ::std::weak_ptr<PluginContext> weak_from_this() const {
        return weak_this_;
    }
    ::std::weak_ptr<PluginContext> weak_this_;
    
    template <typename T>
    friend ::std::shared_ptr<PluginContext> make_plugin_context();
};

template <typename T>
::std::shared_ptr<PluginContext> make_plugin_context() {
    auto ctx = ::std::shared_ptr<PluginContext>(new PluginContext());
    ctx->weak_this_ = ctx;
    return ctx;
}

// Built-in service names
namespace ServiceNames {
    inline constexpr const char* TOOLS = "tools";
    inline constexpr const char* LLM_PROVIDER = "llm_provider";
    inline constexpr const char* SESSION_STORE = "session_store";
    inline constexpr const char* CONFIG = "config";
    inline constexpr const char* AGENT_LOOP = "agent_loop";
    inline constexpr const char* MCP_CLIENT = "mcp_client";
    inline constexpr const char* RAG_ENGINE = "rag_engine";
    inline constexpr const char* SYSTEM_ONE = "system_one";
    inline constexpr const char* FILE_SYSTEM = "file_system";
    inline constexpr const char* SHELL = "shell";
    inline constexpr const char* APPROVAL = "approval";
    inline constexpr const char* NOTIFICATIONS = "notifications";
}

// Plugin base class - all plugins inherit from this
class Plugin {
public:
    virtual ~Plugin() = default;
    virtual ::std::string name() const = 0;
    virtual ::std::string version() const = 0;
    virtual ::std::vector<::std::string> depends_on() const { return {}; }
    
    // Called when plugin is mounted
    virtual void apply(PluginContext& ctx) = 0;
    
    // Called when plugin is unmounted (for reversible effects)
    virtual void unapply(PluginContext& ctx) = 0;
    
    // Default: no-op
    virtual void on_activate() {}
    virtual void on_deactivate() {}
};

// Plugin manager - handles plugin lifecycle and dependencies
class PluginManager {
public:
    PluginManager() : root_context_(make_plugin_context<PluginContext>()) {}
    
    // Mount a plugin (and its dependencies)
    bool mount(::std::unique_ptr<Plugin> plugin) {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        
        // Check dependencies
        for (const auto& dep : plugin->depends_on()) {
            if (!loaded_plugins_.count(dep)) {
                // Dependency not loaded
                return false;
            }
        }
        
        try {
            plugin->apply(*root_context_);
            loaded_plugins_[plugin->name()] = ::std::move(plugin);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Unmount a plugin (reversible effects)
    bool unmount(const ::std::string& name) {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        
        auto it = loaded_plugins_.find(name);
        if (it == loaded_plugins_.end()) return false;
        
        try {
            it->second->unapply(*root_context_);
            loaded_plugins_.erase(it);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Get root context
    PluginContext& root() { return *root_context_; }
    const PluginContext& root() const { return *root_context_; }
    
    // Check if plugin is loaded
    bool is_loaded(const ::std::string& name) const {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        return loaded_plugins_.count(name);
    }
    
    // Get loaded plugin names
    ::std::vector<::std::string> loaded_plugins() const {
        ::std::lock_guard<::std::mutex> lock(mutex_);
        ::std::vector<::std::string> names;
        names.reserve(loaded_plugins_.size());
        for (const auto& [name, _] : loaded_plugins_) {
            names.push_back(name);
        }
        return names;
    }

private:
    mutable ::std::mutex mutex_;
    ::std::shared_ptr<PluginContext> root_context_;
    ::std::unordered_map<::std::string, ::std::unique_ptr<Plugin>> loaded_plugins_;
};

// Convenience macros for defining capabilities
#define DEFINE_CAPABILITY(name, interface) \
    class name##Capability : public pluto::CapabilityDef<interface> { \
    public: \
        ::std::string name() const override { return #name; } \
        ::std::string version() const override { return "1.0"; } \
    }

#define PROVIDE_CAPABILITY(name, impl) \
    class name##Provider : public pluto::CapabilityProvider<impl> { \
    public: \
        ::std::shared_ptr<impl> create() const override { return ::std::make_shared<impl>(); } \
        ::std::string provider_name() const override { return #name; } \
    }

} // namespace pluto