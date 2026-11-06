/*
 * File.h for Haiku compatibility
 * Minimal stub for Haiku BFile class
 */

#ifndef _FILE_H
#define _FILE_H

#include <Node.h>

#ifdef __cplusplus

class BFile : public BNode {
public:
    BFile();
    BFile(const BFile& file);
    BFile(const entry_ref* ref, uint32 openMode);
    BFile(const char* path, uint32 openMode);
    BFile(const BDirectory* dir, const char* path, uint32 openMode);
    virtual ~BFile();

    BFile& operator=(const BFile& file);

    status_t InitCheck() const;
    status_t SetTo(const BFile& file);
    status_t SetTo(const entry_ref* ref, uint32 openMode);
    status_t SetTo(const char* path, uint32 openMode);
    status_t SetTo(const BDirectory* dir, const char* path, uint32 openMode);

    status_t ReadAt(off_t pos, void* buffer, size_t size);
    status_t WriteAt(off_t pos, const void* buffer, size_t size);
    status_t Read(void* buffer, size_t size);
    status_t Write(const void* buffer, size_t size);
    status_t Seek(off_t position, uint32 seekMode = SEEK_SET);
    off_t Position() const;
    status_t GetSize(off_t* size) const;
    status_t SetSize(off_t size);
    status_t GetStat(struct stat* stat) const;
    status_t Sync();
};

#endif /* __cplusplus */

#endif /* _FILE_H */