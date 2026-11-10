// FileToolBase - Shared base for file tools (ReadFile, WriteFile, EditFile, etc.)
// Centralizes JSON parsing, workspace validation, logging, and error handling.

#pragma once

#include "Tool.h"
#include "../json.h"
#include "../workspace.h"
#include "../log.h"

#include <string>
#include <optional>

namespace pluto {

// Base class for file tools that operate on workspace paths.
// Provides common functionality: JSON arg parsing, workspace validation, error handling.
class FileToolBase : public Tool {
public:
    // Parse a JSON input string, extract a required string field.
    // Returns empty optional on parse error or missing field.
    static std::optional<std::string> parse_string_field(const std::string& input_json, const std::string& field);

    // Parse a JSON input string, extract an optional string field.
    // Returns nullopt if field missing or parse error.
    static std::optional<std::string> parse_optional_string_field(const std::string& input_json, const std::string& field);

    // Validate and canonicalize a path against workspace.
    // Returns canonicalized path on success, empty string on failure (sets error in result).
    static std::string validate_path(ToolResult& result, const std::string& path, const char* tool_name);

    // Log access and return whether allowed.
    static bool check_workspace(ToolResult& result, const std::string& path, const char* tool_name, [[maybe_unused]] bool for_write = false);

    // Common error handling for JSON parse failures.
    static void set_json_error(ToolResult& result, const std::exception& e);

    // Common error handling for missing required field.
    static void set_missing_field_error(ToolResult& result, const char* field);

    // Common preview implementation - show tool name + path.
    std::string preview(const std::string& input_json) const override;

protected:
    // Override in derived class to provide tool name for preview.
    virtual std::string tool_name_for_preview() const = 0;
    // Override to extract path from parsed JSON for preview.
    virtual std::string extract_path_for_preview(const json::Value& v) const = 0;
};

// Inline implementations
inline std::optional<std::string> FileToolBase::parse_string_field(const std::string& input_json, const std::string& field) {
    try {
        auto v = json::Value::parse(input_json);
        if (const auto* val = v.find(field); val && val->is_str()) {
            return val->as_str();
        }
    } catch (...) {}
    return std::nullopt;
}

inline std::optional<std::string> FileToolBase::parse_optional_string_field(const std::string& input_json, const std::string& field) {
    try {
        auto v = json::Value::parse(input_json);
        if (const auto* val = v.find(field); val && val->is_str()) {
            return val->as_str();
        }
    } catch (...) {}
    return std::nullopt;
}

inline std::string FileToolBase::validate_path(ToolResult& result, const std::string& path, const char* tool_name) {
    if (path.empty()) {
        result.is_error = true;
        result.output = "path required";
        return "";
    }
    char* resolved = realpath(path.c_str(), nullptr);
    std::string canon = resolved ? resolved : path;
    if (resolved) free(resolved);
    if (!WorkspaceAllows(canon)) {
        LogAccess(canon, tool_name, false);
        result.is_error = true;
        result.output = canon + " is outside workspace " + WorkspaceRoot()
                     + " — use `pluto trust add <dir>` to allow";
        return "";
    }
    return canon;
}

inline bool FileToolBase::check_workspace(ToolResult& result, const std::string& path, const char* tool_name, [[maybe_unused]] bool for_write) {
    char* resolved = realpath(path.c_str(), nullptr);
    std::string canon = resolved ? resolved : path;
    if (resolved) free(resolved);
    if (!WorkspaceAllows(canon)) {
        LogAccess(canon, tool_name, false);
        result.is_error = true;
        result.output = canon + " is outside workspace " + WorkspaceRoot()
                     + " — use `pluto trust add <dir>` to allow";
        return false;
    }
    LogAccess(canon, tool_name, true);
    return true;
}

inline void FileToolBase::set_json_error(ToolResult& result, const std::exception& e) {
    result.is_error = true;
    result.output = std::string("args: ") + e.what();
}

inline void FileToolBase::set_missing_field_error(ToolResult& result, const char* field) {
    result.is_error = true;
    result.output = std::string(field) + " required";
}

inline std::string FileToolBase::preview(const std::string& input_json) const {
    try {
        auto v = json::Value::parse(input_json);
        std::string path = extract_path_for_preview(v);
        return tool_name_for_preview() + " " + path;
    } catch (...) {
        return Tool::preview(input_json);
    }
}

}  // namespace pluto
