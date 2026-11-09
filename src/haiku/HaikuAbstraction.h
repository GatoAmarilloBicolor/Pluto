#ifndef PLUTO_HAIKU_ABSTRACTION_H
#define PLUTO_HAIKU_ABSTRACTION_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <map>

#include "../json.h"

namespace pluto {

// =============================================================================
// IStorage - Abstract file/attribute storage (replaces direct BFS calls)
// =============================================================================

struct FileInfo {
    std::string path;
    std::string name;
    int64_t size = 0;
    time_t modified = 0;
    bool is_directory = false;
    bool is_symlink = false;
    std::map<std::string, std::string> attrs;  // Extended attributes
};

struct AttrInfo {
    std::string name;
    std::string type;
    std::string value;
    size_t size = 0;
};

class IStorage {
public:
    virtual ~IStorage() = default;

    // File operations
    virtual bool exists(const std::string& path) const = 0;
    virtual bool create_directory(const std::string& path, mode_t mode = 0755) = 0;
    virtual bool remove(const std::string& path) = 0;
    virtual bool rename(const std::string& from, const std::string& to) = 0;
    virtual std::optional<FileInfo> stat(const std::string& path) const = 0;
    virtual std::vector<FileInfo> list(const std::string& path) const = 0;

    // File I/O
    virtual std::optional<std::string> read_file(const std::string& path) const = 0;
    virtual bool write_file(const std::string& path, const std::string& content, mode_t mode = 0644) = 0;
    virtual bool append_file(const std::string& path, const std::string& content) = 0;

    // Extended attributes (BFS-style)
    virtual bool write_attr(const std::string& path, const std::string& name,
                            const std::string& type, const std::string& value) = 0;
    virtual std::optional<std::string> read_attr(const std::string& path, const std::string& name) const = 0;
    virtual std::vector<AttrInfo> list_attrs(const std::string& path) const = 0;
    virtual bool remove_attr(const std::string& path, const std::string& name) = 0;

    // Query (BQuery-like)
    struct QueryResult {
        std::string path;
        std::map<std::string, std::string> attrs;
    };
    virtual std::vector<QueryResult> query(const std::string& predicate,
                                           const std::vector<std::string>& volumes = {}) const = 0;

    // Live query (node monitor)
    using LiveQueryCallback = std::function<void(const std::string& path, const std::string& op)>;
    virtual std::unique_ptr<class LiveQuery> create_live_query(const std::string& path,
                                                                const std::string& predicate,
                                                                LiveQueryCallback cb) = 0;
};

class LiveQuery {
public:
    virtual ~LiveQuery() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
};

// =============================================================================
// INotification - Abstract notifications
// =============================================================================

struct Notification {
    enum class Type { INFO, WARNING, ERROR, SUCCESS };
    Type type = Type::INFO;
    std::string title;
    std::string message;
    std::string action_label;
    std::function<void()> action;
    int timeout_ms = 5000;
};

class INotification {
public:
    virtual ~INotification() = default;
    virtual void send(const Notification& n) = 0;
    virtual void send_async(const Notification& n) = 0;
};

// =============================================================================
// IQuery - Abstract search/query (Tracker Find, BQuery)
// =============================================================================

struct SearchResult {
    std::string path;
    std::string name;
    std::string snippet;
    std::map<std::string, std::string> attrs;
    double score = 0.0;
};

class IQuery {
public:
    virtual ~IQuery() = default;
    virtual std::vector<SearchResult> search(const std::string& query,
                                             const std::vector<std::string>& paths = {},
                                             int max_results = 50) = 0;
    virtual std::vector<SearchResult> search_by_attr(const std::string& attr_name,
                                                      const std::string& attr_value,
                                                      const std::string& attr_type = "string",
                                                      int max_results = 50) = 0;
};

// =============================================================================
// IDesktop - Abstract desktop integration (launch, clipboard, etc.)
// =============================================================================

class IDesktop {
public:
    virtual ~IDesktop() = default;
    virtual bool launch(const std::string& url_or_path) = 0;
    virtual bool open_folder(const std::string& path) = 0;
    virtual bool show_in_tracker(const std::string& path) = 0;
    virtual std::string get_clipboard() = 0;
    virtual bool set_clipboard(const std::string& text) = 0;
    virtual std::string get_selection() = 0;
    virtual std::string get_mime_type(const std::string& path) = 0;
};

// =============================================================================
// HaikuServiceLocator - Central registry for Haiku implementations
// =============================================================================

class HaikuServiceLocator {
public:
    static HaikuServiceLocator& instance();

    void set_storage(std::unique_ptr<IStorage> s) { storage_ = std::move(s); }
    void set_notification(std::unique_ptr<INotification> n) { notification_ = std::move(n); }
    void set_query(std::unique_ptr<IQuery> q) { query_ = std::move(q); }
    void set_desktop(std::unique_ptr<IDesktop> d) { desktop_ = std::move(d); }

    IStorage* storage() const { return storage_.get(); }
    INotification* notification() const { return notification_.get(); }
    IQuery* query() const { return query_.get(); }
    IDesktop* desktop() const { return desktop_.get(); }

    // Initialize with real Haiku implementations
    void init_haiku();

    // Initialize with mock implementations (for testing/cross-compile)
    void init_mock();

private:
    HaikuServiceLocator() = default;
    std::unique_ptr<IStorage> storage_;
    std::unique_ptr<INotification> notification_;
    std::unique_ptr<IQuery> query_;
    std::unique_ptr<IDesktop> desktop_;
};

// Convenience macros
#define gStorage pluto::HaikuServiceLocator::instance().storage()
#define gNotification pluto::HaikuServiceLocator::instance().notification()
#define gQuery pluto::HaikuServiceLocator::instance().query()
#define gDesktop pluto::HaikuServiceLocator::instance().desktop()

}  // namespace pluto

#endif // PLUTO_HAIKU_ABSTRACTION_H