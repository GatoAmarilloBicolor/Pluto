/*
 * Directory.h for Haiku compatibility
 * Minimal stub for Haiku BDirectory class
 */

#ifndef _DIRECTORY_H
#define _DIRECTORY_H

#include <Entry.h>
#include <Node.h>

#ifdef __cplusplus

class BDirectory {
public:
    BDirectory();
    BDirectory(const BDirectory& dir);
    BDirectory(const char* path);
    BDirectory(const entry_ref* ref);
    BDirectory(const BEntry* entry);
    virtual ~BDirectory();

    BDirectory& operator=(const BDirectory& dir);

    status_t InitCheck() const;
    status_t SetTo(const BDirectory& dir);
    status_t SetTo(const char* path);
    status_t SetTo(const entry_ref* ref);
    status_t SetTo(const BEntry* entry);

    status_t GetEntry(BEntry* entry) const;
    status_t GetEntry(entry_ref* ref) const;
    status_t GetStat(struct stat* stat) const;
    status_t CreateDirectory(const char* name, BDirectory* newDir, mode_t perms = 0777);
    status_t CreateFile(const char* name, BFile* newFile, mode_t perms = 0666);
    status_t CreateSymLink(const char* name, const char* path, BSymLink* newLink);
    status_t Rename(const char* fromName, const char* toName);
    status_t Remove(const char* name);
    status_t RemoveEntry(const char* name);
    status_t FindEntry(const char* name, BEntry* entry) const;
    status_t FindRef(const char* name, entry_ref* ref) const;
    status_t FindPath(const char* name, char* path) const;
    status_t CountEntries();
    status_t Rewind();
    status_t GetNextEntry(BEntry* entry, bool traverse = false);
    status_t GetNextRef(entry_ref* ref);
    status_t GetNextDirents(void* buffer, size_t bufferSize, int32* count);
    status_t GetNextDirents(struct dirent* buffer, size_t bufferSize, int32* count);

    // Haiku-specific
    status_t GetStatFor(const char* name, struct stat* stat) const;
    status_t CreateDirectory(const char* name, BDirectory* newDir, mode_t perms);
    status_t CreateFile(const char* name, BFile* newFile, mode_t perms);
    status_t CreateSymLink(const char* name, const char* path, BSymLink* newLink);
    bool Contains(const BEntry* entry) const;
    bool Contains(const entry_ref* ref) const;
    bool Contains(const char* name) const;
};

#endif /* __cplusplus */

#endif /* _DIRECTORY_H */