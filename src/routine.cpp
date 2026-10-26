#include "routine.h"
#include "util.h"

#ifdef __HAIKU__
#include <Clipboard.h>
#endif

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

namespace pluto {

namespace {

std::string home_dir() {
	return HomeDir();
}

// Shell-quote a string: wraps in single quotes, escaping embedded quotes.
std::string shq(const std::string& s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	out += "'";
	return out;
}

void ensure_dir(const std::string& dir) {
	std::string acc;
	for (size_t i = 0; i < dir.size(); ++i) {
		acc += dir[i];
		if (dir[i] == '/' && i > 0) ::mkdir(acc.c_str(), 0755);
	}
	::mkdir(dir.c_str(), 0755);
}

std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return {};
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

std::string get_clipboard_text() {
#ifdef __HAIKU__
	BClipboard clipboard("system");
	if (!clipboard.Lock()) return "";
	BMessage* data = clipboard.Data();
	if (!data) { clipboard.Unlock(); return ""; }
	const char* text = nullptr;
	ssize_t len = 0;
	if (data->FindData("text/plain", B_MIME_TYPE,
	    (const void**)&text, &len) != B_OK) {
		clipboard.Unlock();
		return "";
	}
	std::string result(text, len > 0 ? (size_t)len : 0);
	clipboard.Unlock();
	return result;
#else
	(void)0;
	return "";
#endif
}

std::string now_str() {
	std::time_t t = std::time(nullptr);
	std::tm tm;
	localtime_r(&t, &tm);
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
	return buf;
}

// Parse frontmatter key: value from a line.
bool parse_fm_line(const std::string& line, std::string& key, std::string& val) {
	size_t colon = line.find(':');
	if (colon == std::string::npos) return false;
	key = trim(line.substr(0, colon));
	val = trim(line.substr(colon + 1));
	return !key.empty();
}

std::string launch_dir() {
	return home_dir() + "/config/settings/launch";
}

std::string routine_scripts_dir() {
	return home_dir() + "/config/settings/pluto/routines/scripts";
}

std::string routine_job_name(const std::string& name) {
	return "x-vnd.pluto-routine-" + name;
}

}  // namespace

std::string RoutinesDir() {
	return home_dir() + "/config/settings/pluto/routines";
}

std::string RoutineLogsDir() {
	return home_dir() + "/config/settings/pluto/routine-logs";
}

bool ParseRoutine(const std::string& path, Routine& out) {
	std::ifstream f(path);
	if (!f) return false;

	out = Routine{};
	out.path = path;

	// Extract filename as name (without .md).
	size_t slash = path.rfind('/');
	std::string filename = (slash != std::string::npos)
		? path.substr(slash + 1) : path;
	if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".md")
		filename = filename.substr(0, filename.size() - 3);
	out.name = filename;

	std::string content;
	{
		std::ostringstream os;
		os << f.rdbuf();
		content = os.str();
	}

	// Parse YAML-ish frontmatter (--- ... ---).
	if (content.size() >= 4 && content.substr(0, 4) == "---\n") {
		size_t end = content.find("\n---\n", 4);
		if (end == std::string::npos) {
			out.body = content;
			return true;
		}
		std::string fm = content.substr(4, end - 4);
		out.body = content.substr(end + 5);

		std::istringstream ss(fm);
		std::string line;
		while (std::getline(ss, line)) {
			std::string key, val;
			if (!parse_fm_line(line, key, val)) continue;
			if (key == "name") out.name = val;
			else if (key == "schedule") out.schedule = val;
			else if (key == "model") out.model = val;
			else if (key == "notify") out.notify = (val == "true");
			else if (key == "enabled") out.enabled = (val != "false");
			else if (key == "tools") {
				// Comma-separated tool list.
				std::istringstream ts(val);
				std::string t;
				while (std::getline(ts, t, ','))
					if (!trim(t).empty()) out.tools.push_back(trim(t));
			}
		}
	} else {
		out.body = content;
		out.schedule = "manual";
	}

	if (out.schedule.empty()) out.schedule = "manual";
	return true;
}

