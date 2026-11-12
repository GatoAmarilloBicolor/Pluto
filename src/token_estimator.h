// Token estimation utilities with model-specific encodings.
// Provides more accurate token counting than rough character-based estimates.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace pluto {

// Simple BPE-style token estimator for common model families.
// This is a lightweight approximation - for exact counts, use tiktoken library.
class TokenEstimator {
public:
    enum class Encoding {
        CL100K_BASE,    // GPT-4, GPT-3.5-turbo, text-embedding-ada-002
        O200K_BASE,     // GPT-4o, GPT-4o-mini
        P50K_BASE,      // Codex models, older GPT-3
        R50K_BASE,      // GPT-3 (davinci, curie, etc.)
        ANTHROPIC_CLAUDE, // Anthropic Claude models (approximation)
        LLAMA,          // Llama models (approximation)
        DEFAULT         // Fallback: ~3.5 chars/token
    };

    // Get encoding for a model name
    static Encoding encoding_for_model(const std::string& model) {
        std::string m = model;
        std::transform(m.begin(), m.end(), m.begin(), ::tolower);
        
        // OpenAI models
        if (m.find("gpt-4o") != std::string::npos) return Encoding::O200K_BASE;
        if (m.find("gpt-4") != std::string::npos) return Encoding::CL100K_BASE;
        if (m.find("gpt-3.5") != std::string::npos) return Encoding::CL100K_BASE;
        if (m.find("text-embedding") != std::string::npos) return Encoding::CL100K_BASE;
        if (m.find("codex") != std::string::npos) return Encoding::P50K_BASE;
        
        // Anthropic models (use cl100k as approximation)
        if (m.find("claude") != std::string::npos) return Encoding::ANTHROPIC_CLAUDE;
        
        // Llama models
        if (m.find("llama") != std::string::npos) return Encoding::LLAMA;
        
        // DeepSeek
        if (m.find("deepseek") != std::string::npos) return Encoding::CL100K_BASE;
        
        // Google/Gemini
        if (m.find("gemini") != std::string::npos) return Encoding::CL100K_BASE;
        
        return Encoding::DEFAULT;
    }

    // Estimate tokens for text using the specified encoding
    static int estimate(const std::string& text, Encoding encoding) {
        if (text.empty()) return 0;
        
        switch (encoding) {
            case Encoding::CL100K_BASE:
            case Encoding::O200K_BASE:
                return estimate_cl100k(text);
            case Encoding::P50K_BASE:
                return estimate_p50k(text);
            case Encoding::R50K_BASE:
                return estimate_r50k(text);
            case Encoding::ANTHROPIC_CLAUDE:
                return estimate_claude(text);
            case Encoding::LLAMA:
                return estimate_llama(text);
            case Encoding::DEFAULT:
            default:
                return estimate_default(text);
        }
    }

    // Estimate tokens for a message role (system, user, assistant)
    static int estimate_role(const std::string& role, Encoding encoding) {
        // Role tokens: "system", "user", "assistant" + formatting overhead
        static const std::unordered_map<std::string, int> role_tokens = {
            {"system", 3}, {"user", 2}, {"assistant", 2}, {"tool", 2}
        };
        auto it = role_tokens.find(role);
        if (it != role_tokens.end()) return it->second;
        return 3;
    }

    // Estimate tokens for tool use block
    static int estimate_tool_use(const std::string& name, const std::string& input, Encoding encoding) {
        // Tool name + input JSON + formatting overhead (~10-15 tokens)
        int tokens = estimate(name, encoding) + estimate(input, encoding) + 12;
        return tokens;
    }

    // Estimate tokens for tool result block
    static int estimate_tool_result(const std::string& output, Encoding encoding) {
        int tokens = estimate(output, encoding) + 3;
        return tokens;
    }

    // Estimate tokens for image (fixed cost based on detail level)
    static int estimate_image(const std::string& media_type, int detail = 0) {
        // Low detail: ~85 tokens, High detail: ~170-255 tokens
        // For simplicity, use fixed cost
        return 85;
    }

    // Estimate tokens for document
    static int estimate_document(const std::string& data, Encoding encoding) {
        // Base64 encoded document - estimate from decoded size
        return estimate(data, encoding) / 4 + 10;
    }

    // Estimate tokens for audio
    static int estimate_audio(const std::string& format) {
        // Rough estimate: ~50 tokens per audio file reference
        return 50;
    }

private:
    // Default: ~3.5 chars per token
    static int estimate_default(const std::string& text) {
        return static_cast<int>((text.size() + 2) / 3);
    }

