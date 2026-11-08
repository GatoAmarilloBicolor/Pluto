#ifndef MOCK_REFERENCE_H
#define MOCK_REFERENCE_H

#include "Referenceable.h"

template<typename T>
class BReference {
public:
    BReference() : fObject(nullptr) {}
    BReference(T* obj, bool adopt = false) : fObject(obj) {
        if (obj && !adopt) obj->AcquireReference();
    }
    BReference(const BReference& other) : fObject(other.fObject) {
        if (fObject) fObject->AcquireReference();
    }
    ~BReference() { if (fObject) fObject->ReleaseReference(); }

    BReference& operator=(const BReference& other) {
        if (fObject != other.fObject) {
            if (fObject) fObject->ReleaseReference();
            fObject = other.fObject;
            if (fObject) fObject->AcquireReference();
        }
        return *this;
    }

    void SetTo(T* obj, bool adopt = false) {
        if (fObject) fObject->ReleaseReference();
        fObject = obj;
        if (obj && !adopt) obj->AcquireReference();
    }

    T* operator->() const { return fObject; }
    T& operator*() const { return *fObject; }
    T* Get() const { return fObject; }
    operator bool() const { return fObject != nullptr; }
    bool IsSet() const { return fObject != nullptr; }

private:
    T* fObject;
};

#endif