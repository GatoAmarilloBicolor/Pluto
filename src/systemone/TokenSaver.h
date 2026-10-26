#pragma once

#include <string>
#include <vector>
#include <map>
#include <cmath>

namespace pluto {
namespace systemone {

// ============================================================================
// TOKEN SAVER — Active token reduction for context windows
// ============================================================================

// Compute context window headroom and pressure.
// Returns recommended compaction if pressure exceeds threshold.
ContextHeadroomResult compute_context_headroom(
    int total_tokens,
    int used_tokens,
    double compaction_threshold = 0.85);

// Select which messages to compact based on importance scoring.
// Returns indices of messages that should be compacted/removed.
std::vector<int> select_messages_to_compact(
    const std::vector<std::string>& messages,
    const std::string& current_question,
    int max_tokens,
    double compaction_threshold = 0.85);

// Compress a single message by extracting key points.
std::string compress_message(
    const std::string& message,
    int max_tokens = 100);

// Extract key sentences from text using simple scoring.
std::string extract_key_sentences(
    const std::string& text,
    int max_sentences = 3);

// Compute compression ratio for a text.
double compute_compression_ratio(
    const std::string& original,
    const std::string& compressed);

// Estimate token count for a string (approximation: ~4 chars per token).
int estimate_tokens(const std::string& text);

// Estimate tokens saved by compressing a set of messages.
int estimate_tokens_saved(
    const std::vector<std::string>& original_messages,
    const std::vector<std::string>& compressed_messages);

// ============================================================================
// PROGRAMMABLE TOKEN SAVINGS — Declarative token-saving operations
// ============================================================================

// A single token-saving operation.
struct TokenSaveOp {
    std::string id;
    std::string op;  // "compress", "extract", "summarize", "prune", "rank"
    std::map<std::string, std::string> params;
    std::string when;  // condition to apply this op
    std::string target;  // target field/message index
};

// Result of a token-save operation.
struct TokenSaveResult {
    std::string id;
    std::string op;
    bool success = false;
    int tokens_saved = 0;
    int original_tokens = 0;
    int compressed_tokens = 0;
    std::string result_text;
    std::string error;
};

// Execute a declarative token-save program.
// Each op is executed in order; conditions are evaluated.
std::vector<TokenSaveResult> execute_token_save_program(
    const std::vector<TokenSaveOp>& ops,
    const std::map<std::string, std::string>& context);

// Optimize a context window by applying token-saving operations.
// Returns the optimized messages and total tokens saved.
struct ContextOptimizationResult {
    std::vector<std::string> optimized_messages;
    int total_tokens_saved = 0;
    int original_tokens = 0;
    int final_tokens = 0;
    std::vector<TokenSaveResult> op_results;
};

ContextOptimizationResult optimize_context_window(
    const std::vector<std::string>& messages,
    const std::string& current_question,
    int max_tokens,
    double compaction_threshold = 0.85);

}  // namespace systemone
}  // namespace pluto
