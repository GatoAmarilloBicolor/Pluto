#ifndef MOCK_SYSTEM_CATALOG_H
#define MOCK_SYSTEM_CATALOG_H

#include "Types.h"
#include "String.h"

class BSystemCatalog {
public:
    BSystemCatalog() = default;
    virtual ~BSystemCatalog() = default;
    
    virtual status_t GetString(const char* key, BString* into) const { 
        (void)key; if (into) into->SetTo(""); return B_OK; 
    }
    virtual status_t GetString(const char* key, const char* context, BString* into) const { 
        (void)key; (void)context; if (into) into->SetTo(""); return B_OK; 
    }
    virtual const char* Signature() const { return ""; }
};

// Global instance
extern BSystemCatalog* gSystemCatalog;

#endif