std::vector<Routine> ListRoutines() {
	std::vector<Routine> out;
	std::string dir = RoutinesDir();
	DIR* d = opendir(dir.c_str());
	if (!d) return out;
	struct dirent* e;
	while ((e = readdir(d)) != nullptr) {
		std::string name = e->d_name;
		if (name.size() < 4) continue;
		if (name.substr(name.size() - 3) != ".md") continue;
		Routine r;
		if (ParseRoutine(dir + "/" + name, r))
			out.push_back(std::move(r));
	}
	closedir(d);
	std::sort(out.begin(), out.end(),
		[](const Routine& a, const Routine& b) { return a.name < b.name; });
	return out;
}

std::string DetectChangedFiles() {
	// Run git status --porcelain to get modified/added files.
	// Use fork/exec instead of popen for security
	int pipefd[2];
	if (pipe(pipefd) != 0) return "(no git changes)";
	pid_t pid = fork();
	if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return "(no git changes)"; }
	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		execl("/bin/sh", "sh", "-c", "git status --porcelain 2>/dev/null", nullptr);
		_exit(127);
	}
	close(pipefd[1]);
	std::string result;
	char buf[512];
	FILE* fp = fdopen(pipefd[0], "r");
	if (fp) {
		while (std::fgets(buf, sizeof(buf), fp)) {
			std::string line = buf;
			if (line.size() > 3) {
				std::string file = trim(line.substr(3));
				if (!file.empty()) {
					if (!result.empty()) result += "\n";
					result += file;
				}
			}
		}
		fclose(fp);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return result.empty() ? "(no git changes)" : result;

}

std::string ExpandTemplate(const std::string& body,
                            const std::string& changed_files) {
	std::string out = body;
	// {{date}}
	{
		size_t pos = 0;
		std::string date = now_str();
		while ((pos = out.find("{{date}}", pos)) != std::string::npos) {
			out.replace(pos, 8, date);
			pos += date.size();
		}
	}
	// {{clipboard}}
	{
		size_t pos = 0;
		while ((pos = out.find("{{clipboard}}", pos)) != std::string::npos) {
			std::string cb = get_clipboard_text();
			out.replace(pos, 13, cb);
			pos += cb.size();
		}
	}
	// {{changed_files}} — auto-detect from git if not explicitly provided.
	{
		size_t pos = 0;
		while ((pos = out.find("{{changed_files}}", pos)) != std::string::npos) {
			std::string files = changed_files.empty()
				? DetectChangedFiles() : changed_files;
			out.replace(pos, 17, files);
			pos += files.size();
		}
	}
	return out;
}

std::string BuildRoutinePrompt(const Routine& r) {
	std::string prompt;

	// Header with constraints.
	prompt += "[Routine: " + r.name + "]\n";
	if (!r.tools.empty()) {
		prompt += "IMPORTANT: You may ONLY use these tools: ";
		for (size_t i = 0; i < r.tools.size(); ++i) {
			if (i > 0) prompt += ", ";
			prompt += r.tools[i];
		}
		prompt += ". Do NOT use any other tools.\n";
	}
	prompt += "\n";

	// Expanded body.
	prompt += ExpandTemplate(r.body);
	return prompt;
}

bool ImportRoutine(const std::string& source_path, std::string& error) {
	// Read source file.
	std::ifstream sf(source_path);
	if (!sf) {
		error = "cannot open: " + source_path;
		return false;
	}
	std::string content;
	{
		std::ostringstream os;
		os << sf.rdbuf();
		content = os.str();
	}
	sf.close();

	// Validate frontmatter delimiters.
	if (content.size() < 4 || content.substr(0, 4) != "---\n") {
		error = "missing frontmatter: file must start with '---'";
		return false;
	}
	size_t end = content.find("\n---\n", 4);
	if (end == std::string::npos) {
		error = "missing frontmatter: closing '---' not found";
		return false;
	}

	// Parse frontmatter and validate required fields.
	std::string fm = content.substr(4, end - 4);
	std::string body = content.substr(end + 5);

	std::string name, schedule;
	{
		std::istringstream ss(fm);
		std::string line;
		while (std::getline(ss, line)) {
			std::string key, val;
			if (!parse_fm_line(line, key, val)) continue;
			if (key == "name") name = val;
			else if (key == "schedule") schedule = val;
		}
	}

	if (name.empty()) {
		error = "frontmatter missing required field: 'name'";
		return false;
	}
	if (schedule.empty()) {
		error = "frontmatter missing required field: 'schedule'";
		return false;
	}
	if (trim(body).empty()) {
		error = "routine body (prompt) is empty";
		return false;
	}

	// Ensure routines directory exists.
	std::string dir = RoutinesDir();
	ensure_dir(dir);

	// Destination: RoutinesDir()/<name>.md
	std::string dest = dir + "/" + name + ".md";

	// Check for overwrite.
	struct stat st;
	if (::stat(dest.c_str(), &st) == 0) {
		error = "routine '" + name + "' already exists: " + dest;
		return false;
	}

	// Write the file.
	std::ofstream df(dest);
	if (!df) {
		error = "cannot write: " + dest;
		return false;
	}
	df << content;
	df.close();

	return true;
}

