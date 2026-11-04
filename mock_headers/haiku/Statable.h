#ifndef MOCK_STATABLE_H
#define MOCK_STATABLE_H

#include "Types.h"

class BStatable {
public:
    BStatable() = default;
    virtual ~BStatable() = default;
    
    virtual status_t GetStat(struct stat* st) const = 0;
    virtual bool IsDirectory() const = 0;
    virtual bool IsFile() const = 0;
    virtual bool IsSymLink() const = 0;
    virtual off_t Size() const = 0;
    virtual time_t ModificationTime() const = 0;
    virtual time_t CreationTime() const = 0;
};

#endif
