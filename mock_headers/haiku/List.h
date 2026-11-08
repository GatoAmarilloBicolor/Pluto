#ifndef MOCK_LIST_H
#define MOCK_LIST_H

#include "Types.h"
#include <vector>
#include <algorithm>

// Non-templated BList (Haiku original) - stores void* for backward compatibility
class BList {
public:
    BList() = default;
    ~BList() = default;
    
    void AddItem(void* item) { fItems.push_back(item); }
    void RemoveItem(void* item) { 
        auto it = std::find(fItems.begin(), fItems.end(), item);
        if (it != fItems.end()) fItems.erase(it);
    }
    void RemoveItem(int32 index) { 
        if (index >= 0 && index < (int32)fItems.size()) fItems.erase(fItems.begin() + index); 
    }
    void* ItemAt(int32 index) const { 
        if (index >= 0 && index < (int32)fItems.size()) return fItems[index]; 
        return nullptr; 
    }
    int32 CountItems() const { return (int32)fItems.size(); }
    bool HasItem(void* item) const { return std::find(fItems.begin(), fItems.end(), item) != fItems.end(); }
    void MakeEmpty() { fItems.clear(); }
    
private:
    std::vector<void*> fItems;
};

// Templated BList for type-safe usage
template<typename T>
class BListT : public BList {
public:
    using BList::AddItem;
    using BList::RemoveItem;
    using BList::ItemAt;
    using BList::HasItem;
    
    void AddItem(T* item) { BList::AddItem(item); }
    void RemoveItem(T* item) { BList::RemoveItem(item); }
    T* ItemAt(int32 index) const { return static_cast<T*>(BList::ItemAt(index)); }
    bool HasItem(T* item) const { return BList::HasItem(item); }
};

#endif