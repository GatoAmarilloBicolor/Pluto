#include "StdioTransport.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

#include "../json.h"

namespace pluto {

StdioTransport::StdioTransport(std::string command,
                                std::vector<std::string> args,
                                std::map<std::string, std::string> env)
	: fCommand(std::move(command)), fArgs(std::move(args)), fEnv(std::move(env)) {}

StdioTransport::~StdioTransport() { stop(); }

bool StdioTransport::start(std::string& err) {
	if (fCommand.empty()) { err = "command required"; return false; }

	int in_pipe[2], out_pipe[2];
	if (pipe(in_pipe)  != 0) { err = std::strerror(errno); return false; }
	if (pipe(out_pipe) != 0) {
		int e = errno;
		close(in_pipe[0]); close(in_pipe[1]);
		err = std::strerror(e);
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		int e = errno;
		close(in_pipe[0]);  close(in_pipe[1]);
		close(out_pipe[0]); close(out_pipe[1]);
		err = std::strerror(e);
		return false;
	}

	if (pid == 0) {
		// Child
		dup2(in_pipe[0],  STDIN_FILENO);
		dup2(out_pipe[1], STDOUT_FILENO);
		close(in_pipe[0]);  close(in_pipe[1]);
		close(out_pipe[0]); close(out_pipe[1]);

		for (const auto& [k, v] : fEnv) setenv(k.c_str(), v.c_str(), 1);

		std::vector<char*> argv;
		argv.push_back(strdup(fCommand.c_str()));
		for (const auto& a : fArgs) argv.push_back(strdup(a.c_str()));
		argv.push_back(nullptr);
		execvp(fCommand.c_str(), argv.data());
		std::fprintf(stderr, "[mcp/stdio] execvp %s: %s\n",
			fCommand.c_str(), std::strerror(errno));
		_exit(127);
	}

	// Parent
	close(in_pipe[0]);
	close(out_pipe[1]);
	fWrite = in_pipe[1];
	fRead  = out_pipe[0];
	fPid   = pid;

	int flags = fcntl(fRead, F_GETFL, 0);
	if (flags >= 0) fcntl(fRead, F_SETFL, flags | O_NONBLOCK);
	return true;
}

void StdioTransport::stop() {
	if (fWrite >= 0) { close(fWrite); fWrite = -1; }
	if (fRead  >= 0) { close(fRead);  fRead  = -1; }
	if (fPid > 0) {
		kill(fPid, SIGTERM);
		for (int i = 0; i < 50; ++i) {
			int status = 0;
			pid_t r = waitpid(fPid, &status, WNOHANG);
			if (r == fPid || r < 0) { fPid = -1; return; }
			usleep(10000);
		}
		kill(fPid, SIGKILL);
		waitpid(fPid, nullptr, 0);
		fPid = -1;
	}
}

bool StdioTransport::write_all(const std::string& data, std::string& err) {
	if (fWrite < 0) { err = "not started"; return false; }
	size_t total = 0;
	while (total < data.size()) {
		ssize_t n = write(fWrite, data.data() + total, data.size() - total);
		if (n < 0) {
			if (errno == EINTR) continue;
			err = std::strerror(errno);
			return false;
		}
		total += static_cast<size_t>(n);
	}
	return true;
}

bool StdioTransport::read_line(std::string& line, int timeout_ms, std::string& err) {
	for (;;) {
		size_t nl = fBuffer.find('\n');
		if (nl != std::string::npos) {
			line = fBuffer.substr(0, nl);
			fBuffer.erase(0, nl + 1);
			if (!line.empty() && line.back() == '\r') line.pop_back();
			return true;
		}

		struct pollfd pfd;
		pfd.fd = fRead;
		pfd.events = POLLIN;
		pfd.revents = 0;
		int rc = poll(&pfd, 1, timeout_ms);
		if (rc == 0) { err = "read timeout"; return false; }
		if (rc < 0) {
			if (errno == EINTR) continue;
			err = std::strerror(errno);
			return false;
		}

		char buf[4096];
		ssize_t n = read(fRead, buf, sizeof(buf));
		if (n > 0) { fBuffer.append(buf, n); continue; }
		if (n == 0) { err = "child closed stdout"; return false; }
		if (errno == EINTR || errno == EAGAIN) continue;
		err = std::strerror(errno);
		return false;
	}
}

std::string StdioTransport::request(const std::string& body, std::string& err) {
	if (!write_all(body + "\n", err)) return {};

	int my_id = -1;
	try {
		json::Value v = json::Value::parse(body);
		const json::Value* id = v.find("id");
		if (id && id->is_num()) my_id = static_cast<int>(id->as_num());
	} catch (...) {}

	for (;;) {
		std::string line;
		if (!read_line(line, 60000, err)) return {};
		if (line.empty()) continue;

		// Skip notifications (no id) and responses for other ids.
		try {
			json::Value v = json::Value::parse(line);
			const json::Value* id = v.find("id");
			if (!id || !id->is_num()) continue;
			if (my_id >= 0 && static_cast<int>(id->as_num()) != my_id) continue;
		} catch (...) { continue; }
		return line;
	}
}

void StdioTransport::notify(const std::string& body) {
	std::string err;
	(void)write_all(body + "\n", err);
}

}  // namespace pluto
