#ifndef MOCK_REFERENCEABLE_H
#define MOCK_REFERENCEABLE_H

#include "Types.h"

template<typename T>
class BReference {
public:
    BReference() : fObject(nullptr) {}
    BReference(T* obj) : fObject(obj) { if (fObject) fObject->AcquireReference(); }
    BReference(const BReference& other) : fObject(other.fObject) { if (fObject) fObject->AcquireReference(); }
    ~BReference() { if (fObject) fObject->ReleaseReference(); }
    
    BReference& operator=(const BReference& other) {
        if (fObject != other.fObject) {
            if (fObject) fObject->ReleaseReference();
            fObject = other.fObject;
            if (fObject) fObject->AcquireReference();
        }
        return *this;
    }
    
    void SetTo(T* obj, bool adopt = true) {
        if (fObject) fObject->ReleaseReference();
        fObject = obj;
        if (fObject && adopt) fObject->AcquireReference();
    }
    
    bool IsSet() const { return fObject != nullptr; }
    
    T* Get() const { return fObject; }
    T& operator*() const { return *fObject; }
    T* operator->() const { return fObject; }
    operator bool() const { return fObject != nullptr; }
    
private:
    T* fObject;
};

class BReferenceable {
public:
    BReferenceable() = default;
    virtual ~BReferenceable() = default;
    
    virtual void AcquireReference() const {}
    virtual void ReleaseReference() const {}
    virtual int32 CountReferences() const { return 0; }
};

#define BReferenceable BReferenceable

#endif