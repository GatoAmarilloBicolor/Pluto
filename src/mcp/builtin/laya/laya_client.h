#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "../../../../systemone/DecisionModel.h"
#include "../../../../systemone/SystemOneConfig.h"
#include "../../../../systemone/TokenSaver.h"

namespace pluto {
namespace mcp {
namespace laya {

// Laya MCP Server - Dedicated decision model server
// Communicates with the Laya decision model via stdio transport
// Provides all System One decision capabilities with Laya-specific optimizations

struct LayaConfig {
    std::string server_name = "laya";
    std::string command = "laya-mcp-server";
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    bool auto_start = true;
    bool require_approval = false;
    int timeout_secs = 30;
    bool cache_enabled = true;
    int cache_ttl_secs = 300;
};

// Laya-specific result types with enhanced metadata
struct LayaClassifyResult {
    std::string label;
    double probability = 0.0;
    double confidence = 0.0;
    bool abstain = false;
    std::string reasoning;
    std::vector<std::string> alternative_labels;
    std::map<std::string, double> alternative_probabilities;
};

struct LayaVerifyResult {
    bool supported = false;
    double probability = 0.0;
    std::string explanation;
    std::vector<std::string> supporting_evidence;
    std::vector<std::string> contradicting_evidence;
};

struct LayaGuardResult {
    std::string risk = "LOW";
    std::vector<std::string> hits;
    double probability = 0.0;
    std::map<std::string, double> check_probabilities;
    std::vector<std::string> recommended_actions;
    bool requires_human_review = false;
};

struct LayaTriageResult {
    std::string guard_risk = "LOW";
    std::string route;
    std::vector<std::string> tags;
    bool urgent = false;
    double probability = 0.0;
    std::string reasoning;
    std::vector<std::string> recommended_actions;
    int estimated_tokens_saved = 0;
    int priority_score = 0;
};

struct LayaConsultResult {
    std::string choice;
    double probability = 0.0;
    bool abstain = false;
    std::string notes_used;
    std::vector<std::string> relevant_notes;
    double confidence = 0.0;
};

// Laya MCP Client - Dedicated client for Laya decision model
class LayaClient {
public:
    LayaClient(const LayaConfig& config);
    ~LayaClient();

    bool connect();
    bool is_connected() const;
    std::string name() const { return config.server_name; }

    // Classification
    LayaClassifyResult classify(const std::string& text,
                                 const std::map<std::string, std::string>& categories,
                                 double threshold = 0.5);

    ClassifyMultiResult classify_multi(const std::string& text,
                                        const std::map<std::string, std::string>& categories,
                                        double threshold = 0.7);

    // Verification
    LayaVerifyResult verify(const std::string& source,
                             const std::string& claim);

    // Safety guard
    LayaGuardResult guard(const std::string& text,
                           const std::map<std::string, std::string>& checks,
                           double threshold = 0.5);

    // Context pruning
    SelectRelevantResult select_relevant(const std::string& question,
                                          const std::vector<std::string>& items,
                                          const std::string& kind = "chunks",
                                          double min_p = 0.6);

    // Semantic dedup
    SameIntentResult same_intent(const std::string& a,
                                  const std::string& b,
                                  double threshold = 0.5);

    // Triage
    LayaTriageResult triage(const std::string& text,
                             const std::map<std::string, std::string>& routes,
                             const std::map<std::string, std::string>& tags,
                             const std::string& urgent_question = "Does this need a human within the hour?");

    // RAG gate
    AnswerableResult answerable(const std::string& question,
                                 const std::string& context,
                                 double threshold = 0.3);

    // Consult with memory
    LayaConsultResult consult(const std::string& question,
                               const std::map<std::string, std::string>& options,
                               const std::string& topic = "",
                               const std::string& context = "");

    // Full decision program
    DecideProgramResult decide_program(const std::string& program_json,
                                        const std::string& state,
                                        bool compact = true);

    // Token savers
    KeyPointsResult extract_key_points(const std::string& text,
                                        int max_points = 5);
    SummarizeResult summarize_text(const std::string& text,
                                    int max_tokens = 200);
    ExtractEntitiesResult extract_entities(const std::string& text);
    ContextHeadroomResult compute_headroom(int total_tokens,
                                             int used_tokens,
                                             double compaction_threshold = 0.85);
    std::vector<int> compact_messages(const std::vector<std::string>& messages,
                                       int max_tokens,
                                       double compaction_threshold = 0.85);

    // Cache management
    void clear_cache();
    size_t cache_size() const;

private:
    LayaConfig config;
    bool connected = false;
    std::map<std::string, std::string> cache;
    size_t cache_hits = 0;
    size_t cache_misses = 0;

    std::string send_request(const std::string& method,
                              const std::string& params_json);
    std::string get_cache_key(const std::string& method,
                               const std::string& params_json);
    bool is_cache_valid(const std::string& key);
};

// Factory function
std::unique_ptr<LayaClient> create_laya_client(const LayaConfig& config);

} // namespace laya
} // namespace mcp
} // namespace pluto
