#ifndef MOCK_HASH_H
#define MOCK_HASH_H

#include "SupportDefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    B_HASH_MD5    = 0,
    B_HASH_SHA1   = 1,
    B_HASH_SHA256 = 2,
    B_HASH_SHA384 = 3,
    B_HASH_SHA512 = 4,
} hash_algorithm;

typedef struct {
    hash_algorithm algorithm;
    void* context;
} BHash;

status_t BHash_Init(BHash* hash, hash_algorithm algorithm);
status_t BHash_Update(BHash* hash, const void* data, size_t length);
status_t BHash_Final(BHash* hash, void* digest, size_t* digest_length);
void BHash_Free(BHash* hash);

// Convenience function
status_t BHash_Hash(hash_algorithm algorithm, const void* data, size_t length, void* digest, size_t* digest_length);

#ifdef __cplusplus
}
#endif

// C++ wrapper
#ifdef __cplusplus
class BHashCpp {
public:
    BHashCpp() : fHash() {}
    ~BHashCpp() { BHash_Free(&fHash); }
    
    status_t Init(hash_algorithm algorithm) { return BHash_Init(&fHash, algorithm); }
    status_t Update(const void* data, size_t length) { return BHash_Update(&fHash, data, length); }
    status_t Final(void* digest, size_t* digest_length) { return BHash_Final(&fHash, digest, digest_length); }
    
    static status_t Hash(hash_algorithm algorithm, const void* data, size_t length, void* digest, size_t* digest_length) {
        return BHash_Hash(algorithm, data, length, digest, digest_length);
    }
    
    static const hash_algorithm B_HASH_MD5    = B_HASH_MD5;
    static const hash_algorithm B_HASH_SHA1   = B_HASH_SHA1;
    static const hash_algorithm B_HASH_SHA256 = B_HASH_SHA256;
    static const hash_algorithm B_HASH_SHA384 = B_HASH_SHA384;
    static const hash_algorithm B_HASH_SHA512 = B_HASH_SHA512;

private:
    BHash fHash;
};

#endif

#endif // MOCK_HASH_H
