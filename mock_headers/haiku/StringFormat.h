#ifndef MOCK_STRING_FORMAT_H
#define MOCK_STRING_FORMAT_H

#include "Types.h"
#include "String.h"

class BStringFormat {
public:
    BStringFormat() = default;
    virtual ~BStringFormat() = default;
    
    virtual status_t Format(BString* into, const char* format, ...) = 0;
    virtual status_t Format(BString* into, const char* format, va_list args) = 0;
};

#include "NumberFormat.h"
#include "DateTimeFormat.h"

#endif