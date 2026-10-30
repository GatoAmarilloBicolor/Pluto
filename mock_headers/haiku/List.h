#ifndef MOCK_LIST_H
#define MOCK_LIST_H

#include "Types.h"
#include <vector>
#include <algorithm>

// Non-templated BList for mock purposes
class BList {
public:
    BList() = default;
    ~BList() = default;
    
    int32 CountItems() const { return fItems.size(); }
    
    bool AddItem(void* item) { 
        fItems.push_back(item);
        return true;
    }
    
    bool AddItem(void* item, int32 index) {
        if (index < 0 || index >= (int32)fItems.size()) {
            fItems.push_back(item);
        } else {
            fItems.insert(fItems.begin() + index, item);
        }
        return true;
    }
    
    bool RemoveItem(void* item) {
        auto it = std::find(fItems.begin(), fItems.end(), item);
        if (it != fItems.end()) {
            fItems.erase(it);
            return true;
        }
        return false;
    }
    
    void* RemoveItem(int32 index) {
        if (index < 0 || index >= (int32)fItems.size()) return nullptr;
        void* item = fItems[index];
        fItems.erase(fItems.begin() + index);
        return item;
    }
    
    void* ItemAt(int32 index) const {
        if (index < 0 || index >= (int32)fItems.size()) return nullptr;
        return fItems[index];
    }
    
    int32 IndexOf(void* item) const {
        auto it = std::find(fItems.begin(), fItems.end(), item);
        if (it != fItems.end()) return it - fItems.begin();
        return -1;
    }
    
    void MakeEmpty() { fItems.clear(); }
    
    bool HasItem(void* item) const {
        return std::find(fItems.begin(), fItems.end(), item) != fItems.end();
    }

private:
    std::vector<void*> fItems;
};

#endif
