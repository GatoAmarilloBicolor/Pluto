#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pluto {

struct Schedule {
	std::string name;          // schedule identifier (slug)
	std::string prompt_file;   // path to .md prompt file
	std::string event;         // launch_daemon event: "boot", "login", or ""
	int interval_mins = 0;     // periodic interval in minutes (0 = one-shot on event)
	std::string model;         // model override (empty = use default)
	bool enabled = true;
};

// Directory for schedule configs.
std::string SchedulesDir();

// List all configured schedules.
std::vector<Schedule> ListSchedules();

// Load a single schedule by name.  Returns false if not found.
bool LoadSchedule(const std::string& name, Schedule& out);

// Save a schedule config file.  Creates or overwrites.
bool SaveSchedule(const Schedule& sched, std::string& error);

// Remove a schedule and its launch_daemon job.
bool RemoveSchedule(const std::string& name, std::string& error);

// Install (or update) a launch_daemon job file for the schedule.
bool InstallLaunchJob(const Schedule& sched, std::string& error);

// Remove the launch_daemon job file.
bool UninstallLaunchJob(const std::string& name, std::string& error);

// Run the `pluto schedule` subcommand.
int RunScheduleCli(int argc, char** argv);

}  // namespace pluto
