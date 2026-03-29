#pragma once
#include <string>
#include <unordered_set>

enum class ProfileMode {
    PERFORMANCE,
    BALANCE,
    POWERSAVE
};

struct OmniConfig {
    int poll_interval_seconds = 950;
    bool background_little_core_only = true;
    bool force_vulkan = false;
    // 三級動態設定檔
    ProfileMode current_profile = ProfileMode::BALANCE;
    bool game_mode_enabled = true;
    bool lite_mode_enabled = false;
    std::unordered_set<std::string> gamelist;
    
    static const OmniConfig& get();
    static void reload();
};
