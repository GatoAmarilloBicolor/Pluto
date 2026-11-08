#ifndef MOCK_DATA_IO_H
#define MOCK_DATA_IO_H

#include "Types.h"

class BDataIO {
public:
    BDataIO() = default;
    virtual ~BDataIO() = default;
    
    virtual ssize_t Read(void* buffer, size_t size) { (void)buffer; (void)size; return 0; }
    virtual ssize_t Write(const void* buffer, size_t size) { (void)buffer; (void)size; return 0; }
    virtual off_t Seek(off_t position, uint32 seekMode) { (void)position; (void)seekMode; return 0; }
    virtual off_t Position() const { return 0; }
    virtual void SetSize(off_t size) { (void)size; }
};

class BPositionIO : public BDataIO {
public:
    BPositionIO() = default;
    virtual ~BPositionIO() = default;
};

class BMallocIO : public BPositionIO {
public:
    BMallocIO() = default;
    virtual ~BMallocIO() = default;
    void* Buffer() { return nullptr; }
    const void* Buffer() const { return nullptr; }
    off_t BufferLength() const { return 0; }
};

class BMemoryIO : public BPositionIO {
public:
    BMemoryIO() = default;
    BMemoryIO(const void* buffer, size_t size) { (void)buffer; (void)size; }
    virtual ~BMemoryIO() = default;
    void SetBuffer(const void* buffer, size_t size) { (void)buffer; (void)size; }
};

#endif