    // cl100k_base approximation (GPT-4, GPT-3.5-turbo)
    // Based on common patterns: words ~1.3 tokens, code ~1 token per 3-4 chars
    static int estimate_cl100k(const std::string& text) {
        if (text.empty()) return 0;
        
        int tokens = 0;
        bool in_word = false;
        bool in_code = false;
        int code_depth = 0;
        
        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            
            // Detect code blocks
            if (c == '`') {
                code_depth++;
                in_code = (code_depth % 2 == 1);
                tokens++; // backtick is a token
                continue;
            }
            
            if (in_code) {
                // In code: ~1 token per 3-4 chars
                if (c == ' ' || c == '\t' || c == '\n') {
                    tokens++;
                } else if (is_punctuation(c)) {
                    tokens++;
                } else {
                    // Count alphanumeric sequences
                    size_t j = i;
                    while (j < text.size() && (isalnum(text[j]) || text[j] == '_')) j++;
                    int len = static_cast<int>(j - i);
                    tokens += (len + 3) / 4; // ~4 chars per token in code
                    i = j - 1;
                }
            } else {
                // Natural language: ~1.3 tokens per word
                if (is_word_char(c)) {
                    if (!in_word) {
                        in_word = true;
                        // Start of word
                        size_t j = i;
                        while (j < text.size() && is_word_char(text[j])) j++;
                        int len = static_cast<int>(j - i);
                        // Average word length ~5 chars, ~1.3 tokens
                        tokens += (len + 3) / 4;
                        i = j - 1;
                    }
                } else {
                    in_word = false;
                    if (c == ' ' || c == '\t' || c == '\n') {
                        tokens += 0.5; // whitespace often merges
                    } else if (is_punctuation(c)) {
                        tokens++; // punctuation often separate tokens
                    }
                }
            }
        }
        
        return std::max(1, tokens);
    }

    // p50k_base approximation (Codex, older models)
    static int estimate_p50k(const std::string& text) {
        // Similar to cl100k but slightly different vocab
        return estimate_cl100k(text);
    }

    // r50k_base approximation (GPT-3)
    static int estimate_r50k(const std::string& text) {
        // Larger vocab, fewer tokens for common words
        return static_cast<int>(estimate_cl100k(text) * 0.9);
    }

    // Anthropic Claude approximation
    // Claude uses a different tokenizer, roughly similar to cl100k
    static int estimate_claude(const std::string& text) {
        return estimate_cl100k(text);
    }

    // Llama approximation
    static int estimate_llama(const std::string& text) {
        // Llama tokenizer (SPM) - roughly similar to cl100k
        return estimate_cl100k(text);
    }

    static bool is_word_char(char c) {
        return isalnum(static_cast<unsigned char>(c)) || c == '\'' || c == '-';
    }

    static bool is_punctuation(char c) {
        return c == '.' || c == ',' || c == '!' || c == '?' || 
               c == ':' || c == ';' || c == '(' || c == ')' ||
               c == '[' || c == ']' || c == '{' || c == '}' ||
               c == '"' || c == '\'' || c == '/' || c == '\\' ||
               c == '|' || c == '@' || c == '#' || c == '$' ||
               c == '%' || c == '^' || c == '&' || c == '*' ||
               c == '+' || c == '=' || c == '<' || c == '>' ||
               c == '~' || c == '_';
    }
};

// Convenience function for provider-specific token estimation
inline int estimate_tokens_for_model(const std::string& model, const std::string& text) {
    return TokenEstimator::estimate(text, TokenEstimator::encoding_for_model(model));
}

inline int estimate_role_tokens_for_model(const std::string& model, const std::string& role) {
    return TokenEstimator::estimate_role(role, TokenEstimator::encoding_for_model(model));
}

inline int estimate_tool_use_for_model(const std::string& model, const std::string& name, const std::string& input) {
    return TokenEstimator::estimate_tool_use(name, input, TokenEstimator::encoding_for_model(model));
}

inline int estimate_tool_result_for_model(const std::string& model, const std::string& output) {
    return TokenEstimator::estimate_tool_result(output, TokenEstimator::encoding_for_model(model));
}

inline int estimate_image_tokens(const std::string& media_type, int detail = 0) {
    return TokenEstimator::estimate_image(media_type, detail);
}

inline int estimate_document_tokens(const std::string& model, const std::string& data) {
    return TokenEstimator::estimate_document(data, TokenEstimator::encoding_for_model(model));
}

inline int estimate_audio_tokens(const std::string& format) {
    return TokenEstimator::estimate_audio(format);
}

}  // namespace pluto