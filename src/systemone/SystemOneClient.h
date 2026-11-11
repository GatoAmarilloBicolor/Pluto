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
// Exposes 20+ tools:
//   Classification: systemone_classify, systemone_classify_multi,
//                   systemone_classify_sentiment, systemone_classify_priority,
//                   systemone_classify_topic, systemone_classify_urgency
//   Verification/Safety: systemone_verify, systemone_guard
//   Context/Token Savings: systemone_select_relevant,
//                   systemone_extract_key_points, systemone_summarize_text,
//                   systemone_extract_entities, systemone_rank_importance,
//                   systemone_compute_headroom, systemone_compact_messages,
//                   systemone_optimize_context
//   Decision/Triage: systemone_triage, systemone_triage_full,
//                   systemone_answerable, systemone_decide_route,
//                   systemone_decide_priority, systemone_decide_action,
//                   systemone_consult, systemone_decide_program
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

    // Returns the underlying DecisionModel pointer (for advanced use).
    DecisionModel* model() const;

private:
    std::unique_ptr<DecisionModel> fModel;

    // Tool registration helper
    struct ToolDef {
        const char* name;
        const char* description;
        const char* schema;
        std::string (*fn)(DecisionModel*, const std::string&);
    };
    
    
};

// Convenience: create a SystemOneClient from config and register tools.
std::unique_ptr<SystemOneClient> CreateSystemOneClient(
    const SystemOneConfig& config,
    ToolRegistry& registry);

}  // namespace systemone
}  // namespace pluto

