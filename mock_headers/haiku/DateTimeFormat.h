#ifndef MOCK_DATETIMEFORMAT_H
#define MOCK_DATETIMEFORMAT_H

#include "Types.h"

class BDateTimeFormat {
public:
    BDateTimeFormat() = default;
    virtual ~BDateTimeFormat() = default;
    
    virtual void Format(BString* out, const tm* time) const { (void)out; (void)time; }
    virtual void Format(BString* out, time_t time) const { (void)out; (void)time; }
    virtual void SetFormat(const char* format) { (void)format; }
    virtual const char* Format() const { return ""; }
};

class BDateFormat {
public:
    BDateFormat() = default;
    virtual ~BDateFormat() = default;
    
    virtual void Format(BString* out, const tm* time) const { (void)out; (void)time; }
    virtual void Format(BString* out, time_t time) const { (void)out; (void)time; }
    virtual void SetFormat(const char* format) { (void)format; }
    virtual const char* Format() const { return ""; }
};

#endif