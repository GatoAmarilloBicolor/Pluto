// Tokenizer.cpp - Token estimation implementation

#include "tokenizer.h"
#include "provider/Provider.h"
#include <algorithm>
#include <cctype>

namespace pluto {

// Simple BPE-style token estimation
// Approximates cl100k_base (GPT-4) encoding behavior:
// - Common words ~1 token
// - Punctuation often separate tokens
// - Code symbols often separate tokens
// - ~3.5 chars/token for English prose
// - ~2.5 chars/token for code
int Tokenizer::estimate_bpe(const std::string& text) {
    if (text.empty()) return 0;

    int tokens = 0;
    bool in_word = false;
    
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        
        // Detect code-like patterns (each symbol typically a token)
        if (c == '{' || c == '}' || c == '[' || c == ']' || 
            c == '(' || c == ')' || c == '<' || c == '>' ||
            c == ';' || c == ':' || c == '=' || c == '+' || c == '-' ||
            c == '*' || c == '/' || c == '%' || c == '&' || c == '|' ||
            c == '^' || c == '~' || c == '`' || c == '@' || c == '#') {
            tokens++;
            in_word = false;
            continue;
        }
        
        if (std::isspace(static_cast<unsigned char>(c))) {
            in_word = false;
            continue;
        }
        
        // Word characters
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            if (!in_word) {
                tokens++;  // Start of new word
                in_word = true;
            }
        } else {
            // Other punctuation
            tokens++;
            in_word = false;
        }
    }
    
    // Minimum 1 token for non-empty
    return std::max(1, tokens);
}

int Tokenizer::estimate(const std::string& text) {
    // Use BPE-style estimation for better accuracy
    return estimate_bpe(text);
}

TokenCount Tokenizer::estimate_conversation(
    const std::string& system_prompt,
    const std::vector<std::string>& messages,
    const std::vector<std::string>& tool_schemas) {
    
    TokenCount tc;
    tc.system = estimate(system_prompt);
    
    for (const auto& msg : messages) {
        tc.messages += estimate(msg) + Tokenizer::kMessageOverhead;
    }
    
    for (const auto& schema : tool_schemas) {
        tc.tools += estimate(schema) + Tokenizer::kToolOverhead;
    }
    
    tc.overhead = static_cast<int>(messages.size()) * Tokenizer::kMessageOverhead;
    tc.total = tc.system + tc.messages + tc.tools + tc.overhead;
    
    return tc;
}

// Provider-aware token counting
int estimate_tokens_with_provider(const Provider& provider, const std::string& text) {
    return provider.estimate_tokens(text);
}

TokenCount estimate_conversation_tokens(
    const Provider& provider,
    const std::string& system_prompt,
    const std::vector<Message>& messages,
    const std::vector<ToolSpec>& tools) {
    
    TokenCount tc;
    
    // System prompt
    if (!system_prompt.empty()) {
        tc.system = provider.estimate_tokens(system_prompt);
    }
    
    // Messages
    for (const auto& m : messages) {
        // Role + content overhead
        tc.messages += Tokenizer::kMessageOverhead;
        
        for (const auto& block : m.content) {
            switch (block.type) {
                case ContentBlock::TEXT:
                    if (!block.text.empty())
                        tc.messages += provider.estimate_tokens(block.text);
                    break;
                case ContentBlock::TOOL_USE:
                    tc.messages += provider.estimate_tokens(block.tool_name) 
                                  + provider.estimate_tokens(block.tool_input)
                                  + Tokenizer::kToolOverhead;
                    break;
                case ContentBlock::TOOL_RESULT:
                    tc.messages += provider.estimate_tokens(block.text) + 2;
                    break;
                case ContentBlock::IMAGE:
                    tc.messages += 85;  // ~85 tokens for image (fixed cost)
                    break;
                case ContentBlock::DOCUMENT:
                    tc.messages += provider.estimate_tokens(block.doc_data) / 4 + 10;
                    break;
                case ContentBlock::AUDIO:
                    tc.messages += 50;  // Approximate
                    break;
            }
        }
    }
    
    // Tools/schemas
    for (const auto& tool : tools) {
        tc.tools += provider.estimate_tokens(tool.name)
                   + provider.estimate_tokens(tool.description)
                   + provider.estimate_tokens(tool.input_schema_json)
                   + Tokenizer::kToolOverhead;
    }
    
    tc.total = tc.system + tc.messages + tc.tools;
    return tc;
}

}  // namespace pluto
