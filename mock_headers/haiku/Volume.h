#ifndef MOCK_VOLUME_H
#define MOCK_VOLUME_H

#include <Entry.h>
#include <Node.h>

class BVolume {
public:
    BVolume() = default;
    BVolume(dev_t dev) { (void)dev; }
    ~BVolume() = default;
    
    dev_t Device() const { return 0; }
    bool IsReadOnly() const { return false; }
    bool IsPersistent() const { return true; }
    off_t FreeBytes() const { return 0; }
    off_t TotalBytes() const { return 0; }
    status_t GetName(char* name) const { (void)name; return B_OK; }
    
    bool KnowsQuery() const { return true; }
    bool KnowsAttr() const { return true; }
    bool KnowsMimeType() const { return true; }
};

struct fs_info {
    dev_t dev;
    char fsh_name[32];
    char vol_name[32];
    uint32_t flags;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
};

inline status_t fs_stat_dev(dev_t dev, struct fs_info* info) { (void)dev; (void)info; return B_OK; }

#endif