bool SetRoutineEnabled(const std::string& path, bool enabled,
                         std::string& error) {
	Routine r;
	if (!ParseRoutine(path, r)) {
		error = "cannot parse: " + path;
		return false;
	}
	r.enabled = enabled;

	// Rewrite the file with updated frontmatter.
	std::ofstream f(path);
	if (!f) { error = "cannot write: " + path; return false; }
	f << "---\n";
	f << "name: " << r.name << "\n";
	f << "schedule: " << r.schedule << "\n";
	if (!r.model.empty()) f << "model: " << r.model << "\n";
	if (!r.tools.empty()) {
		f << "tools: ";
		for (size_t i = 0; i < r.tools.size(); ++i) {
			if (i > 0) f << ", ";
			f << r.tools[i];
		}
		f << "\n";
	}
	f << "notify: " << (r.notify ? "true" : "false") << "\n";
	f << "enabled: " << (enabled ? "true" : "false") << "\n";
	f << "---\n";
	f << r.body;

	// Install or remove launch_daemon job for automatic schedules.
	if (r.schedule != "manual" && !r.schedule.empty()) {
		std::string job_err;
		if (enabled)
			InstallRoutineJob(r, job_err);
		else
			UninstallRoutineJob(r.name, job_err);
	}

	return true;
}

bool InstallRoutineJob(const Routine& r, std::string& error) {
	if (r.schedule == "manual" || r.schedule.empty()) {
		error = "routine '" + r.name + "' has schedule=manual, nothing to install";
		return false;
	}

	ensure_dir(launch_dir());

	// Build the run command.
	std::string run_cmd = "pluto routine run " + shq(r.name);

	// Parse schedule to determine event and interval.
	std::string event;
	int interval_secs = 0;

	if (r.schedule == "on_boot") {
		event = "initial_volumes_mounted";
	} else if (r.schedule.substr(0, 15) == "on_file_change ") {
		// on_file_change needs a watcher — for now, generate a polling
		// script that checks mtime every 30s.
		event = "initial_volumes_mounted";
		std::string watch_dir = r.schedule.substr(15);

		ensure_dir(routine_scripts_dir());
		std::string script = routine_scripts_dir() + "/" + r.name + ".sh";
		std::ofstream sf(script);
		if (!sf) { error = "cannot write: " + script; return false; }
		std::string sq_watch = shq(watch_dir);
		std::string sq_name = shq(r.name);
		sf << "#!/bin/sh\n";
		sf << "# Auto-generated by pluto routine. Do not edit.\n";
		sf << "WATCH_DIR=" << sq_watch << "\n";
		sf << "LAST_MOD=0\n";
		sf << "while true; do\n";
		sf << "  CUR=$(stat -c %Y \"$WATCH_DIR\" 2>/dev/null || echo 0)\n";
		sf << "  if [ \"$CUR\" != \"$LAST_MOD\" ] && [ \"$LAST_MOD\" != \"0\" ]; then\n";
		sf << "    CHANGED=$(find \"$WATCH_DIR\" -newer /tmp/pluto-watch-"
		   << sq_name << " -type f 2>/dev/null | head -20)\n";
		sf << "    echo \"$CHANGED\" > /tmp/pluto-routine-changed-" << sq_name << "\n";
		sf << "    " << run_cmd << " 2>>/tmp/pluto-routine-" << sq_name << ".log\n";
		sf << "  fi\n";
		sf << "  LAST_MOD=$CUR\n";
		sf << "  touch /tmp/pluto-watch-" << sq_name << "\n";
		sf << "  sleep 30\n";
		sf << "done\n";
		sf.close();
		chmod(script.c_str(), 0755);
		run_cmd = script;
	} else {
		// Try parsing as interval: "every Nm" or "every Nh"
		size_t every_pos = r.schedule.find("every ");
		if (every_pos != std::string::npos) {
			std::string spec = r.schedule.substr(every_pos + 6);
			int val = std::atoi(spec.c_str());
			if (val > 0) {
				char unit = spec.back();
				if (unit == 'h') interval_secs = val * 3600;
				else interval_secs = val * 60;  // default minutes
			}
			event = "initial_volumes_mounted";

			ensure_dir(routine_scripts_dir());
			std::string script = routine_scripts_dir() + "/" + r.name + ".sh";
			std::ofstream sf(script);
			if (!sf) { error = "cannot write: " + script; return false; }
			sf << "#!/bin/sh\n";
			sf << "# Auto-generated by pluto routine. Do not edit.\n";
			sf << "while true; do\n";
			sf << "  " << run_cmd
			   << " 2>>/tmp/pluto-routine-" << shq(r.name) << ".log\n";
			sf << "  sleep " << interval_secs << "\n";
			sf << "done\n";
			sf.close();
			chmod(script.c_str(), 0755);
			run_cmd = script;
		} else {
			// Unknown schedule type, treat as on_boot.
			event = "initial_volumes_mounted";
		}
	}

	// Write launch_daemon job file.
	std::string job_path = launch_dir() + "/pluto-routine-" + r.name;
	std::ofstream jf(job_path);
	if (!jf) { error = "cannot write: " + job_path; return false; }

	jf << "job " << routine_job_name(r.name) << " {\n";
	jf << "\tlaunch " << run_cmd << "\n";
	jf << "\ton " << event << "\n";
	if (!r.enabled) jf << "\tdisabled\n";
	jf << "}\n";

	return true;
}

