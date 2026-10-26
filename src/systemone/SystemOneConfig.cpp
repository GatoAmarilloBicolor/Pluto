#include "SystemOneConfig.h"
#include "../config.h"
#include "../util.h"

#ifdef __HAIKU__
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#endif

#include <fstream>
#include <sstream>

namespace pluto {
namespace systemone {

namespace {

std::string settings_dir() {
    return home_dir() + "/config/settings/pluto";
}

std::string config_path() {
    return settings_dir() + "/systemone.conf";
}

std::string decision_model_type_to_string(DecisionModelType type) {
    switch (type) {
        case DecisionModelType::REMOTE_KEV: return "kev";
        case DecisionModelType::REMOTE_LAYA: return "laya";
        case DecisionModelType::REMOTE_JEV: return "jev";
        case DecisionModelType::LOCAL: return "local";
        case DecisionModelType::AUTO: return "auto";
    }
    return "auto";
}

DecisionModelType decision_model_type_from_string(const std::string& s) {
    if (s == "kev") return DecisionModelType::REMOTE_KEV;
    if (s == "laya") return DecisionModelType::REMOTE_LAYA;
    if (s == "jev") return DecisionModelType::REMOTE_JEV;
    if (s == "local") return DecisionModelType::LOCAL;
    return DecisionModelType::AUTO;
}

} // namespace

SystemOneConfig LoadSystemOneConfig() {
    SystemOneConfig cfg;
    std::ifstream f(config_path());
    if (!f) return cfg;

    std::string line;
    while (std::getline(f, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line[start] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // Trim
        size_t ks = key.find_first_not_of(" \t");
        size_t ke = key.find_last_not_of(" \t");
        key = key.substr(ks, ke - ks + 1);
        size_t vs = val.find_first_not_of(" \t");
        size_t ve = val.find_last_not_of(" \t");
        val = val.substr(vs, ve - vs + 1);

        if (key == "enabled") cfg.enabled = (val == "true" || val == "1");
        else if (key == "model") cfg.model = decision_model_type_from_string(val);
        else if (key == "api_key") cfg.api_key = val;
        else if (key == "base_url") cfg.base_url = val;
        else if (key == "kev_url") cfg.kev_url = val;
        else if (key == "laya_url") cfg.laya_url = val;
        else if (key == "jev_url") cfg.jev_url = val;
        else if (key == "threshold") {
            try { cfg.default_threshold = std::stod(val); } catch (...) {}
        }
        else if (key == "timeout_secs") {
            try { cfg.timeout_secs = std::stoi(val); } catch (...) {}
        }
        else if (key == "prefer_local") cfg.prefer_local = (val == "true" || val == "1");
        else if (key == "fallback_to_local") cfg.fallback_to_local = (val == "true" || val == "1");
    }
    return cfg;
}

bool SaveSystemOneConfig(const SystemOneConfig& cfg) {
    std::string dir = settings_dir();
#ifdef __HAIKU__
    ::mkdir(dir.c_str(), 0755);
#else
    ::mkdir(dir.c_str(), 0755);
#endif

    std::ofstream f(config_path(), std::ios::trunc);
    if (!f) return false;

    f << "# System One Decision Engine Configuration\n";
    f << "# Lines starting with # are comments\n\n";
    f << "enabled = " << (cfg.enabled ? "true" : "false") << "\n";
    f << "model = " << decision_model_type_to_string(cfg.model) << "\n";
    f << "api_key = " << cfg.api_key << "\n";
    f << "base_url = " << cfg.base_url << "\n";
    f << "kev_url = " << cfg.kev_url << "\n";
    f << "laya_url = " << cfg.laya_url << "\n";
    f << "jev_url = " << cfg.jev_url << "\n";
    f << "threshold = " << cfg.default_threshold << "\n";
    f << "timeout_secs = " << cfg.timeout_secs << "\n";
    f << "prefer_local = " << (cfg.prefer_local ? "true" : "false") << "\n";
    f << "fallback_to_local = " << (cfg.fallback_to_local ? "true" : "false") << "\n";
    return f.good();
}

bool IsSystemOneEnabled() {
    auto cfg = LoadSystemOneConfig();
    return cfg.enabled;
}

std::string ResolveSystemOneApiKey() {
    // 1. Check environment variable
    const char* env = std::getenv("SYSTEMONE_API_KEY");
    if (env && *env) return std::string(env);

    // 2. Check config file
    auto cfg = LoadSystemOneConfig();
    if (!cfg.api_key.empty()) return cfg.api_key;

    // 3. Check per-provider key file
    std::string key_path = home_dir() + "/config/settings/pluto/keys/SYSTEMONE_API_KEY";
    std::ifstream kf(key_path);
    if (kf) {
        std::string key;
        std::getline(kf, key);
        if (!key.empty()) return key;
    }

    return {};
}

std::string DecisionModelTypeToString(DecisionModelType type) {
    return decision_model_type_to_string(type);
}

DecisionModelType DecisionModelTypeFromString(const std::string& str) {
    return decision_model_type_from_string(str);
}

std::vector<std::string> ListDecisionModels() {
    return { "auto", "kev", "laya", "jev", "local" };
}

} // namespace systemone
} // namespace pluto
