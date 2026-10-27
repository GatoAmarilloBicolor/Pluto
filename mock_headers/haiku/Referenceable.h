#ifndef MOCK_REFERENCEABLE_H
#define MOCK_REFERENCEABLE_H

#include "Types.h"

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