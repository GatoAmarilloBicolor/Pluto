// Mock Haiku libtracker - minimal stubs for linking
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

extern "C" {
    void* _ZN13BFilePanelC1Ev() { return malloc(1); }
    void _ZN13BFilePanelD1Ev() {}
    void _ZN13BFilePanel4ShowEv() {}
    void _ZN13BFilePanel4HideEv() {}
    void* _ZN13BFilePanel8PanelEv() { return nullptr; }
    void _ZN13BFilePanel10SetButtonLabelEPKc() {}
    void _ZN13BFilePanel10SetMessageEP7BMessage() {}
    void _ZN13BFilePanel11SetRefFilterEP10BRefFilter() {}
    void _ZN13BFilePanel12SetPanelDirectoryEP4BEntry() {}
    void* _ZN13BFilePanel14GetPanelDirectoryEP4BEntry() { return nullptr; }
    
    void* _ZN10BRefFilterC1Ev() { return malloc(1); }
    void _ZN10BRefFilterD1Ev() {}
    bool _ZN10BRefFilter10FilterRefEP6entry_ref() { return true; }
    
    struct entry_ref {
        dev_t device;
        ino_t directory;
        char name[256];
    };
    
    void* _ZN11BVolumeRosterC1Ev() { return malloc(1); }
    void _ZN11BVolumeRosterD1Ev() {}
    void* _ZN11BVolumeRoster9GetBootVolumeEP7BVolume() { return nullptr; }
    
    void* _ZN7BVolumeC1Ev() { return malloc(1); }
    void _ZN7BVolumeD1Ev() {}
    
    void* _ZN6BEntryC1EPKc() { return malloc(1); }
    void _ZN6BEntryD1Ev() {}
    bool _ZN6BEntry6ExistsEv() { return false; }
    void _ZN6BEntry7GetPathEP4BPath() {}
    void _ZN6BEntry8GetNameEPc() {}
    void _ZN6BEntry8GetRefEP10entry_ref() {}
    
    void* _ZN5BPathC1EPKc() { return malloc(1); }
    void _ZN5BPathD1Ev() {}
    void _ZN5BPath7AppendEPKc() {}
    const char* _ZN5BPath5PathEv() { return ""; }
    
    void* _ZN10BDirectoryC1EPKc() { return malloc(1); }
    void _ZN10BDirectoryD1Ev() {}
    int _ZN10BDirectory15GetNextRefsEP6BEntryPi() { return -1; }
    void _ZN10BDirectory6RewindEv() {}
    
    void* _ZN5BNodeC1EPKc() { return malloc(1); }
    void _ZN5BNodeD1Ev() {}
    int _ZN5BNode12ReadAttrEPKcjPvi() { return -1; }
    int _ZN5BNode13WriteAttrEPKcjPKvi() { return -1; }
    int _ZN5BNode10RemoveAttrEPKc() { return -1; }
    int _ZN5BNode9GetAttrInfoEPKcP11attr_info() { return -1; }
    
    struct attr_info {
        uint32_t type;
        off_t size;
    };
    
    void* _ZN7BRosterC1Ev() { return malloc(1); }
    void _ZN7BRosterD1Ev() {}
    int _ZN7BRoster8LaunchEPKc() { return -1; }
    int _ZN7BRoster12LaunchEP10entry_refP7BMessageiP7BMessageP8thread_id() { return -1; }
    
    void* _ZN7BQueryC1Ev() { return malloc(1); }
    void _ZN7BQueryD1Ev() {}
    void _ZN7BQuery10SetVolumeEP7BVolume() {}
    void _ZN7BQuery12SetPredicateEPKc() {}
    int _ZN7BQuery5FetchEv() { return -1; }
    int _ZN7BQuery10GetNextRefEP6BEntry() { return -1; }
}