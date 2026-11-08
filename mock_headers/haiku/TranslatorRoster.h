#ifndef MOCK_TRANSLATOR_ROSTER_H
#define MOCK_TRANSLATOR_ROSTER_H

#include "Types.h"
#include "Bitmap.h"
#include "Message.h"
#include "DataIO.h"

struct translator_info {
    int32 type;
    const char* name;
    float quality;
    float capability;
};

class BBitmapStream;

class BTranslatorRoster {
public:
    static BTranslatorRoster* Default() { return nullptr; }
    
    virtual ~BTranslatorRoster() = default;
    
    virtual status_t Translate(BPositionIO* inSource,
                               const translator_info* onlyThisTranslator,
                               BMessage* settings,
                               BBitmap* outBitmap,
                               uint32 outFlags = 0) {
        (void)inSource; (void)onlyThisTranslator; (void)settings; (void)outBitmap; (void)outFlags;
        return B_ERROR;
    }
    
    // Overload for BBitmapStream output
    virtual status_t Translate(BPositionIO* inSource,
                               const translator_info* onlyThisTranslator,
                               BMessage* settings,
                               BBitmapStream* outStream,
                               uint32 outFlags = 0) {
        (void)inSource; (void)onlyThisTranslator; (void)settings; (void)outStream; (void)outFlags;
        return B_OK;
    }
    
    // Overload for BBitmapStream input to BPositionIO output (e.g., BMallocIO, BMemoryIO)
    virtual status_t Translate(BBitmapStream* inStream,
                               const translator_info* onlyThisTranslator,
                               BMessage* settings,
                               BPositionIO* outIO,
                               uint32 outFlags = 0) {
        (void)inStream; (void)onlyThisTranslator; (void)settings; (void)outIO; (void)outFlags;
        return B_OK;
    }
    
    // Overload with translator format string (e.g., "image/png")
    virtual status_t Translate(BBitmapStream* inStream,
                               const translator_info* onlyThisTranslator,
                               BMessage* settings,
                               BPositionIO* outIO,
                               const char* format) {
        (void)inStream; (void)onlyThisTranslator; (void)settings; (void)outIO; (void)format;
        return B_OK;
    }
    
    // Overload for BBitmapStream input to BFile output
    virtual status_t Translate(BBitmapStream* inStream,
                               const translator_info* onlyThisTranslator,
                               BMessage* settings,
                               BFile* outFile,
                               const char* outFormat) {
        (void)inStream; (void)onlyThisTranslator; (void)settings; (void)outFile; (void)outFormat;
        return B_OK;
    }
    
    virtual status_t Translate(const entry_ref* sourceRef,
                               BMessage* settings,
                               BBitmap* outBitmap,
                               uint32 outFlags = 0) {
        (void)sourceRef; (void)settings; (void)outBitmap; (void)outFlags;
        return B_ERROR;
    }
};

#endif