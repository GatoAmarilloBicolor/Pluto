#ifndef MOCK_ARCHIVABLE_H
#define MOCK_ARCHIVABLE_H

#include "Types.h"
#include "Message.h"

class BArchivable {
public:
    BArchivable() = default;
    virtual ~BArchivable() = default;
    
    virtual status_t Archive(BMessage* archive, bool deep = true) const { (void)archive; (void)deep; return B_OK; }
    static BArchivable* Instantiate(BMessage* archive) { (void)archive; return nullptr; }
};

#endif