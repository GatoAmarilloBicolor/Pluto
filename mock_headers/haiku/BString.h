#ifndef MOCK_STRING_H
#define MOCK_STRING_H

#include "Types.h"
#include <string>
#include <stdarg.h>

class BString {
public:
    BString() = default;
    BString(const char* str) : fString(str ? str : "") {}
    BString(const std::string& str) : fString(str) {}
    ~BString() = default;
    
    const char* String() const { return fString.c_str(); }
    int32 Length() const { return fString.length(); }
    
    BString& operator=(const char* str) { fString = str ? str : ""; return *this; }
    BString& operator=(const std::string& str) { fString = str; return *this; }
    
    BString& operator+=(const char* str) { fString += str ? str : ""; return *this; }
    BString& operator+=(const std::string& str) { fString += str; return *this; }
    BString& operator+=(const BString& str) { fString += str.fString; return *this; }
    
    int32 FindFirst(const char* str) const {
        size_t pos = fString.find(str ? str : "");
        return pos != std::string::npos ? pos : -1;
    }
    
    BString& Remove(int32 start, int32 count) {
        if (start >= 0 && count > 0 && start < (int32)fString.length()) {
            fString.erase(start, count);
        }
        return *this;
    }
    
    BString& SetToFormat(const char* format, ...) {
        va_list args;
        va_start(args, format);
        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        fString = buffer;
        return *this;
    }
    
private:
    std::string fString;
};

#endif