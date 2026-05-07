#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pluto {

struct ModelStats {
	std::string model;
	int64_t input_tokens = 0;
	int64_t output_tokens = 0;
	int64_t cache_read_tokens = 0;
	int64_t cache_create_tokens = 0;
	int64_t turns = 0;
	int64_t sessions = 0;
	double total_cost_usd = 0.0;
};

// Record token usage for a model. Appends to the stats file.
void TelemetryRecord(const std::string& model,
                      int input_tokens, int output_tokens,
                      int cache_read, int cache_create);

// Record a session completion (increments session count).
void TelemetrySessionEnd(const std::string& model);

// Load aggregated stats per model.
std::vector<ModelStats> TelemetryLoad();

// Daily aggregation for time-series chart.
struct DailyStats {
	std::string date;    // "YYYY-MM-DD"
	std::string model;
	int64_t input_tokens = 0;
	int64_t output_tokens = 0;
	int64_t turns = 0;
	double cost_usd = 0.0;
};

// Load stats aggregated by (date, model). Only includes records with
// timestamps. Sorted by date ascending.
std::vector<DailyStats> TelemetryLoadDaily();

// Check if telemetry is enabled (opt-in via config).
bool TelemetryEnabled();

// Run `pluto stats` subcommand.
int RunStatsCli(int argc, char** argv);

}  // namespace pluto
