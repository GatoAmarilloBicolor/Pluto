#ifndef MOCK_OBJECT_LIST_H
#define MOCK_OBJECT_LIST_H

#include "Types.h"
#include <vector>

template<typename T>
class BObjectList {
public:
    BObjectList() = default;
    ~BObjectList() = default;
    
    void AddItem(T* item) { fItems.push_back(item); }
    void RemoveItem(T* item) { 
        auto it = std::find(fItems.begin(), fItems.end(), item);
        if (it != fItems.end()) fItems.erase(it);
    }
    void RemoveItem(int32 index) { 
        if (index >= 0 && index < (int32)fItems.size()) fItems.erase(fItems.begin() + index); 
    }
    T* ItemAt(int32 index) const { 
        if (index >= 0 && index < (int32)fItems.size()) return fItems[index]; 
        return nullptr; 
    }
    int32 CountItems() const { return (int32)fItems.size(); }
    bool HasItem(T* item) const { return std::find(fItems.begin(), fItems.end(), item) != fItems.end(); }
    void MakeEmpty() { fItems.clear(); }
    
private:
    std::vector<T*> fItems;
};

#endif