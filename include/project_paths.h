#pragma once
#include <cstdlib>
#include <string>
#include <vector>

namespace omnisched {

constexpr const char* kModuleId = "zygisk_omnisched";
constexpr const char* kLegacyConfigDir = "/data/adb/zygisk_omnisched";
constexpr const char* kLegacyConfigPath = "/data/adb/zygisk_omnisched/config.json";

inline std::string default_config_dir() {
    return std::string("/data/adb/") + kModuleId;
}

inline std::string resolved_config_dir() {
    const char* env_dir = std::getenv("OMNISCHED_CONFIG_DIR");
    if (env_dir != nullptr && env_dir[0] != '\0') {
        return env_dir;
    }

    const char* env_path = std::getenv("OMNISCHED_CONFIG_PATH");
    if (env_path != nullptr && env_path[0] != '\0') {
        const std::string path(env_path);
        const std::size_t slash = path.find_last_of('/');
        if (slash != std::string::npos) {
            return path.substr(0, slash);
        }
    }

    return default_config_dir();
}

inline std::string resolved_config_path() {
    const char* env_path = std::getenv("OMNISCHED_CONFIG_PATH");
    if (env_path != nullptr && env_path[0] != '\0') {
        return env_path;
    }
    return resolved_config_dir() + "/config.json";
}

inline std::vector<std::string> config_path_candidates() {
    std::vector<std::string> candidates;
    candidates.push_back(resolved_config_path());
    if (candidates.front() != kLegacyConfigPath) {
        candidates.emplace_back(kLegacyConfigPath);
    }
    return candidates;
}

}  // namespace omnisched