bool UninstallRoutineJob(const std::string& name, std::string& error) {
	std::string path = launch_dir() + "/pluto-routine-" + name;
	if (std::remove(path.c_str()) != 0 && errno != ENOENT) {
		error = "cannot remove: " + path;
		return false;
	}
	// Also remove script if present.
	std::string script = routine_scripts_dir() + "/" + name + ".sh";
	std::remove(script.c_str());
	return true;
}

void LogRoutineRun(const std::string& name, const std::string& output) {
	ensure_dir(RoutineLogsDir());
	std::string path = RoutineLogsDir() + "/" + name + ".log";
	std::ofstream f(path, std::ios::app);
	if (!f) return;
	f << "=== " << now_str() << " ===\n";
	f << output << "\n\n";
}

std::string ReadRoutineLog(const std::string& name, int lines) {
	std::string path = RoutineLogsDir() + "/" + name + ".log";
	std::ifstream f(path);
	if (!f) return "(no log)";
	std::vector<std::string> all_lines;
	std::string line;
	while (std::getline(f, line)) all_lines.push_back(line);
	size_t start = all_lines.size() > (size_t)lines
		? all_lines.size() - (size_t)lines : 0;
	std::string result;
	for (size_t i = start; i < all_lines.size(); ++i) {
		result += all_lines[i];
		result += "\n";
	}
	return result;
}

