#ifndef MOCK_RANDOM_H
#define MOCK_RANDOM_H

#include "SupportDefs.h"

#ifdef __cplusplus
extern "C" {
#endif

status_t BRandom_GetRandomBytes(void* buffer, size_t length);
uint32_t BRandom_GetRandomInt(uint32_t max);
uint64_t BRandom_GetRandomInt64(uint64_t max);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class BRandom {
public:
    static status_t GetRandomBytes(void* buffer, size_t length) {
        return BRandom_GetRandomBytes(buffer, length);
    }
    
    static uint32_t GetRandomInt(uint32_t max) {
        return BRandom_GetRandomInt(max);
    }
    
    static uint64_t GetRandomInt64(uint64_t max) {
        return BRandom_GetRandomInt64(max);
    }
};

#endif

#endif // MOCK_RANDOM_H
