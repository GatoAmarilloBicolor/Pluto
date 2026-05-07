#pragma once

// Thread-safe cooperative cancel flag used by the streaming provider to
// abort in-flight requests on SIGINT. Set by the signal handler, checked
// from curl's progress callback, cleared by the REPL before each turn.

namespace pluto {

void RequestCancel();
void ClearCancel();
bool IsCancelRequested();

}  // namespace pluto
