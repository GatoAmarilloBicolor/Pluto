#pragma once

#include "../config.h"
#include "../provider/Provider.h"
#include "../tools/Tool.h"
#include <memory>
#include "../mcp/Manager.h"
#include <memory>
#include <string>
#include <vector>

namespace pluto {

struct CliOptions {
    std::string provider;
    std::string model;
    std::string base_url;
    bool yolo = false;
    bool no_cache = false;
    bool dry_run = false;
    std::string one_shot_prompt;
    std::string format = "text";
    bool quiet = false;
    bool headless = false;
    std::string headless_prompt;
    std::string headless_format;
    bool headless_quiet = false;
    bool help = false;
    bool version = false;
};

class CliApp {
public:
    CliApp(int argc, char** argv);
    ~CliApp();
    
    int run();
    
private:
    void parse_args(int argc, char** argv);
    void handle_subcommands();
    void setup_provider_and_tools();
    int run_repl_or_headless();
    
    CliOptions options_;
    Config cfg_;
    std::unique_ptr<Provider> provider_;
    std::unique_ptr<ToolRegistry> tools_;
    std::unique_ptr<McpManager> mcp_manager_;
    bool trust_command_ = false;
    std::string trust_subcommand_;
    std::string trust_path_;
    bool mcp_command_ = false;
    std::string mcp_subcommand_;
    std::vector<std::string> mcp_args_;
    bool stats_command_ = false;
    bool routine_command_ = false;
    bool schedule_command_ = false;
    bool wiki_command_ = false;
    bool beapi_command_ = false;
    bool agent_command_ = false;
    bool agent_run_ = false;
    std::string agent_run_file_;
    bool login_command_ = false;
    bool logout_command_ = false;
    bool init_wizard_ = false;
};

} // namespace pluto