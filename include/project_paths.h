#pragma once
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

namespace omnisched {

constexpr const char* kModuleId = "zygisk_omnisched";
constexpr const char* kModuleConfigDir = "/data/adb/zygisk_omnisched";
constexpr const char* kModuleConfigPath = "/data/adb/zygisk_omnisched/config.json";
constexpr const char* kStableConfigDir = "/data/adb/omnisched";
constexpr const char* kStableConfigPath = "/data/adb/omnisched/config.json";

inline bool file_exists(const char* path) {
    return path != nullptr && access(path, F_OK) == 0;
}

inline void append_unique(std::vector<std::string>& candidates, const std::string& value) {
    if (value.empty()) return;
    for (const auto& candidate : candidates) {
        if (candidate == value) return;
    }
    candidates.push_back(value);
}

inline std::string default_config_dir() {
    return kModuleConfigDir;
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

    if (file_exists(kStableConfigPath) || file_exists(kStableConfigDir)) {
        return kStableConfigDir;
    }

    return default_config_dir();
}

inline std::string resolved_config_path() {
    const char* env_path = std::getenv("OMNISCHED_CONFIG_PATH");
    if (env_path != nullptr && env_path[0] != '\0') {
        return env_path;
    }
    if (file_exists(kStableConfigPath)) {
        return kStableConfigPath;
    }
    return resolved_config_dir() + "/config.json";
}

inline std::vector<std::string> config_dir_candidates() {
    std::vector<std::string> candidates;
    append_unique(candidates, resolved_config_dir());
    append_unique(candidates, kStableConfigDir);
    append_unique(candidates, kModuleConfigDir);
    return candidates;
}

inline std::vector<std::string> config_path_candidates() {
    std::vector<std::string> candidates;
    append_unique(candidates, resolved_config_path());
    append_unique(candidates, kStableConfigPath);
    append_unique(candidates, kModuleConfigPath);
    return candidates;
}

}  // namespace omnisched
