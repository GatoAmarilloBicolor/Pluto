#ifndef MOCK_STRING_H
#define MOCK_STRING_H

#include "Types.h"
#include <string>

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
    
private:
    std::string fString;
};

#endif