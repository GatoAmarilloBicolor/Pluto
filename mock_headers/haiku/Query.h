#ifndef MOCK_QUERY_H
#define MOCK_QUERY_H

#include "Types.h"
#include "Volume.h"
#include "Entry.h"
#include "Messenger.h"

class BQuery {
public:
    BQuery() = default;
    ~BQuery() = default;
    
    status_t SetVolume(const BVolume* vol) { (void)vol; return B_OK; }
    status_t SetPredicate(const char* pred) { (void)pred; return B_OK; }
    status_t Fetch() { return B_OK; }
    status_t Fetch(entry_ref* ref) { (void)ref; return B_ENTRY_NOT_FOUND; }
    status_t GetNextEntry(BEntry* entry) { (void)entry; return B_ENTRY_NOT_FOUND; }
    status_t GetNextRef(entry_ref* ref) { (void)ref; return B_ENTRY_NOT_FOUND; }
    status_t SetTarget(const BMessenger& target) { (void)target; return B_OK; }
    void Rewind() {}
    int32 CountEntries() { return 0; }
    void Clear() {}
};

const uint32_t B_QUERY_UPDATE = 'qup1';

#endif
