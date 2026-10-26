#ifndef MOCK_NODE_INFO_H
#define MOCK_NODE_INFO_H

#include "Types.h"
#include "Node.h"

class BBitmap;

class BNodeInfo {
public:
    BNodeInfo() = default;
    BNodeInfo(const BNode* node) { (void)node; }
    ~BNodeInfo() = default;
    
    status_t GetType(char* type) const { (void)type; return B_ERROR; }
    status_t SetType(const char* type) { (void)type; return B_ERROR; }
    status_t GetPreferredApp(entry_ref* ref) const { (void)ref; return B_ERROR; }
    status_t SetPreferredApp(const entry_ref* ref) { (void)ref; return B_ERROR; }
    status_t GetTrackerIcon(BBitmap* icon, int32_t* size) const { (void)icon; (void)size; return B_ERROR; }
    status_t SetTrackerIcon(const BBitmap* icon, int32_t size) { (void)icon; (void)size; return B_ERROR; }
};

#endif