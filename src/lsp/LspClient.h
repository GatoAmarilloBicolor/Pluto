/*
 * Copyright 2026, Pluto Authors. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef LSP_CLIENT_H
#define LSP_CLIENT_H


#include <string>
#include <vector>

#include "../json.h"


namespace pluto {


// Result of an LSP query (definition, hover, references, etc.).
struct LspLocation {
	std::string		uri;
	int32_t			line = 0;		// 0-based
	int32_t			character = 0;	// 0-based
	int32_t			endLine = 0;
	int32_t			endCharacter = 0;
};


struct LspHoverResult {
	std::string		contents;		// markdown or plain text
	bool			empty = true;
};


struct LspDiagnostic {
	int32_t			line = 0;
	int32_t			character = 0;
	int32_t			severity = 0;	// 1=error, 2=warning, 3=info, 4=hint
	std::string		message;
	std::string		source;
};


struct LspSymbol {
	std::string		name;
	std::string		kind;			// "function", "class", "variable", ...
	int32_t			line = 0;
};


// Minimal LSP client that spawns a language server as a subprocess
// and communicates via JSON-RPC 2.0 over stdin/stdout with
// Content-Length framing.
//
// Lifecycle: Start() → Initialize() → DidOpen() → queries → Shutdown()
// The client is single-threaded; all calls block until a response arrives.
class LspClient {
public:
							LspClient();
							~LspClient();

	// Spawn the language server process. Returns false on error.
	bool					Start(const std::string& command,
								const std::vector<std::string>& args,
								std::string& error);

	// Send initialize request with workspace root. Must be called
	// after Start() and before any document operations.
	bool					Initialize(const std::string& rootUri,
								std::string& error);

	// Notify the server that a file has been opened. Content is read
	// from disk. Must be called before querying a file.
	bool					DidOpen(const std::string& filePath,
								std::string& error);

	// Notify the server that a file has been closed.
	void					DidClose(const std::string& filePath);

	// Send shutdown request and exit notification. Safe to call
	// multiple times.
	void					Shutdown();

	// --- Query operations (require DidOpen first) ---

	LspHoverResult			Hover(const std::string& filePath,
								int32_t line, int32_t character,
								std::string& error);

	std::vector<LspLocation> GoToDefinition(const std::string& filePath,
								int32_t line, int32_t character,
								std::string& error);

	std::vector<LspLocation> FindReferences(const std::string& filePath,
								int32_t line, int32_t character,
								std::string& error);

	std::vector<LspDiagnostic> GetDiagnostics(const std::string& filePath,
								std::string& error);

	std::vector<LspSymbol>	DocumentSymbols(const std::string& filePath,
								std::string& error);

	bool					IsRunning() const { return fPid > 0; }

private:
	// JSON-RPC over Content-Length framing.
	bool					_SendRequest(const json::Value& request,
								std::string& error);
	bool					_SendNotification(const json::Value& notification);
	json::Value				_ReadResponse(int32_t expectedId,
								int timeoutMs, std::string& error);

	// Low-level I/O.
	bool					_WriteAll(const char* data, size_t length);
	bool					_ReadBytes(size_t count, std::string& out,
								int timeoutMs);
	bool					_ReadLine(std::string& line, int timeoutMs);

	// Helpers.
	json::Value				_BuildTextDocumentPosition(
								const std::string& filePath,
								int32_t line, int32_t character);
	std::string				_FileUri(const std::string& path);
	std::string				_DetectLanguageId(const std::string& path);

	int32_t					_NextId() { return fNextId++; }

private:
	pid_t					fPid = -1;
	int						fWriteFd = -1;
	int						fReadFd = -1;
	int32_t					fNextId = 1;
	bool					fInitialized = false;
	std::string				fReadBuffer;

	// Track opened files to avoid duplicate didOpen.
	std::vector<std::string> fOpenFiles;
};


}	// namespace pluto

// LSP integration complete
#endif	// LSP_CLIENT_H
