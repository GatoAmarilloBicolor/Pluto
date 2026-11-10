#ifndef MOCK_ARCHIVABLE_H
#define MOCK_ARCHIVABLE_H

#include "Types.h"
#include "Message.h"

// Global function for validating archived object instantiation
bool validate_instantiation(BMessage* archive, const char* className);

#endif