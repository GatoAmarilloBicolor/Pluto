#include "HaikuAbstraction.h"

#ifdef __HAIKU__
#include <Entry.h>
#include <Directory.h>
#include <File.h>
#include <Path.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <Query.h>
#include <NodeMonitor.h>
#include <Messenger.h>
#include <Alert.h>
#include <Clipboard.h>
#include <Mime.h>
#include <Roster.h>
#include <String.h>

#include "BQueryUtil.h"

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <map>

namespace pluto {

// =============================================================================
// HaikuStorage - BFS implementation
// =============================================================================

class HaikuStorage : public IStorage {
public:
    bool exists(const std::string& path) const override {
        BEntry entry(path.c_str());
        return entry.InitCheck() == B_OK && entry.Exists();
    }

    bool create_directory(const std::string& path, mode_t mode) override {
        BDirectory dir;
        status_t err = dir.CreateDirectory(path.c_str(), NULL);
        return err == B_OK;
    }

    bool remove(const std::string& path) override {
        BEntry entry(path.c_str());
        return entry.Remove() == B_OK;
    }

    bool rename(const std::string& from, const std::string& to) override {
        BEntry entry(from.c_str());
        BDirectory parent;
        BPath parentPath(to.c_str());
        parentPath.GetParent(&parentPath);
        parent.SetTo(parentPath.Path());
        return entry.MoveTo(&parent, to.c_str()) == B_OK;
    }

    std::optional<FileInfo> stat(const std::string& path) const override {
        BEntry entry(path.c_str());
        if (entry.InitCheck() != B_OK) return std::nullopt;

        BNode node(&entry);
        if (node.InitCheck() != B_OK) return std::nullopt;

        FileInfo info;
        info.path = path;
        
        BPath bpath(&entry);
        info.name = bpath.Leaf();

        struct stat st;
        node.GetStat(&st);
        info.size = st.st_size;
        info.modified = st.st_mtime;
        info.is_directory = S_ISDIR(st.st_mode);
        info.is_symlink = S_ISLNK(st.st_mode);

        // Read attributes
        char attrName[B_ATTR_NAME_LENGTH];
        while (node.GetNextAttrName(attrName) == B_OK) {
            attr_info ainfo;
            if (node.GetAttrInfo(attrName, &ainfo) == B_OK) {
                std::string value(ainfo.size, '\0');
                node.ReadAttr(attrName, ainfo.type, 0, &value[0], ainfo.size);
                info.attrs[attrName] = value;
            }
        }
        return info;
    }

    std::vector<FileInfo> list(const std::string& path) const override {
        std::vector<FileInfo> results;
        BDirectory dir(path.c_str());
        if (dir.InitCheck() != B_OK) return results;

        BEntry entry;
        while (dir.GetNextEntry(&entry) == B_OK) {
            if (auto info = stat(entry.Path())) {
                results.push_back(*info);
            }
        }
        return results;
    }

    std::optional<std::string> read_file(const std::string& path) const override {
        BFile file(path.c_str(), B_READ_ONLY);
        if (file.InitCheck() != B_OK) return std::nullopt;

        off_t size;
        file.GetSize(&size);
        std::string content(size, '\0');
        ssize_t read = file.Read(&content[0], size);
        if (read < 0) return std::nullopt;
        content.resize(read);
        return content;
    }

