#include "telemetry.h"
#include "filelock.h"
#include "util.h"

#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace pluto {

namespace {

std::string home_dir() {
	return HomeDir();
}

std::string stats_path() {
	return home_dir() + "/config/settings/pluto/stats.csv";
}

void ensure_parent(const std::string& path) {
	size_t slash = path.rfind('/');
	if (slash == std::string::npos) return;
	std::string dir = path.substr(0, slash);
	std::string acc;
	for (size_t i = 0; i < dir.size(); ++i) {
		acc += dir[i];
		if (dir[i] == '/' && i > 0) ::mkdir(acc.c_str(), 0755);
	}
	::mkdir(dir.c_str(), 0755);
}

double estimate_cost(const std::string& model, int64_t in, int64_t out) {
	// Default to Sonnet pricing (Anthropic)
	double in_rate = 3.0, out_rate = 15.0;
	std::string m = model;
	for (auto& c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

	// Anthropic
	if (m.find("opus") != std::string::npos) {
		in_rate = 15.0; out_rate = 75.0;
	} else if (m.find("sonnet") != std::string::npos) {
		in_rate = 3.0; out_rate = 15.0;
	} else if (m.find("haiku") != std::string::npos) {
		in_rate = 0.80; out_rate = 4.0;
	}
	// OpenAI
	else if (m.find("gpt-4o") != std::string::npos) {
		in_rate = 5.0; out_rate = 15.0;
	} else if (m.find("gpt-4") != std::string::npos) {
		in_rate = 30.0; out_rate = 60.0;
	} else if (m.find("gpt-3.5") != std::string::npos) {
		in_rate = 0.50; out_rate = 1.50;
	}
	// OpenRouter - varies by model, use Sonnet as default
	// Local models (Ollama, LM Studio) - free
	else if (m.find("llama") != std::string::npos
	         || m.find("mistral") != std::string::npos
	         || m.find("qwen") != std::string::npos
	         || m.find("gemma") != std::string::npos
	         || m.find("phi") != std::string::npos) {
		in_rate = 0.0; out_rate = 0.0;
	}
	// DeepSeek
	else if (m.find("deepseek") != std::string::npos) {
		in_rate = 0.14; out_rate = 0.28;
	}
	// Groq
	else if (m.find("groq") != std::string::npos) {
		in_rate = 0.0; out_rate = 0.0;  // free tier
	}
	// Google Gemini
	else if (m.find("gemini") != std::string::npos) {
		in_rate = 0.075; out_rate = 0.30;
	}
	// Cohere
	else if (m.find("command") != std::string::npos) {
		in_rate = 2.50; out_rate = 10.0;
	}
	// XAI Grok
	else if (m.find("grok") != std::string::npos) {
		in_rate = 3.0; out_rate = 15.0;
	}

	return (in * in_rate / 1e6) + (out * out_rate / 1e6);
}

}  // namespace

bool TelemetryEnabled() {
	// Opt-in: check for file existence.
	std::string flag = home_dir() + "/config/settings/pluto/telemetry";
	struct stat st;
	return ::stat(flag.c_str(), &st) == 0;
}

void TelemetryRecord(const std::string& model,
                      int input_tokens, int output_tokens,
                      int cache_read, int cache_create) {
	if (!TelemetryEnabled()) return;
	std::string path = stats_path();
	ensure_parent(path);
	ScopedFileLock lock(path + ".lock", 500);
	if (!lock.locked()) return;
	std::ofstream f(path, std::ios::app);
	if (!f) return;
	// CSV: model,in,out,cache_read,cache_create,type,timestamp
	// Sanitize model name: strip commas, newlines, quotes (RFC 4180).
	std::string safe_model;
	safe_model.reserve(model.size());
	for (char c : model) {
		if (c == ',' || c == '\n' || c == '\r' || c == '"') continue;
		safe_model += c;
	}
	f << safe_model << "," << input_tokens << "," << output_tokens
	  << "," << cache_read << "," << cache_create << ",turn,"
	  << (long long)std::time(nullptr) << "\n";
}

void TelemetrySessionEnd(const std::string& model) {
	if (!TelemetryEnabled()) return;
	std::string path = stats_path();
	ensure_parent(path);
	ScopedFileLock lock(path + ".lock", 500);
	if (!lock.locked()) return;
	std::ofstream f(path, std::ios::app);
	if (!f) return;
	std::string safe_model;
	safe_model.reserve(model.size());
	for (char c : model) {
		if (c == ',' || c == '\n' || c == '\r' || c == '"') continue;
		safe_model += c;
	}
	f << safe_model << ",0,0,0,0,session,"
	  << (long long)std::time(nullptr) << "\n";
}

std::vector<ModelStats> TelemetryLoad() {
	std::map<std::string, ModelStats> agg;
	std::string path = stats_path();
	std::ifstream f(path);
	if (!f) return {};
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty()) continue;
		// Parse CSV: model,in,out,cache_read,cache_create,type
		std::istringstream ss(line);
		std::string model, in_s, out_s, cr_s, cc_s, type;
		if (!std::getline(ss, model, ',')) continue;
		if (!std::getline(ss, in_s, ',')) continue;
		if (!std::getline(ss, out_s, ',')) continue;
		if (!std::getline(ss, cr_s, ',')) continue;
		if (!std::getline(ss, cc_s, ',')) continue;
		std::getline(ss, type, ',');

