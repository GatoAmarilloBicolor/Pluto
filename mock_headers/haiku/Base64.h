#ifndef MOCK_BASE64_H
#define MOCK_BASE64_H

#include "SupportDefs.h"
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

size_t BBase64_Encode(const void* input, size_t input_length, char* output, size_t output_size);
size_t BBase64_Decode(const char* input, size_t input_length, void* output, size_t output_size);

// URL-safe base64 (no padding, -_ instead of +/)
size_t BBase64_EncodeURL(const void* input, size_t input_length, char* output, size_t output_size);
size_t BBase64_DecodeURL(const char* input, size_t input_length, void* output, size_t output_size);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class BBase64 {
public:
    static std::string Encode(const void* input, size_t length) {
        size_t max_size = ((length + 2) / 3) * 4 + 1;
        std::string out;
        out.resize(max_size);
        size_t len = BBase64_Encode(input, length, &out[0], max_size);
        out.resize(len);
        return out;
    }
    
    static std::string Decode(const char* input, size_t length) {
        size_t max_size = (length * 3) / 4 + 1;
        std::string out;
        out.resize(max_size);
        size_t len = BBase64_Decode(input, length, &out[0], max_size);
        out.resize(len);
        return out;
    }
    
    static std::string EncodeURL(const void* input, size_t length) {
        size_t max_size = ((length + 2) / 3) * 4 + 1;
        std::string out;
        out.resize(max_size);
        size_t len = BBase64_EncodeURL(input, length, &out[0], max_size);
        out.resize(len);
        return out;
    }
    
    static std::string DecodeURL(const char* input, size_t length) {
        size_t max_size = (length * 3) / 4 + 1;
        std::string out;
        out.resize(max_size);
        size_t len = BBase64_DecodeURL(input, length, &out[0], max_size);
        out.resize(len);
        return out;
    }
    
    // Mock implementations for cross-compilation
    static size_t BBase64_Encode(const void* input, size_t input_length, char* output, size_t output_size) {
        // Simple mock implementation for cross-compilation
        return 0;
    }
    
    static size_t BBase64_Decode(const char* input, size_t input_length, void* output, size_t output_size) {
        return 0;
    }
    
    static size_t BBase64_EncodeURL(const void* input, size_t input_length, char* output, size_t output_size) {
        // Simple mock implementation for cross-compilation
        // Just copy the input as a placeholder
        size_t copy_len = std::min(input_length, output_size);
        memcpy(output, input, copy_len);
        return copy_len;
    }
    
    static size_t BBase64_DecodeURL(const char* input, size_t input_length, void* output, size_t output_size) {
        // Simple mock implementation for cross-compilation
        size_t copy_len = std::min(input_length, output_size);
        memcpy(output, input, copy_len);
        return copy_len;
    }
};

#endif

#endif // MOCK_BASE64_H
