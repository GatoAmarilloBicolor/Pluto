#ifndef MOCK_ROSTER_H
#define MOCK_ROSTER_H

#include "Types.h"
#include "Entry.h"
#include "Path.h"
#include "Message.h"
#include "Application.h"

#define B_MIME_TYPE_LENGTH 256

// app_info and BRoster are defined in Application.h

// Global be_roster pointer (Haiku API)
extern BRoster* be_roster;

#endif