// Mock Haiku libnetwork - minimal stubs for linking
#include <cstdlib>
#include <cstddef>
#include <sys/types.h>

extern "C" {
    void* _ZN11BNetworkAddressC1Ev() { return malloc(1); }
    void* _ZN11BNetworkAddressC1EPKc() { return malloc(1); }
    void _ZN11BNetworkAddressD1Ev() {}
    void _ZN11BNetworkAddress8SetPortEj() {}
    unsigned short _ZN11BNetworkAddress4PortEv() { return 0; }
    void _ZN11BNetworkAddress8SetHostEPKc() {}
    const char* _ZN11BNetworkAddress4HostEv() { return ""; }
    
    void* _ZN10BNetEndpointC1Ev() { return malloc(1); }
    void _ZN10BNetEndpointD1Ev() {}
    int _ZN10BNetEndpoint5BindEP11BNetworkAddress() { return -1; }
    int _ZN10BNetEndpoint7ConnectEP11BNetworkAddress() { return -1; }
    long _ZN10BNetEndpoint5SendEPKvj() { return -1; }
    long _ZN10BNetEndpoint5RecvEPvj() { return -1; }
    
    void* _ZN10BNetBufferC1Ej() { return malloc(1); }
    void _ZN10BNetBufferD1Ev() {}
    void* _ZN10BNetBuffer5DataEv() { return nullptr; }
    size_t _ZN10BNetBuffer4SizeEv() { return 0; }
    void _ZN10BNetBuffer8SetSizeEj() {}
}