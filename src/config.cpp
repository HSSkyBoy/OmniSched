#include "config.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static OmniConfig current_config;

const OmniConfig& OmniConfig::get() {
    return current_config;
}

void OmniConfig::reload() {
    std::ifstream file("/data/adb/omnisched/config.json");
    if (!file.is_open()) return;

    json data = json::parse(file, nullptr, false);
    if (data.is_discarded()) return;

    current_config.poll_interval_seconds = data.value("poll_interval_seconds", 950);
    
    if (data.contains("cpuset") && data["cpuset"].is_object()) {
        current_config.background_little_core_only = 
            data["cpuset"].value("background_little_core_only", true);
    }
    
    if (data.contains("render") && data["render"].is_object()) {
        current_config.force_vulkan = 
            data["render"].value("force_vulkan", false);
    }

    current_config.game_mode_enabled = data.value("game_mode_enabled", true);
    current_config.lite_mode_enabled = data.value("lite_mode_enabled", false);
    
    std::string profile_str = data.value("current_profile", "balance");
    if (profile_str == "performance") current_config.current_profile = ProfileMode::PERFORMANCE;
    else if (profile_str == "powersave") current_config.current_profile = ProfileMode::POWERSAVE;
    else current_config.current_profile = ProfileMode::BALANCE;

    current_config.gamelist.clear();
    if (data.contains("gamelist") && data["gamelist"].is_array()) {
        for (const auto& item : data["gamelist"]) {
            if (item.is_string()) {
                current_config.gamelist.insert(item.get<std::string>());
            }
        }
    }
}