int RunRoutineCli(int argc, char** argv) {
	if (argc < 1) {
		std::fprintf(stderr,
			"usage: pluto routine list\n"
			"       pluto routine run <name>\n"
			"       pluto routine import <file.md>\n"
			"       pluto routine enable <name>\n"
			"       pluto routine disable <name>\n"
			"       pluto routine install <name>\n"
			"       pluto routine uninstall <name>\n"
			"       pluto routine log <name>\n");
		return 2;
	}

	std::string sub = argv[0];

	if (sub == "list") {
		auto routines = ListRoutines();
		if (routines.empty()) {
			std::printf("(no routines in %s)\n", RoutinesDir().c_str());
			return 0;
		}
		for (const auto& r : routines) {
			const char* status = r.enabled ? "active" : "paused";
			std::printf("  %-20s  %-8s  %-20s",
				r.name.c_str(), status, r.schedule.c_str());
			if (!r.model.empty())
				std::printf("  model=%s", r.model.c_str());
			std::printf("\n");
		}
		return 0;
	}

	if (sub == "run") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto routine run <name>\n");
			return 2;
		}
		std::string name = argv[1];
		auto routines = ListRoutines();
		const Routine* found = nullptr;
		for (const auto& r : routines) {
			if (r.name == name) { found = &r; break; }
		}
		if (!found) {
			std::fprintf(stderr, "routine not found: %s\n", name.c_str());
			return 1;
		}
		std::string prompt = ExpandTemplate(found->body);

		// Write expanded prompt to a temp file (owner-only) and use agent run.
		char tmpl[] = "/tmp/pluto-routine-XXXXXX";
		int tmpfd = mkstemp(tmpl);
		if (tmpfd < 0) {
			std::fprintf(stderr, "cannot create temp file\n");
			return 1;
		}
		std::string tmp = tmpl;
		::write(tmpfd, prompt.c_str(), prompt.size());
		::close(tmpfd);
		std::string run_cmd = "pluto agent run " + shq(tmp);
		if (!found->model.empty())
			run_cmd += " --model " + shq(found->model);

		std::printf("Running routine '%s'…\n", name.c_str());
		// Use fork/exec instead of system for security
		pid_t pid = fork();
		if (pid < 0) return -1;
		if (pid == 0) {
			execl("/bin/sh", "sh", "-c", run_cmd.c_str(), nullptr);
			_exit(127);
		}
		int status = 0;
		waitpid(pid, &status, 0);
		int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

		// TODO: capture output for logging (currently goes to stdout).
		return rc;
	}

	if (sub == "enable" || sub == "disable") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto routine %s <name>\n", sub.c_str());
			return 2;
		}
		std::string name = argv[1];
		auto routines = ListRoutines();
		for (const auto& r : routines) {
			if (r.name == name) {
				std::string error;
				bool en = (sub == "enable");
				if (!SetRoutineEnabled(r.path, en, error)) {
					std::fprintf(stderr, "error: %s\n", error.c_str());
					return 1;
				}
				std::printf("routine '%s' %sd\n", name.c_str(), sub.c_str());
				return 0;
			}
		}
		std::fprintf(stderr, "routine not found: %s\n", name.c_str());
		return 1;
	}

	if (sub == "install" || sub == "uninstall") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto routine %s <name>\n", sub.c_str());
			return 2;
		}
		std::string name = argv[1];
		if (sub == "uninstall") {
			std::string error;
			if (!UninstallRoutineJob(name, error)) {
				std::fprintf(stderr, "error: %s\n", error.c_str());
				return 1;
			}
			std::printf("routine '%s' launch_daemon job removed\n", name.c_str());
			return 0;
		}
		auto routines = ListRoutines();
		for (const auto& r : routines) {
			if (r.name == name) {
				std::string error;
				if (!InstallRoutineJob(r, error)) {
					std::fprintf(stderr, "error: %s\n", error.c_str());
					return 1;
				}
				std::printf("routine '%s' launch_daemon job installed\n",
					name.c_str());
				std::printf("  takes effect on next boot\n");
				return 0;
			}
		}
		std::fprintf(stderr, "routine not found: %s\n", name.c_str());
		return 1;
	}

	if (sub == "import") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto routine import <file.md>\n");
			return 2;
		}
		std::string path = argv[1];
		std::string error;
		if (!ImportRoutine(path, error)) {
			std::fprintf(stderr, "import failed: %s\n", error.c_str());
			return 1;
		}
		// Re-parse to show the imported routine details.
		Routine r;
		std::string dir = RoutinesDir();
		// Find the imported name from the file.
		Routine tmp;
		ParseRoutine(path, tmp);
		std::string dest = dir + "/" + tmp.name + ".md";
		if (ParseRoutine(dest, r)) {
			std::printf("routine '%s' imported\n", r.name.c_str());
			std::printf("  schedule: %s\n", r.schedule.c_str());
			if (!r.model.empty())
				std::printf("  model:    %s\n", r.model.c_str());
			std::printf("  enabled:  %s\n", r.enabled ? "true" : "false");
			std::printf("  path:     %s\n", dest.c_str());
		} else {
			std::printf("routine imported to %s\n", dest.c_str());
		}
		return 0;
	}

	if (sub == "log") {
		if (argc < 2) {
			std::fprintf(stderr, "usage: pluto routine log <name>\n");
			return 2;
		}
		std::string log = ReadRoutineLog(argv[1]);
		std::printf("%s", log.c_str());
		return 0;
	}

	std::fprintf(stderr, "unknown subcommand: %s\n", sub.c_str());
	return 2;
}

}  // namespace pluto
