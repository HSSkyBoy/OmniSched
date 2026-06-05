#include "config.h"
#include "project_paths.h"
#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static OmniConfig current_config;

namespace {

std::string trim_copy(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::ifstream open_first_available_config() {
    std::ifstream file;
    for (const auto& config_path : omnisched::config_path_candidates()) {
        file.open(config_path);
        if (file.is_open()) break;
        file.clear();
    }
    return file;
}

}  // namespace

const OmniConfig& OmniConfig::get() { return current_config; }

void OmniConfig::reload() {
    current_config = OmniConfig{};

    std::ifstream file = open_first_available_config();
    if (!file.is_open()) return;

    json data = json::parse(file, nullptr, false);
    if (data.is_discarded()) return;

    current_config.poll_interval_seconds = std::clamp(
        data.value("poll_interval_seconds", current_config.poll_interval_seconds),
        300,
        3600
    );

    if (data.contains("cpuset") && data["cpuset"].is_object()) {
        current_config.background_little_core_only =
            data["cpuset"].value("background_little_core_only", current_config.background_little_core_only);
    } else if (data.contains("background_little_core_only") && data["background_little_core_only"].is_boolean()) {
        current_config.background_little_core_only = data["background_little_core_only"].get<bool>();
    }

    if (data.contains("power") && data["power"].is_object()) {
        std::string policy = data["power"].value("policy", "balanced");
        if (policy == "performance") current_config.power_policy = PowerPolicy::PERFORMANCE;
        else if (policy == "powersave") current_config.power_policy = PowerPolicy::POWERSAVE;
        else current_config.power_policy = PowerPolicy::BALANCED;
    }
    if (data.contains("render") && data["render"].is_object()) {
        auto renderNode = data["render"];
        if (renderNode.contains("force_vulkan") && renderNode["force_vulkan"].is_boolean()) {
            current_config.vulkan_mode =
                renderNode["force_vulkan"].get<bool>() ? VulkanMode::GLOBAL : VulkanMode::OFF;
        } else {
            std::string vMode = renderNode.value("vulkan_mode", "off");
            if (vMode == "global") current_config.vulkan_mode = VulkanMode::GLOBAL;
            else if (vMode == "per_app") current_config.vulkan_mode = VulkanMode::PER_APP;
            else current_config.vulkan_mode = VulkanMode::OFF;
        }
        if (renderNode.contains("vulkan_apps") && renderNode["vulkan_apps"].is_array()) {
            std::unordered_set<std::string> seen_apps;
            for (const auto& app : renderNode["vulkan_apps"]) {
                if (!app.is_string()) continue;

                const std::string package_name = trim_copy(app.get<std::string>());
                if (package_name.empty() || !seen_apps.insert(package_name).second) continue;
                current_config.vulkan_apps.push_back(package_name);
            }
        }
    }
    if (data.contains("performance") && data["performance"].is_object()) {
        auto perfNode = data["performance"];
        current_config.auto_optimize = perfNode.value("auto_optimize", false);
        current_config.lite_mode = perfNode.value("lite_mode", false);
    }
}
