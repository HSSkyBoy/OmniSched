#include "config.h"
#include "project_paths.h"
#include "utils.h"
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

int read_clamped_int(const json& node, const char* key, int fallback, int min_value, int max_value) {
    if (!node.contains(key) || !node[key].is_number_integer()) return fallback;
    return std::clamp(node[key].get<int>(), min_value, max_value);
}

std::string read_cpuset_value(const json& node, const char* key) {
    if (!node.contains(key) || !node[key].is_string()) return {};
    return normalize_cpuset(trim_copy(node[key].get<std::string>()));
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
        current_config.scheduler_tune = perfNode.value("scheduler_tune", true);
        current_config.memory_tune = perfNode.value("memory_tune", true);
        current_config.io_tune = perfNode.value("io_tune", true);
        current_config.gpu_tune = perfNode.value("gpu_tune", true);
        current_config.input_boost = perfNode.value("input_boost", true);
        current_config.thermal_guard = perfNode.value("thermal_guard", true);
    }

    if (data.contains("scheduler") && data["scheduler"].is_object()) {
        const auto& scheduler_node = data["scheduler"];
        current_config.scheduler.top_app_uclamp_min =
            read_clamped_int(scheduler_node, "top_app_uclamp_min", current_config.scheduler.top_app_uclamp_min, 0, 100);
        current_config.scheduler.foreground_uclamp_min =
            read_clamped_int(scheduler_node, "foreground_uclamp_min", current_config.scheduler.foreground_uclamp_min, 0, 100);
        current_config.scheduler.background_uclamp_max =
            read_clamped_int(scheduler_node, "background_uclamp_max", current_config.scheduler.background_uclamp_max, 0, 100);
        current_config.scheduler.schedutil_up_rate_limit_us =
            read_clamped_int(scheduler_node, "schedutil_up_rate_limit_us", current_config.scheduler.schedutil_up_rate_limit_us, 0, 100000);
        current_config.scheduler.schedutil_down_rate_limit_us =
            read_clamped_int(scheduler_node, "schedutil_down_rate_limit_us", current_config.scheduler.schedutil_down_rate_limit_us, 0, 100000);
        current_config.scheduler.schedutil_iowait_boost =
            read_clamped_int(scheduler_node, "schedutil_iowait_boost", current_config.scheduler.schedutil_iowait_boost, 0, 1);
    }

    if (data.contains("cpu") && data["cpu"].is_object()) {
        const auto& cpu_node = data["cpu"];
        if (cpu_node.contains("governor_override") && cpu_node["governor_override"].is_string()) {
            current_config.cpu.governor_override = trim_copy(cpu_node["governor_override"].get<std::string>());
        }
        current_config.cpu.foreground_cpuset = read_cpuset_value(cpu_node, "foreground_cpuset");
        current_config.cpu.system_background_cpuset = read_cpuset_value(cpu_node, "system_background_cpuset");
        current_config.cpu.background_cpuset = read_cpuset_value(cpu_node, "background_cpuset");
        current_config.cpu.scaling_min_freq_khz =
            read_clamped_int(cpu_node, "scaling_min_freq_khz", current_config.cpu.scaling_min_freq_khz, 0, 10000000);
        current_config.cpu.scaling_max_freq_khz =
            read_clamped_int(cpu_node, "scaling_max_freq_khz", current_config.cpu.scaling_max_freq_khz, 0, 10000000);
    }

    if (data.contains("input") && data["input"].is_object()) {
        const auto& input_node = data["input"];
        current_config.input.boost_ms =
            read_clamped_int(input_node, "boost_ms", current_config.input.boost_ms, 0, 5000);
    }

    if (data.contains("thermal") && data["thermal"].is_object()) {
        const auto& thermal_node = data["thermal"];
        current_config.thermal.throttle_temp_c =
            read_clamped_int(thermal_node, "throttle_temp_c", current_config.thermal.throttle_temp_c, 30, 95);
        current_config.thermal.top_app_uclamp_max =
            read_clamped_int(thermal_node, "top_app_uclamp_max", current_config.thermal.top_app_uclamp_max, 0, 100);
        current_config.thermal.foreground_uclamp_max =
            read_clamped_int(thermal_node, "foreground_uclamp_max", current_config.thermal.foreground_uclamp_max, 0, 100);
        current_config.thermal.background_uclamp_max =
            read_clamped_int(thermal_node, "background_uclamp_max", current_config.thermal.background_uclamp_max, 0, 100);
    }
}
