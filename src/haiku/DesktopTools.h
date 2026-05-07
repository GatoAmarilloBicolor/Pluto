#pragma once

#include <memory>

#include "../tools/Tool.h"

namespace pluto {

// System clipboard (BClipboard) — get and set plain text.
std::unique_ptr<Tool> MakeClipboardTool();

// Node monitor (BNodeMonitor) — watch a directory for create/modify/remove
// events for a configurable duration, then return the list. Useful for
// "linter on save" and "react to download" workflows.
std::unique_ptr<Tool> MakeNodeMonitorTool();

// Launch an application by MIME signature via be_roster.
std::unique_ptr<Tool> MakeLaunchAppTool();

// BMessage scripting — send scripting messages to other running Haiku apps.
// Allows Claude to query and manipulate windows, views, and properties of
// any application that implements the standard Be scripting protocol.
std::unique_ptr<Tool> MakeDesktopScriptTool();

}  // namespace pluto
