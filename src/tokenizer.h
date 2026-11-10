// Tokenizer - Accurate token counting for context management.
// Provides provider-aware token estimation using tiktoken-like BPE approximation
// with fallback to provider's built-in estimate_tokens().

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace pluto {

// Token count result with breakdown
struct TokenCount {
    int total = 0;
    int system = 0;
    int messages = 0;
    int tools = 0;
    int overhead = 0;  // per-message overhead (~4 tokens)
};

// Simple BPE-style token estimation for when provider tokenizer unavailable
// Based on cl100k_base (GPT-4) encoding - reasonable approximation for most models
class Tokenizer {
public:
    Tokenizer() = default;
    ~Tokenizer() = default;

    // Estimate tokens for a single string
    static int estimate(const std::string& text);

    // Estimate tokens for a full conversation (system + messages + tools)
    static TokenCount estimate_conversation(
        const std::string& system_prompt,
        const std::vector<std::string>& messages,
        const std::vector<std::string>& tool_schemas = {});

    // Per-message overhead (role + formatting)
    static constexpr int kMessageOverhead = 4;
    static constexpr int kToolOverhead = 3;

private:
    // Rough BPE token estimation: ~3.5 chars/token for English
    // More accurate than 4 chars/token, handles code/punctuation better
    static int estimate_bpe(const std::string& text);
};

// Provider-aware token counter - delegates to provider's estimate_tokens()
// Falls back to Tokenizer::estimate() if provider unavailable
int estimate_tokens_with_provider(const class Provider& provider, const std::string& text);
TokenCount estimate_conversation_tokens(
    const class Provider& provider,
    const std::string& system_prompt,
    const std::vector<struct Message>& messages,
    const std::vector<struct ToolSpec>& tools);

}  // namespace pluto