    bool write_file(const std::string& path, const std::string& content, mode_t) override {
        BFile file(path.c_str(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
        if (file.InitCheck() != B_OK) return false;
        return file.Write(content.c_str(), content.size()) == (ssize_t)content.size();
    }

    bool append_file(const std::string& path, const std::string& content) override {
        BFile file(path.c_str(), B_WRITE_ONLY | B_CREATE_FILE);
        if (file.InitCheck() != B_OK) return false;
        off_t size;
        file.GetSize(&size);
        file.Seek(size, SEEK_SET);
        return file.Write(content.c_str(), content.size()) == (ssize_t)content.size();
    }

    bool write_attr(const std::string& path, const std::string& name,
                    const std::string& type, const std::string& value) override {
        BNode node(path.c_str());
        if (node.InitCheck() != B_OK) return false;
        
        uint32_t type_code = B_STRING_TYPE;
        if (type == "int32") type_code = B_INT32_TYPE;
        else if (type == "int64") type_code = B_INT64_TYPE;
        else if (type == "float") type_code = B_FLOAT_TYPE;
        else if (type == "double") type_code = B_DOUBLE_TYPE;
        else if (type == "bool") type_code = B_BOOL_TYPE;
        else if (type == "raw") type_code = B_RAW_TYPE;

        return node.WriteAttr(name.c_str(), type_code, 0, value.c_str(), value.size()) == (ssize_t)value.size();
    }

    std::optional<std::string> read_attr(const std::string& path, const std::string& name) const override {
        BNode node(path.c_str());
        if (node.InitCheck() != B_OK) return std::nullopt;

        attr_info info;
        if (node.GetAttrInfo(name.c_str(), &info) != B_OK) return std::nullopt;

        std::string value(info.size, '\0');
        if (node.ReadAttr(name.c_str(), info.type, 0, &value[0], info.size) != (ssize_t)info.size) {
            return std::nullopt;
        }
        return value;
    }

    std::vector<AttrInfo> list_attrs(const std::string& path) const override {
        std::vector<AttrInfo> results;
        BNode node(path.c_str());
        if (node.InitCheck() != B_OK) return results;

        char attrName[B_ATTR_NAME_LENGTH];
        while (node.GetNextAttrName(attrName) == B_OK) {
            attr_info info;
            if (node.GetAttrInfo(attrName, &info) == B_OK) {
                AttrInfo ai;
                ai.name = attrName;
                ai.type = std::to_string(info.type);
                ai.size = info.size;
                if (info.type == B_STRING_TYPE) {
                    std::string value(info.size, '\0');
                    node.ReadAttr(attrName, info.type, 0, &value[0], info.size);
                    ai.value = value;
                }
                results.push_back(ai);
            }
        }
        return results;
    }

    bool remove_attr(const std::string& path, const std::string& name) override {
        BNode node(path.c_str());
        return node.InitCheck() == B_OK && node.RemoveAttr(name.c_str()) == B_OK;
    }

    std::vector<QueryResult> query(const std::string& predicate,
                                   const std::vector<std::string>& volumes) const override {
        std::vector<QueryResult> results;
        
        if (volumes.empty()) {
            // Query all volumes
            bquery::for_each_volume([&](const BVolume& vol) {
                bquery::query_volume(vol, predicate, [&](const std::string& path, const BEntry& entry) {
                    QueryResult qr;
                    qr.path = path;
                    BNode node(&entry);
                    qr.attrs = bquery::read_string_attrs(node);
                    results.push_back(std::move(qr));
                });
                return true;
            });
        } else {
            bquery::for_each_volume(volumes, [&](const BVolume& vol) {
                bquery::query_volume(vol, predicate, [&](const std::string& path, const BEntry& entry) {
                    QueryResult qr;
                    qr.path = path;
                    BNode node(&entry);
                    qr.attrs = bquery::read_string_attrs(node);
                    results.push_back(std::move(qr));
                });
                return true;
            });
        }
        return results;
    }

    std::unique_ptr<LiveQuery> create_live_query(const std::string& path,
                                                  const std::string& predicate,
                                                  LiveQueryCallback cb) override {
        return std::make_unique<HaikuLiveQuery>(path, predicate, std::move(cb));
    }

private:
    void query_volume(const BVolume& vol, const std::string& predicate,
                      std::vector<QueryResult>& results) const {
        BQuery query;
        query.SetVolume(&vol);
        query.SetPredicate(predicate.c_str());
        
        if (query.Fetch() != B_OK) return;

        BEntry entry;
        while (query.GetNextEntry(&entry) == B_OK) {
            BPath path(&entry);
            QueryResult qr;
            qr.path = path.Path();
            
            BNode node(&entry);
            char attrName[B_ATTR_NAME_LENGTH];
            while (node.GetNextAttrName(attrName) == B_OK) {
                attr_info info;
                if (node.GetAttrInfo(attrName, &info) == B_OK && info.type == B_STRING_TYPE) {
                    std::string value(info.size, '\0');
                    node.ReadAttr(attrName, info.type, 0, &value[0], info.size);
                    qr.attrs[attrName] = value;
                }
            }
            results.push_back(qr);
        }
    }
};

// =============================================================================
// HaikuLiveQuery - Node monitor implementation
// =============================================================================

class HaikuLiveQuery : public LiveQuery {
public:
    HaikuLiveQuery(const std::string& path, const std::string& predicate, LiveQueryCallback cb)
        : path_(path), predicate_(predicate), callback_(std::move(cb)) {}

    void start() override {
        if (running_) return;
        
        BDirectory dir(path_.c_str());
        if (dir.InitCheck() != B_OK) return;

        // Watch for node changes
        status_t err = watch_node(dir.NodeRef(), B_WATCH_NAME | B_WATCH_STAT | B_WATCH_ATTR,
                                  BMessenger(this, NULL));
        if (err == B_OK) {
            running_ = true;
        }
    }

    void stop() override {
        if (!running_) return;
        
        BDirectory dir(path_.c_str());
        if (dir.InitCheck() == B_OK) {
            watch_node(dir.NodeRef(), B_STOP_WATCHING, BMessenger(this, NULL));
        }
        running_ = false;
    }

    bool is_running() const override { return running_; }

    // BHandler interface for node monitor messages
    void MessageReceived(BMessage* msg) {
        if (!msg || !running_) return;
        
        int32 opcode;
        if (msg->FindInt32("opcode", &opcode) != B_OK) return;

        entry_ref ref;
        if (msg->FindRef("ref", &ref) != B_OK) return;

        BPath path(&ref);
        std::string op_str;
        switch (opcode) {
            case B_ENTRY_CREATED: op_str = "created"; break;
            case B_ENTRY_REMOVED: op_str = "removed"; break;
            case B_ENTRY_MOVED: op_str = "moved"; break;
            case B_STAT_CHANGED: op_str = "changed"; break;
            case B_ATTR_CHANGED: op_str = "attr_changed"; break;
            default: op_str = "unknown"; break;
        }

        if (callback_) {
            callback_(path.Path(), op_str);
        }
    }

private:
    std::string path_;
    std::string predicate_;
    LiveQueryCallback callback_;
    bool running_ = false;
};

// =============================================================================
// HaikuNotification - BAlert/Notification implementation
// =============================================================================

class HaikuNotification : public INotification {
public:
    void send(const Notification& n) override {
        send_impl(n, true);
    }

    void send_async(const Notification& n) override {
        send_impl(n, false);
    }

private:
    void send_impl(const Notification& n, bool blocking) {
        // For now, use BAlert for GUI notifications
        // In a real implementation, this would use BNotification (Haiku R1+)
        BAlert::Type alert_type = B_INFO_ALERT;
        switch (n.type) {
            case Notification::Type::WARNING: alert_type = B_WARNING_ALERT; break;
            case Notification::Type::ERROR: alert_type = B_ERROR_ALERT; break;
            case Notification::Type::SUCCESS: alert_type = B_INFO_ALERT; break;
            default: alert_type = B_INFO_ALERT; break;
        }

        BAlert* alert = new BAlert("Pluto", n.message.c_str(), "OK", nullptr, nullptr, 
                                   B_WIDTH_AS_USUAL, alert_type);
        if (blocking) {
            alert->Go();
        } else {
            alert->Go(NULL);
        }
    }
};

// =============================================================================
// HaikuQuery - Tracker Find / BQuery implementation
// =============================================================================

class HaikuQuery : public IQuery {
public:
    std::vector<SearchResult> search(const std::string& query,
                                     const std::vector<std::string>& paths,
                                     int max_results) override {
        std::vector<SearchResult> results;
        
        // Build BQuery predicate from query string
        std::string predicate = build_predicate(query);
        int count = 0;
        
        bquery::for_each_volume([&](const BVolume& vol) {
            if (count >= max_results) return false;
            
            BQuery bquery;
            bquery.SetVolume(&vol);
            bquery.SetPredicate(predicate.c_str());
            
            if (bquery.Fetch() != B_OK) return true;
            
            BEntry entry;
            while (bquery.GetNextEntry(&entry) == B_OK && count < max_results) {
                BPath path(&entry);
                SearchResult sr;
                sr.path = path.Path();
                sr.name = path.Leaf();
                
                BNode node(&entry);
                sr.attrs = bquery::read_string_attrs(node);
                
                // Generate snippet from attrs
                sr.snippet = generate_snippet(sr.attrs, query);
                sr.score = calculate_score(sr, query);
                
                results.push_back(sr);
                count++;
            }
            return true;
        });
        
        // Sort by score
        std::sort(results.begin(), results.end(), 
                  [](const SearchResult& a, const SearchResult& b) {
                      return a.score > b.score;
                  });
        
        return results;
    }

    std::vector<SearchResult> search_by_attr(const std::string& attr_name,
                                              const std::string& attr_value,
                                              const std::string& attr_type,
                                              int max_results) override {
        std::string predicate = attr_name + " == \"" + attr_value + "\"";
        return search(predicate, {}, max_results);
    }

private:
    std::string build_predicate(const std::string& query) {
        // Simple query parsing - could be enhanced
        // For now, search in common attributes
        return "(name == \"" + query + "*\") | "
               "(pluto:title == \"" + query + "*\") | "
               "(pluto:tags == \"" + query + "*\") | "
               "(MAIL:subject == \"" + query + "*\")";
    }

    std::string generate_snippet(const std::map<std::string, std::string>& attrs,
                                 const std::string& query) {
        for (const auto& [key, value] : attrs) {
            if (value.find(query) != std::string::npos) {
                return key + ": " + value.substr(0, 100);
            }
        }
        return "";
    }

    double calculate_score(const SearchResult& sr, const std::string& query) {
        double score = 0.0;
        for (const auto& [key, value] : sr.attrs) {
            if (key == "name" || key == "pluto:title") {
                if (value.find(query) == 0) score += 10.0;
                else if (value.find(query) != std::string::npos) score += 5.0;
            } else if (key == "pluto:tags") {
                if (value.find(query) != std::string::npos) score += 3.0;
            } else {
                if (value.find(query) != std::string::npos) score += 1.0;
            }
        }
        return score;
    }
};

// =============================================================================
// HaikuDesktop - Desktop integration
// =============================================================================

class HaikuDesktop : public IDesktop {
public:
    bool launch(const std::string& url_or_path) override {
        BString url(url_or_path.c_str());
        return be_roster->Launch(url.String()) == B_OK;
    }

    bool open_folder(const std::string& path) override {
        entry_ref ref;
        if (get_ref_for_path(path.c_str(), &ref) != B_OK) return false;
        return be_roster->Launch(&ref) == B_OK;
    }

    bool show_in_tracker(const std::string& path) override {
        entry_ref ref;
        if (get_ref_for_path(path.c_str(), &ref) != B_OK) return false;
        
        BMessage msg(B_REFS_RECEIVED);
        msg.AddRef("refs", &ref);
        BMessenger tracker("application/x-vnd.Be-TRAK");
        return tracker.SendMessage(&msg) == B_OK;
    }

    std::string get_clipboard() override {
        if (!be_clipboard->Lock()) return "";
        
        BMessage* clip = be_clipboard->Data();
        std::string result;
        
        if (clip) {
            const char* text;
            ssize_t len;
            if (clip->FindData("text/plain", B_MIME_TYPE, (const void**)&text, &len) == B_OK) {
                result.assign(text, len);
            }
        }
        
        be_clipboard->Unlock();
        return result;
    }

    bool set_clipboard(const std::string& text) override {
        if (!be_clipboard->Lock()) return false;
        
        be_clipboard->Clear();
        BMessage* clip = be_clipboard->Data();
        if (clip) {
            clip->AddData("text/plain", B_MIME_TYPE, text.c_str(), text.size());
            be_clipboard->Commit();
        }
        
        be_clipboard->Unlock();
        return true;
    }

    std::string get_selection() override {
        // Haiku doesn't have a global selection API like X11
        // This would require tracking the focused text view
        return "";
    }

    std::string get_mime_type(const std::string& path) override {
        BMimeType mime;
        if (BMimeType::GuessMimeType(path.c_str(), &mime) == B_OK) {
            return mime.Type();
        }
        return "";
    }
};

// =============================================================================
// HaikuServiceLocator implementation
// =============================================================================

void HaikuServiceLocator::init_haiku() {
    set_storage(std::make_unique<HaikuStorage>());
    set_notification(std::make_unique<HaikuNotification>());
    set_query(std::make_unique<HaikuQuery>());
    set_desktop(std::make_unique<HaikuDesktop>());
}

}  // namespace pluto

#else  // !__HAIKU__

// Mock implementations for cross-compilation
namespace pluto {

class MockStorage : public IStorage {
public:
    bool exists(const std::string&) const override { return false; }
    bool create_directory(const std::string&, mode_t) override { return true; }
    bool remove(const std::string&) override { return true; }
    bool rename(const std::string&, const std::string&) override { return true; }
    std::optional<FileInfo> stat(const std::string&) const override { return std::nullopt; }
    std::vector<FileInfo> list(const std::string&) const override { return {}; }
    std::optional<std::string> read_file(const std::string&) const override { return std::nullopt; }
    bool write_file(const std::string&, const std::string&, mode_t) override { return true; }
    bool append_file(const std::string&, const std::string&) override { return true; }
    bool write_attr(const std::string&, const std::string&, const std::string&, const std::string&) override { return true; }
    std::optional<std::string> read_attr(const std::string&, const std::string&) const override { return std::nullopt; }
    std::vector<AttrInfo> list_attrs(const std::string&) const override { return {}; }
    bool remove_attr(const std::string&, const std::string&) override { return true; }
    std::vector<QueryResult> query(const std::string&, const std::vector<std::string>&) const override { return {}; }
    std::unique_ptr<LiveQuery> create_live_query(const std::string&, const std::string&, LiveQueryCallback) override {
        return std::make_unique<MockLiveQuery>();
    }
};

class MockLiveQuery : public LiveQuery {
public:
    void start() override {}
    void stop() override {}
    bool is_running() const override { return false; }
};

class MockNotification : public INotification {
public:
    void send(const Notification&) override {}
    void send_async(const Notification&) override {}
};

class MockQuery : public IQuery {
public:
    std::vector<SearchResult> search(const std::string&, const std::vector<std::string>&, int) override { return {}; }
    std::vector<SearchResult> search_by_attr(const std::string&, const std::string&, const std::string&, int) override { return {}; }
};

class MockDesktop : public IDesktop {
public:
    bool launch(const std::string&) override { return true; }
    bool open_folder(const std::string&) override { return true; }
    bool show_in_tracker(const std::string&) override { return true; }
    std::string get_clipboard() override { return ""; }
    bool set_clipboard(const std::string&) override { return true; }
    std::string get_selection() override { return ""; }
    std::string get_mime_type(const std::string&) override { return ""; }
};

void HaikuServiceLocator::init_mock() {
    set_storage(std::make_unique<MockStorage>());
    set_notification(std::make_unique<MockNotification>());
    set_query(std::make_unique<MockQuery>());
    set_desktop(std::make_unique<MockDesktop>());
}

}  // namespace pluto

#endif  // __HAIKU__