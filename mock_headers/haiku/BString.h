#ifndef MOCK_BSTRING_H
#define MOCK_BSTRING_H

#include "Types.h"
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cstring>

class BString {
public:
    BString() : fString(nullptr), fLength(0), fCapacity(0) {}
    BString(const char* str) : fString(nullptr), fLength(0), fCapacity(0) { SetTo(str); }
    BString(const char* str, size_t len) : fString(nullptr), fLength(0), fCapacity(0) { SetTo(str, len); }
    BString(const BString& other) : fString(nullptr), fLength(0), fCapacity(0) { SetTo(other.fString, other.fLength); }
    ~BString() { free(fString); }
    
    BString& operator=(const BString& other) {
        if (this != &other) {
            SetTo(other.fString, other.fLength);
        }
        return *this;
    }
    
    BString& operator=(const char* str) {
        SetTo(str);
        return *this;
    }
    
    const char* String() const { return fString ? fString : ""; }
    int32 Length() const { return fLength; }
    const char* c_str() const { return fString ? fString : ""; }
    
    BString& operator+=(const char* str) {
        if (str) {
            size_t len = std::strlen(str);
            EnsureCapacity(fLength + len + 1);
            std::strcpy(fString + fLength, str);
            fLength += len;
        }
        return *this;
    }
    
    BString& operator+=(const BString& str) {
        return operator+=(str.fString);
    }
    
    BString operator+(const char* str) const {
        BString r(*this);
        r += str;
        return r;
    }
    
    BString operator+(const BString& str) const {
        BString r(*this);
        r += str;
        return r;
    }
    
    friend BString operator+(const char* lhs, const BString& rhs) {
        BString r(lhs);
        r += rhs;
        return r;
    }
    
    // Stream operator (Haiku BString uses << for concatenation)
    template<typename T>
    BString& operator<<(const T& val) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%ld", static_cast<long>(val));
        return operator+=(buf);
    }
    
    // Explicit overloads for common types
    BString& operator<<(int val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", val);
        return operator+=(buf);
    }
    BString& operator<<(long val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", val);
        return operator+=(buf);
    }
    BString& operator<<(long long val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", val);
        return operator+=(buf);
    }
    BString& operator<<(unsigned val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", val);
        return operator+=(buf);
    }
    BString& operator<<(unsigned long val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu", val);
        return operator+=(buf);
    }
    BString& operator<<(unsigned long long val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", val);
        return operator+=(buf);
    }
    BString& operator<<(float val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%f", val);
        return operator+=(buf);
    }
    BString& operator<<(double val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%f", val);
        return operator+=(buf);
    }
    BString& operator<<(long double val) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%Lf", val);
        return operator+=(buf);
    }
    
    BString& operator<<(const char* str) { return operator+=(str); }
    BString& operator<<(const BString& str) { return operator+=(str); }
    
    int32 FindFirst(const char* str) const {
        if (!str || !fString) return -1;
        const char* found = std::strstr(fString, str);
        return found ? static_cast<int32>(found - fString) : -1;
    }
    
    BString& Remove(int32 start, int32 count) {
        if (start >= 0 && count > 0 && start < fLength && fString) {
            size_t s = start;
            size_t c = count;
            if (s + c > fLength) c = fLength - s;
            std::memmove(fString + s, fString + s + c, fLength - s - c + 1);
            fLength -= c;
        }
        return *this;
    }
    
    BString& SetToFormat(const char* format, ...) {
        va_list args;
        va_start(args, format);
        va_list args_copy;
        va_copy(args_copy, args);
        int len = vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);
        if (len >= 0) {
            EnsureCapacity(len + 1);
            vsnprintf(fString, fCapacity, format, args);
            fLength = len;
        }
        va_end(args);
        return *this;
    }
    
    void SetTo(const char* str, size_t len = SIZE_MAX) {
        if (!str) {
            SetTo("", 0);
            return;
        }
        if (len == SIZE_MAX) len = std::strlen(str);
        EnsureCapacity(len + 1);
        std::memcpy(fString, str, len);
        fString[len] = '\0';
        fLength = static_cast<int32>(len);
    }

private:
    void EnsureCapacity(size_t capacity) {
        if (capacity > static_cast<size_t>(fCapacity)) {
            size_t newCap = fCapacity ? fCapacity * 2 : 16;
            while (newCap < capacity) newCap *= 2;
            fString = static_cast<char*>(realloc(fString, newCap));
            fCapacity = static_cast<int32>(newCap);
        }
    }
    
    char* fString;
    int32 fLength;
    int32 fCapacity;
};

#endif // MOCK_BSTRING_H