#ifndef MOCK_DATETIMEFORMAT_H
#define MOCK_DATETIMEFORMAT_H

#include "Types.h"

class BDateFormat {
public:
    BDateFormat() = default;
    virtual ~BDateFormat() = default;
    
    virtual void Format(BString* out, const tm* time) const { (void)out; (void)time; }
    virtual void Format(BString* out, time_t time) const { (void)out; (void)time; }
    virtual ssize_t Format(char* buffer, size_t size, time_t time,
                           BDateFormatStyle dateStyle, BTimeFormatStyle timeStyle) const {
        (void)buffer; (void)size; (void)time; (void)dateStyle; (void)timeStyle;
        return 0;
    }
    virtual void SetFormat(const char* format) { (void)format; }
    virtual const char* Format() const { return ""; }
};

class BDateTimeFormat {
public:
    BDateTimeFormat() = default;
    virtual ~BDateTimeFormat() = default;
    
    virtual void Format(BString* out, const tm* time) const { (void)out; (void)time; }
    virtual void Format(BString* out, time_t time) const { (void)out; (void)time; }
    virtual ssize_t Format(char* buffer, size_t size, time_t time,
                           BDateFormatStyle dateStyle, BTimeFormatStyle timeStyle) const {
        (void)buffer; (void)size; (void)time; (void)dateStyle; (void)timeStyle;
        return 0;
    }
    virtual void SetFormat(const char* format) { (void)format; }
    virtual const char* Format() const { return ""; }
};

#endif