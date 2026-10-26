#pragma once

#include "DecisionModel.h"
#include "SystemOneConfig.h"
#include "../tools/Tool.h"

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace pluto {
namespace systemone {

// SystemOneClient wraps a DecisionModel and exposes its capabilities as
// Pluto tools (read_file style) so the agent can call them through the
// standard tool registry.
// Exposes 9 tools:
//   systemone_classify, systemone_classify_multi, systemone_verify,
//   systemone_guard, systemone_select_relevant, systemone_same_intent,
//   systemone_triage, systemone_answerable, systemone_consult,
//   systemone_decide_program
class SystemOneClient {
public:
    SystemOneClient(std::unique_ptr<DecisionModel> model);
    ~SystemOneClient();

    // Register all System One tools into the given ToolRegistry.
    void register_tools(ToolRegistry& registry);

    // Direct tool invocation (for programmatic use).
    std::string call_tool(const std::string& name, const std::string& args_json);

    // Returns true if the decision model is available.
    bool is_available() const;

    // Returns the name of the active decision model.
    std::string model_name() const;

private:
    std::unique_ptr<DecisionModel> fModel;

    // Individual tool implementations
    ToolResult tool_classify(const std::string& input_json);
    ToolResult tool_classify_multi(const std::string& input_json);
    ToolResult tool_verify(const std::string& input_json);
    ToolResult tool_guard(const std::string& input_json);
    ToolResult tool_select_relevant(const std::string& input_json);
    ToolResult tool_same_intent(const std::string& input_json);
    ToolResult tool_triage(const std::string& input_json);
    ToolResult tool_answerable(const std::string& input_json);
    ToolResult tool_consult(const std::string& input_json);
    ToolResult tool_decide_program(const std::string& input_json);
};

// Convenience: create a SystemOneClient from config and register tools.
std::unique_ptr<SystemOneClient> CreateSystemOneClient(
    const SystemOneConfig& config,
    ToolRegistry& registry);

} // namespace systemone
} // namespace pluto
