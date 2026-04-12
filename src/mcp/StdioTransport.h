#pragma once

#include "Transport.h"

#include <map>
#include <string>
#include <sys/types.h>
#include <vector>

namespace pluto {

// Spawns the MCP server as a child process and speaks line-delimited
// JSON-RPC over its stdin/stdout. Child stderr is passed through to
// parent stderr so server diagnostics remain visible.
class StdioTransport : public Transport {
public:
	StdioTransport(std::string command,
	               std::vector<std::string> args,
	               std::map<std::string, std::string> env);
	~StdioTransport() override;

	bool start(std::string& err) override;
	void stop() override;
	std::string request(const std::string& body, std::string& err) override;
	void notify(const std::string& body) override;

private:
	std::string fCommand;
	std::vector<std::string> fArgs;
	std::map<std::string, std::string> fEnv;

	pid_t fPid   = -1;
	int   fWrite = -1;  // child stdin
	int   fRead  = -1;  // child stdout
	std::string fBuffer;

	bool write_all(const std::string& data, std::string& err);
	bool read_line(std::string& line, int timeout_ms, std::string& err);
};

}  // namespace pluto
