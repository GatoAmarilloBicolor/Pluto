#ifndef MOCK_VOLUME_ROSTER_H
#define MOCK_VOLUME_ROSTER_H

#include "Types.h"
#include "Volume.h"

class BVolumeRoster {
public:
    BVolumeRoster() = default;
    ~BVolumeRoster() = default;
    
    status_t GetBootVolume(BVolume* volume) const { (void)volume; return B_ERROR; }
    status_t GetVolume(const char* name, BVolume* volume) const { (void)name; (void)volume; return B_ERROR; }
    status_t GetVolume(dev_t dev, BVolume* volume) const { (void)dev; (void)volume; return B_ERROR; }
    status_t GetNextVolume(BVolume* volume, dev_t* cookie) const { (void)volume; (void)cookie; return B_ERROR; }
    void Rewind() {}
};

#endif