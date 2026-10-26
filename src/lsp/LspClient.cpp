/*
 * Copyright 2026, Pluto Authors. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "LspClient.h"

#ifdef __HAIKU__
#include <image.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>

#include "../log.h"


namespace pluto {


// --- Construction / destruction ---


LspClient::LspClient()
{
}


LspClient::~LspClient()
{
	Shutdown();
}


// --- Process lifecycle ---


bool
LspClient::Start(const std::string& command,
	const std::vector<std::string>& args, std::string& error)
{
	if (command.empty()) {
		error = "command required";
		return false;
	}

	// Create pipes: inPipe for child's stdin, outPipe for child's stdout.
	int inPipe[2], outPipe[2];
	if (pipe(inPipe) != 0) {
		error = strerror(errno);
		return false;
	}
	if (pipe(outPipe) != 0) {
		int savedErrno = errno;
		close(inPipe[0]);
		close(inPipe[1]);
		error = strerror(savedErrno);
		return false;
	}

	// Mark parent-side pipe ends as close-on-exec.
	fcntl(inPipe[1], F_SETFD, FD_CLOEXEC);
	fcntl(outPipe[0], F_SETFD, FD_CLOEXEC);

	// Redirect stdin/stdout for the child via load_image().
	// Save originals so we can restore after load_image().
	int savedStdin = dup(STDIN_FILENO);
	int savedStdout = dup(STDOUT_FILENO);
	int savedStderr = dup(STDERR_FILENO);
	fcntl(savedStdin, F_SETFD, FD_CLOEXEC);
	fcntl(savedStdout, F_SETFD, FD_CLOEXEC);
	fcntl(savedStderr, F_SETFD, FD_CLOEXEC);

	dup2(inPipe[0], STDIN_FILENO);
	dup2(outPipe[1], STDOUT_FILENO);

	// Redirect child's stderr to /dev/null.
	int devNull = open("/dev/null", O_WRONLY);
	if (devNull >= 0) {
		dup2(devNull, STDERR_FILENO);
		close(devNull);
	}

	close(inPipe[0]);
	close(outPipe[1]);

	// Build argv array for load_image().
	std::vector<const char*> argv;
	argv.push_back(command.c_str());
	for (const auto& a : args)
		argv.push_back(a.c_str());
	argv.push_back(NULL);

	// Spawn using Haiku's native load_image() API.
	thread_id child = load_image(
		static_cast<int32>(argv.size() - 1),
		argv.data(),
		const_cast<const char**>(environ));

	// Restore parent's stdin/stdout/stderr immediately.
	dup2(savedStdin, STDIN_FILENO);
	dup2(savedStdout, STDOUT_FILENO);
	dup2(savedStderr, STDERR_FILENO);
	close(savedStdin);
	close(savedStdout);
	close(savedStderr);

	if (child < 0) {
		close(inPipe[1]);
		close(outPipe[0]);
		error = strerror(child);
		return false;
	}

	// Resume the child thread.
	resume_thread(child);

	fWriteFd = inPipe[1];
	fReadFd = outPipe[0];
	fPid = child;

	// Non-blocking reads so poll() works properly.
	int flags = fcntl(fReadFd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(fReadFd, F_SETFL, flags | O_NONBLOCK);

	log_debug("lsp", "started %s (pid %d)", command.c_str(), (int)child);
	return true;
}


bool
LspClient::Initialize(const std::string& rootUri, std::string& error)
{
	if (fPid < 0) {
		error = "server not started";
		return false;
	}

	int32_t id = _NextId();

	json::Object params;
	params["processId"] = json::Value((double)getpid());
	params["rootUri"] = json::Value(rootUri);

	// Client capabilities — request what we need.
	json::Object capabilities;

	json::Object textDocument;
	// Hover
	json::Object hover;
	hover["contentFormat"] = json::Value(json::Array{
		json::Value("markdown"), json::Value("plaintext")});
	textDocument["hover"] = json::Value(std::move(hover));
	// Definition
	textDocument["definition"] = json::Value(json::Object{});
	// References
	textDocument["references"] = json::Value(json::Object{});
	// Document symbols
	textDocument["documentSymbol"] = json::Value(json::Object{});
	// Diagnostics (published by server as notifications)
	textDocument["publishDiagnostics"] = json::Value(json::Object{});

	capabilities["textDocument"] = json::Value(std::move(textDocument));
	params["capabilities"] = json::Value(std::move(capabilities));

	json::Object request;
	request["jsonrpc"] = json::Value("2.0");
	request["id"] = json::Value(id);
	request["method"] = json::Value("initialize");
	request["params"] = json::Value(std::move(params));

	if (!_SendRequest(json::Value(std::move(request)), error))
		return false;

	json::Value response = _ReadResponse(id, 30000, error);
	if (response.is_null())
		return false;

	// Send initialized notification.
	json::Object initialized;
	initialized["jsonrpc"] = json::Value("2.0");
	initialized["method"] = json::Value("initialized");
	initialized["params"] = json::Value(json::Object{});
	_SendNotification(json::Value(std::move(initialized)));

	fInitialized = true;
	log_debug("lsp", "initialized (root=%s)", rootUri.c_str());
	return true;
}


bool
LspClient::DidOpen(const std::string& filePath, std::string& error)
{
	if (!fInitialized) {
		error = "not initialized";
		return false;
	}

	// Already open?
	for (const auto& f : fOpenFiles) {
		if (f == filePath)
			return true;
	}

	// Read file content from disk.
	std::ifstream file(filePath);
	if (!file) {
		error = "cannot read " + filePath;
		return false;
	}
	std::ostringstream content;
	content << file.rdbuf();

	json::Object textDocument;
	textDocument["uri"] = json::Value(_FileUri(filePath));
	textDocument["languageId"] = json::Value(_DetectLanguageId(filePath));
	textDocument["version"] = json::Value(1);
	textDocument["text"] = json::Value(content.str());

	json::Object params;
	params["textDocument"] = json::Value(std::move(textDocument));

	json::Object notification;
	notification["jsonrpc"] = json::Value("2.0");
	notification["method"] = json::Value("textDocument/didOpen");
	notification["params"] = json::Value(std::move(params));

	_SendNotification(json::Value(std::move(notification)));
	fOpenFiles.push_back(filePath);

	log_debug("lsp", "didOpen %s", filePath.c_str());
	return true;
}


void
LspClient::DidClose(const std::string& filePath)
{
	auto it = std::find(fOpenFiles.begin(), fOpenFiles.end(), filePath);
	if (it == fOpenFiles.end())
		return;

	json::Object textDocument;
	textDocument["uri"] = json::Value(_FileUri(filePath));

	json::Object params;
	params["textDocument"] = json::Value(std::move(textDocument));

	json::Object notification;
	notification["jsonrpc"] = json::Value("2.0");
	notification["method"] = json::Value("textDocument/didClose");
	notification["params"] = json::Value(std::move(params));

	_SendNotification(json::Value(std::move(notification)));
	fOpenFiles.erase(it);
}


void
LspClient::Shutdown()
{
	if (fPid < 0)
		return;

	if (fInitialized) {
		// Send shutdown request.
		int32_t id = _NextId();
		json::Object request;
		request["jsonrpc"] = json::Value("2.0");
		request["id"] = json::Value(id);
		request["method"] = json::Value("shutdown");
		request["params"] = json::Value(nullptr);

		std::string error;
		if (_SendRequest(json::Value(std::move(request)), error))
			_ReadResponse(id, 5000, error);

		// Send exit notification.
		json::Object exitNotif;
		exitNotif["jsonrpc"] = json::Value("2.0");
		exitNotif["method"] = json::Value("exit");
		_SendNotification(json::Value(std::move(exitNotif)));

		fInitialized = false;
	}

	// Close pipes and wait for process.
	if (fWriteFd >= 0) {
		close(fWriteFd);
		fWriteFd = -1;
	}
	if (fReadFd >= 0) {
		close(fReadFd);
		fReadFd = -1;
	}

	if (fPid > 0) {
		// Wait for the child to exit gracefully after shutdown/exit.
		status_t dummy;
		for (int i = 0; i < 50; i++) {
			pid_t result = waitpid(fPid, &dummy, WNOHANG);
			if (result == fPid || result < 0) {
				fPid = -1;
				return;
			}
			usleep(10000);
		}
		// Force kill if still alive.
		kill(fPid, SIGKILL);
		waitpid(fPid, &dummy, 0);
		fPid = -1;
	}

	fOpenFiles.clear();
	fReadBuffer.clear();
}


// --- Query operations ---


LspHoverResult
LspClient::Hover(const std::string& filePath, int32_t line,
	int32_t character, std::string& error)
{
	LspHoverResult result;

	if (!DidOpen(filePath, error))
		return result;

	int32_t id = _NextId();
	json::Object request;
	request["jsonrpc"] = json::Value("2.0");
	request["id"] = json::Value(id);
	request["method"] = json::Value("textDocument/hover");
	request["params"] = _BuildTextDocumentPosition(filePath, line, character);

	if (!_SendRequest(json::Value(std::move(request)), error))
		return result;

	json::Value response = _ReadResponse(id, 10000, error);
	if (response.is_null())
		return result;

	const json::Value* responseResult = response.find("result");
	if (responseResult == NULL || responseResult->is_null())
		return result;

	const json::Value* contents = responseResult->find("contents");
	if (contents == NULL)
		return result;

	result.empty = false;
	if (contents->is_str()) {
		result.contents = contents->as_str();
	} else if (contents->is_obj()) {
		// MarkedString or MarkupContent
		result.contents = contents->str("value");
	} else if (contents->is_arr()) {
		// Array of MarkedString
		for (const auto& item : contents->as_arr()) {
			if (!result.contents.empty())
				result.contents += "\n---\n";
			if (item.is_str())
				result.contents += item.as_str();
			else if (item.is_obj())
				result.contents += item.str("value");
		}
	}

	return result;
}


std::vector<LspLocation>
LspClient::GoToDefinition(const std::string& filePath, int32_t line,
	int32_t character, std::string& error)
{
	std::vector<LspLocation> locations;

	if (!DidOpen(filePath, error))
		return locations;

	int32_t id = _NextId();
	json::Object request;
	request["jsonrpc"] = json::Value("2.0");
	request["id"] = json::Value(id);
	request["method"] = json::Value("textDocument/definition");
	request["params"] = _BuildTextDocumentPosition(filePath, line, character);

	if (!_SendRequest(json::Value(std::move(request)), error))
		return locations;

	json::Value response = _ReadResponse(id, 10000, error);
	if (response.is_null())
		return locations;

	const json::Value* responseResult = response.find("result");
	if (responseResult == NULL || responseResult->is_null())
		return locations;

	// Result can be a single Location or an array.
	auto parseLocation = [](const json::Value& value) -> LspLocation {
		LspLocation location;
		location.uri = value.str("uri");
		const json::Value* range = value.find("range");
		if (range != NULL) {
			const json::Value* start = range->find("start");
			if (start != NULL) {
				const json::Value* lineVal = start->find("line");
				const json::Value* charVal = start->find("character");
				if (lineVal != NULL && lineVal->is_num())
					location.line = static_cast<int32_t>(lineVal->as_num());
				if (charVal != NULL && charVal->is_num())
					location.character = static_cast<int32_t>(charVal->as_num());
			}
		}
		return location;
	};

	if (responseResult->is_arr()) {
		for (const auto& item : responseResult->as_arr())
			locations.push_back(parseLocation(item));
	} else if (responseResult->is_obj()) {
		locations.push_back(parseLocation(*responseResult));
	}

	return locations;
}


std::vector<LspLocation>
LspClient::FindReferences(const std::string& filePath, int32_t line,
	int32_t character, std::string& error)
{
	std::vector<LspLocation> locations;

	if (!DidOpen(filePath, error))
		return locations;

	int32_t id = _NextId();

	json::Object context;
	context["includeDeclaration"] = json::Value(true);

	json::Value params = _BuildTextDocumentPosition(filePath, line, character);
	params.set("context", json::Value(std::move(context)));

	json::Object request;
	request["jsonrpc"] = json::Value("2.0");
	request["id"] = json::Value(id);
	request["method"] = json::Value("textDocument/references");
	request["params"] = std::move(params);

	if (!_SendRequest(json::Value(std::move(request)), error))
		return locations;

	json::Value response = _ReadResponse(id, 15000, error);
	if (response.is_null())
		return locations;

	const json::Value* responseResult = response.find("result");
	if (responseResult == NULL || !responseResult->is_arr())
		return locations;

	for (const auto& item : responseResult->as_arr()) {
		LspLocation location;
		location.uri = item.str("uri");
		const json::Value* range = item.find("range");
		if (range != NULL) {
			const json::Value* start = range->find("start");
			if (start != NULL) {
				const json::Value* lineVal = start->find("line");
				const json::Value* charVal = start->find("character");
				if (lineVal != NULL && lineVal->is_num())
					location.line = static_cast<int32_t>(lineVal->as_num());
				if (charVal != NULL && charVal->is_num())
					location.character = static_cast<int32_t>(charVal->as_num());
			}
		}
		locations.push_back(std::move(location));
	}

	return locations;
}


std::vector<LspDiagnostic>
LspClient::GetDiagnostics(const std::string& filePath, std::string& error)
{
	std::vector<LspDiagnostic> diagnostics;

	if (!DidOpen(filePath, error))
		return diagnostics;

	// Diagnostics are pushed via notifications. We read any pending
	// notifications and collect diagnostics for our file.
	// Give the server a moment to publish after didOpen.
	usleep(500000);

	// Drain pending messages looking for publishDiagnostics.
	std::string uri = _FileUri(filePath);
	for (int attempt = 0; attempt < 5; attempt++) {
		std::string line;
		if (!_ReadLine(line, 200))
			break;

		if (line.empty())
			continue;

		// Parse Content-Length header.
		if (line.rfind("Content-Length:", 0) == 0) {
			int contentLength = atoi(line.c_str() + 15);
			std::string emptyLine;
			_ReadLine(emptyLine, 200);  // consume \r\n

			std::string body;
			if (!_ReadBytes(contentLength, body, 5000))
				break;

			try {
				json::Value message = json::Value::parse(body);
				if (message.str("method") == "textDocument/publishDiagnostics") {
					const json::Value* params = message.find("params");
					if (params != NULL && params->str("uri") == uri) {
						const json::Value* diags = params->find("diagnostics");
						if (diags != NULL && diags->is_arr()) {
							for (const auto& d : diags->as_arr()) {
								LspDiagnostic diag;
								diag.message = d.str("message");
								diag.source = d.str("source");
								const json::Value* severity = d.find("severity");
								if (severity != NULL && severity->is_num())
									diag.severity = static_cast<int32_t>(
										severity->as_num());
								const json::Value* range = d.find("range");
								if (range != NULL) {
									const json::Value* start = range->find("start");
									if (start != NULL) {
										const json::Value* l = start->find("line");
										const json::Value* c = start->find("character");
										if (l != NULL && l->is_num())
											diag.line = static_cast<int32_t>(l->as_num());
										if (c != NULL && c->is_num())
											diag.character = static_cast<int32_t>(
												c->as_num());
									}
								}
								diagnostics.push_back(std::move(diag));
							}
						}
					}
				}
			} catch (const std::exception&) {
				// Malformed message — skip.
			}
		}
	}

	return diagnostics;
}


std::vector<LspSymbol>
LspClient::DocumentSymbols(const std::string& filePath, std::string& error)
{
	std::vector<LspSymbol> symbols;

	if (!DidOpen(filePath, error))
		return symbols;

	int32_t id = _NextId();

	json::Object textDocument;
	textDocument["uri"] = json::Value(_FileUri(filePath));

	json::Object params;
	params["textDocument"] = json::Value(std::move(textDocument));

	json::Object request;
	request["jsonrpc"] = json::Value("2.0");
	request["id"] = json::Value(id);
	request["method"] = json::Value("textDocument/documentSymbol");
	request["params"] = json::Value(std::move(params));

	if (!_SendRequest(json::Value(std::move(request)), error))
		return symbols;

	json::Value response = _ReadResponse(id, 10000, error);
	if (response.is_null())
		return symbols;

	const json::Value* responseResult = response.find("result");
	if (responseResult == NULL || !responseResult->is_arr())
		return symbols;

	// Symbol kind number → name mapping (LSP spec).
	auto kindName = [](int kind) -> std::string {
		switch (kind) {
			case 1:  return "file";
			case 2:  return "module";
			case 3:  return "namespace";
			case 5:  return "class";
			case 6:  return "method";
			case 7:  return "property";
			case 8:  return "field";
			case 9:  return "constructor";
			case 10: return "enum";
			case 11: return "interface";
			case 12: return "function";
			case 13: return "variable";
			case 14: return "constant";
			case 23: return "struct";
			case 26: return "type_parameter";
			default: return "symbol";
		}
	};

	// Flatten DocumentSymbol tree (recursive children).
	std::function<void(const json::Array&)> flatten;
	flatten = [&](const json::Array& items) {
		for (const auto& item : items) {
			LspSymbol symbol;
			symbol.name = item.str("name");
			const json::Value* kindVal = item.find("kind");
			if (kindVal != NULL && kindVal->is_num())
				symbol.kind = kindName(static_cast<int>(kindVal->as_num()));
			else
				symbol.kind = "symbol";
			const json::Value* range = item.find("range");
			if (range == NULL)
				range = item.find("location");
			if (range != NULL) {
				const json::Value* rangeObj = range->find("range");
				if (rangeObj != NULL)
					range = rangeObj;
				const json::Value* start = range->find("start");
				if (start != NULL) {
					const json::Value* lineVal = start->find("line");
					if (lineVal != NULL && lineVal->is_num())
						symbol.line = static_cast<int32_t>(lineVal->as_num());
				}
			}
			symbols.push_back(std::move(symbol));

			// Recurse into children (DocumentSymbol has nested children).
			const json::Value* children = item.find("children");
			if (children != NULL && children->is_arr())
				flatten(children->as_arr());
		}
	};

	flatten(responseResult->as_arr());
	return symbols;
}


// --- JSON-RPC with Content-Length framing ---


bool
LspClient::_SendRequest(const json::Value& request, std::string& error)
{
	std::string body = request.serialize();
	std::string frame = "Content-Length: " + std::to_string(body.size())
		+ "\r\n\r\n" + body;

	if (!_WriteAll(frame.c_str(), frame.size())) {
		error = "write failed";
		return false;
	}
	return true;
}


bool
LspClient::_SendNotification(const json::Value& notification)
{
	std::string body = notification.serialize();
	std::string frame = "Content-Length: " + std::to_string(body.size())
		+ "\r\n\r\n" + body;
	return _WriteAll(frame.c_str(), frame.size());
}


json::Value
LspClient::_ReadResponse(int32_t expectedId, int timeoutMs, std::string& error)
{
	// Read messages until we find one with our id.
	// Skip notifications (no id).
	for (int attempt = 0; attempt < 20; attempt++) {
		std::string headerLine;
		if (!_ReadLine(headerLine, timeoutMs)) {
			error = "timeout waiting for response header";
			return json::Value(nullptr);
		}

		// Skip empty lines.
		if (headerLine.empty())
			continue;

		// Parse Content-Length.
		if (headerLine.rfind("Content-Length:", 0) != 0)
			continue;

		int contentLength = atoi(headerLine.c_str() + 15);
		if (contentLength <= 0 || contentLength > 10 * 1024 * 1024) {
			error = "invalid Content-Length";
			return json::Value(nullptr);
		}

		// Consume the empty line after headers.
		std::string emptyLine;
		_ReadLine(emptyLine, 1000);

		// Read the body.
		std::string body;
		if (!_ReadBytes(contentLength, body, timeoutMs)) {
			error = "timeout reading response body";
			return json::Value(nullptr);
		}

		try {
			json::Value message = json::Value::parse(body);

			// Skip notifications (no id field).
			const json::Value* idField = message.find("id");
			if (idField == NULL)
				continue;

			if (idField->is_num()
				&& static_cast<int32_t>(idField->as_num()) == expectedId) {
				// Check for error.
				const json::Value* err = message.find("error");
				if (err != NULL && err->is_obj()) {
					error = "LSP error: " + err->str("message");
					return json::Value(nullptr);
				}
				return message;
			}
		} catch (const std::exception&) {
			error = "malformed JSON response";
			return json::Value(nullptr);
		}
	}

	error = "no matching response after 20 messages";
	return json::Value(nullptr);
}


// --- Low-level I/O ---


bool
LspClient::_WriteAll(const char* data, size_t length)
{
	if (fWriteFd < 0)
		return false;

	size_t total = 0;
	while (total < length) {
		ssize_t written = write(fWriteFd, data + total, length - total);
		if (written < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		total += static_cast<size_t>(written);
	}
	return true;
}


bool
LspClient::_ReadBytes(size_t count, std::string& out, int timeoutMs)
{
	while (fReadBuffer.size() < count) {
		struct pollfd pfd;
		pfd.fd = fReadFd;
		pfd.events = POLLIN;
		pfd.revents = 0;

		int result = poll(&pfd, 1, timeoutMs);
		if (result <= 0)
			return false;

		char buffer[8192];
		ssize_t bytesRead = read(fReadFd, buffer, sizeof(buffer));
		if (bytesRead > 0)
			fReadBuffer.append(buffer, bytesRead);
		else if (bytesRead == 0)
			return false;	// EOF
		else if (errno != EINTR && errno != EAGAIN)
			return false;
	}

	out = fReadBuffer.substr(0, count);
	fReadBuffer.erase(0, count);
	return true;
}


bool
LspClient::_ReadLine(std::string& line, int timeoutMs)
{
	for (;;) {
		size_t newline = fReadBuffer.find('\n');
		if (newline != std::string::npos) {
			line = fReadBuffer.substr(0, newline);
			fReadBuffer.erase(0, newline + 1);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			return true;
		}

		struct pollfd pfd;
		pfd.fd = fReadFd;
		pfd.events = POLLIN;
		pfd.revents = 0;

		int result = poll(&pfd, 1, timeoutMs);
		if (result <= 0)
			return false;

		char buffer[4096];
		ssize_t bytesRead = read(fReadFd, buffer, sizeof(buffer));
		if (bytesRead > 0)
			fReadBuffer.append(buffer, bytesRead);
		else if (bytesRead == 0)
			return false;
		else if (errno != EINTR && errno != EAGAIN)
			return false;
	}
}


// --- Helpers ---


json::Value
LspClient::_BuildTextDocumentPosition(const std::string& filePath,
	int32_t line, int32_t character)
{
	json::Object textDocument;
	textDocument["uri"] = json::Value(_FileUri(filePath));

	json::Object position;
	position["line"] = json::Value(line);
	position["character"] = json::Value(character);

	json::Object params;
	params["textDocument"] = json::Value(std::move(textDocument));
	params["position"] = json::Value(std::move(position));

	return json::Value(std::move(params));
}


std::string
LspClient::_FileUri(const std::string& path)
{
	// Convert absolute path to file:// URI.
	if (path.empty())
		return "";
	if (path.rfind("file://", 0) == 0)
		return path;
	return "file://" + path;
}


std::string
LspClient::_DetectLanguageId(const std::string& path)
{
	auto endsWith = [](const std::string& str, const std::string& suffix) {
		if (suffix.size() > str.size())
			return false;
		return str.compare(str.size() - suffix.size(), suffix.size(),
			suffix) == 0;
	};

	if (endsWith(path, ".cpp") || endsWith(path, ".cc")
		|| endsWith(path, ".cxx")) {
		return "cpp";
	}
	if (endsWith(path, ".h") || endsWith(path, ".hpp")
		|| endsWith(path, ".hxx")) {
		return "cpp";
	}
	if (endsWith(path, ".c"))
		return "c";
	if (endsWith(path, ".py"))
		return "python";
	if (endsWith(path, ".go"))
		return "go";
	if (endsWith(path, ".js") || endsWith(path, ".mjs"))
		return "javascript";
	if (endsWith(path, ".ts"))
		return "typescript";
	if (endsWith(path, ".rs"))
		return "rust";
	if (endsWith(path, ".java"))
		return "java";
	if (endsWith(path, ".sh") || endsWith(path, ".bash"))
		return "shellscript";

	return "plaintext";
}


}	// namespace pluto