		auto& s = agg[model];
		s.model = model;
		int64_t in = std::atoll(in_s.c_str());
		int64_t out = std::atoll(out_s.c_str());
		s.input_tokens += in;
		s.output_tokens += out;
		s.cache_read_tokens += std::atoll(cr_s.c_str());
		s.cache_create_tokens += std::atoll(cc_s.c_str());
		if (type == "turn") s.turns++;
		else if (type == "session") s.sessions++;
	}

	std::vector<ModelStats> result;
	for (auto& [_, v] : agg) {
		v.total_cost_usd = estimate_cost(v.model, v.input_tokens, v.output_tokens);
		result.push_back(std::move(v));
	}
	return result;
}

std::vector<DailyStats> TelemetryLoadDaily() {
	// Key: "YYYY-MM-DD|model"
	std::map<std::string, DailyStats> agg;
	std::string path = stats_path();
	std::ifstream f(path);
	if (!f) return {};
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty()) continue;
		std::istringstream ss(line);
		std::string model, in_s, out_s, cr_s, cc_s, type, ts_s;
		if (!std::getline(ss, model, ',')) continue;
		if (!std::getline(ss, in_s, ',')) continue;
		if (!std::getline(ss, out_s, ',')) continue;
		if (!std::getline(ss, cr_s, ',')) continue;
		if (!std::getline(ss, cc_s, ',')) continue;
		std::getline(ss, type, ',');
		std::getline(ss, ts_s, ',');  // optional timestamp

		int64_t ts = std::atoll(ts_s.c_str());
		if (ts <= 0) continue;  // skip records without timestamp

		// Convert timestamp to date string.
		std::time_t t = static_cast<std::time_t>(ts);
		std::tm tm;
		if (!localtime_r(&t, &tm)) continue;
		char date[12];
		std::strftime(date, sizeof(date), "%Y-%m-%d", &tm);

		std::string key = std::string(date) + "|" + model;
		auto& d = agg[key];
		d.date = date;
		d.model = model;
		int64_t in = std::atoll(in_s.c_str());
		int64_t out = std::atoll(out_s.c_str());
		d.input_tokens += in;
		d.output_tokens += out;
		if (type == "turn") d.turns++;
		d.cost_usd += estimate_cost(model, in, out);
	}

	std::vector<DailyStats> result;
	for (auto& [_, v] : agg) result.push_back(std::move(v));
	std::sort(result.begin(), result.end(), [](const DailyStats& a, const DailyStats& b) {
		return a.date < b.date || (a.date == b.date && a.model < b.model);
	});
	return result;
}

int RunStatsCli(int argc, char** argv) {
	if (!TelemetryEnabled()) {
		std::printf("Telemetry is disabled.\n");
		std::printf("To enable: touch ~/config/settings/pluto/telemetry\n");
		return 0;
	}

	auto stats = TelemetryLoad();
	if (stats.empty()) {
		std::printf("No telemetry data yet.\n");
		return 0;
	}

	if (argc >= 1 && std::string(argv[0]) == "reset") {
		std::remove(stats_path().c_str());
		std::printf("Telemetry data cleared.\n");
		return 0;
	}

	std::printf("%-30s  %8s  %8s  %6s  %6s  %10s\n",
		"Model", "In tok", "Out tok", "Turns", "Sess.", "Cost USD");
	std::printf("%-30s  %8s  %8s  %6s  %6s  %10s\n",
		"-----", "------", "-------", "-----", "-----", "--------");

	int64_t total_in = 0, total_out = 0, total_turns = 0, total_sess = 0;
	double total_cost = 0;
	for (const auto& s : stats) {
		std::printf("%-30s  %8lld  %8lld  %6lld  %6lld  $%9.4f\n",
			s.model.c_str(),
			(long long)s.input_tokens, (long long)s.output_tokens,
			(long long)s.turns, (long long)s.sessions,
			s.total_cost_usd);
		total_in += s.input_tokens;
		total_out += s.output_tokens;
		total_turns += s.turns;
		total_sess += s.sessions;
		total_cost += s.total_cost_usd;
	}
	std::printf("%-30s  %8lld  %8lld  %6lld  %6lld  $%9.4f\n",
		"TOTAL",
		(long long)total_in, (long long)total_out,
		(long long)total_turns, (long long)total_sess,
		total_cost);

	// --graph: ASCII bar chart of token usage per model.
	if (argc >= 1 && std::string(argv[0]) == "--graph") {
		std::printf("\n  Token usage by model:\n\n");
		int64_t max_tok = 0;
		for (const auto& s : stats)
			max_tok = std::max(max_tok, s.input_tokens + s.output_tokens);
		if (max_tok == 0) max_tok = 1;
		const int bar_width = 40;
		for (const auto& s : stats) {
			int64_t total = s.input_tokens + s.output_tokens;
			int in_len = (int)(s.input_tokens * bar_width / max_tok);
			int out_len = (int)(s.output_tokens * bar_width / max_tok);
			if (total > 0 && in_len + out_len == 0) in_len = 1;
			// Model name (truncated to 20 chars).
			std::string name = s.model;
			if (name.size() > 20) name = name.substr(0, 17) + "...";
			std::printf("  %-20s |", name.c_str());
			// Input bar (cyan), output bar (green).
			std::printf("\x1b[36m");
			for (int i = 0; i < in_len; ++i) std::putchar('#');
			std::printf("\x1b[32m");
			for (int i = 0; i < out_len; ++i) std::putchar('#');
			std::printf("\x1b[0m");
			std::printf(" %lld tok\n", (long long)total);
		}
		std::printf("\n  \x1b[36m#\x1b[0m = input  "
		            "\x1b[32m#\x1b[0m = output\n");
	}

	return 0;
}

}  // namespace pluto
