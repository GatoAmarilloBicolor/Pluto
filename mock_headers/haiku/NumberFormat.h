#ifndef MOCK_NUMBERFORMAT_H
#define MOCK_NUMBERFORMAT_H

#include "Types.h"

class BNumberFormat {
public:
    BNumberFormat() = default;
    virtual ~BNumberFormat() = default;
    
    virtual void Format(BString* out, double number) const { (void)out; (void)number; }
    virtual void Format(BString* out, int64 number) const { (void)out; (void)number; }
    virtual void SetFormat(const char* format) { (void)format; }
    virtual const char* Format() const { return ""; }
};

#endif