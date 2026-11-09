// Mock Haiku libtranslation - minimal stubs for linking
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

extern "C" {
    void* _ZN15BTranslatorRosterC1Ev() { return malloc(1); }
    void _ZN15BTranslatorRosterD1Ev() {}
    void* _ZN15BTranslatorRoster9DefaultEv() { return nullptr; }
    int _ZN15BTranslatorRoster12IdentifyEP10BPositionIOPK16translator_infoiPi() { return -1; }
    int _ZN15BTranslatorRoster10TranslateEP10BPositionIOS1_P14BTranslatorP16translator_infoPK9BMessage() { return -1; }
    int _ZN15BTranslatorRoster11GetAllFormatsEP16translator_infoPi() { return -1; }
    int _ZN15BTranslatorRoster13GetInputFormatsEP16translator_infoPi() { return -1; }
    int _ZN15BTranslatorRoster14GetOutputFormatsEP16translator_infoPi() { return -1; }
    
    void* _ZN14BTranslatorC1Ev() { return malloc(1); }
    void _ZN14BTranslatorD1Ev() {}
    void* _ZN14BTranslator10NewConfigEv() { return nullptr; }
    void* _ZN14BTranslator14DeriveConfigEv() { return nullptr; }
    int _ZN14BTranslator14IdentifyEP10BPositionIOPK16translator_infoiPi() { return -1; }
    int _ZN14BTranslator10TranslateEP10BPositionIOS1_P16translator_infoPK9BMessageiPi() { return -1; }
    
    void* _ZN10BPositionIOC1Ev() { return malloc(1); }
    void _ZN10BPositionIOD1Ev() {}
    off_t _ZN10BPositionIO5SeekEli() { return -1; }
    off_t _ZN10BPositionIO6PositionEv() { return 0; }
    ssize_t _ZN10BPositionIO4ReadEPvj() { return -1; }
    ssize_t _ZN10BPositionIO5WriteEPKvj() { return -1; }
    int _ZN10BPositionIO8SetSizeEx() { return -1; }
    
    struct translator_info {
        int32_t type;
        int32_t group;
        int32_t quality;
        int32_t capability;
        char name[256];
        char info[256];
    };
}