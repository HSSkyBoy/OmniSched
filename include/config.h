#pragma once
#include <string>
#include <vector>

enum class VulkanMode { OFF, GLOBAL, PER_APP };
enum class PowerPolicy { BALANCED, PERFORMANCE, POWERSAVE };

struct SchedulerConfig {
    int top_app_uclamp_min = -1;
    int foreground_uclamp_min = -1;
    int background_uclamp_max = -1;
    int schedutil_up_rate_limit_us = -1;
    int schedutil_down_rate_limit_us = -1;
    int schedutil_iowait_boost = -1;
};

struct CpuControlConfig {
    std::string governor_override;
    std::string foreground_cpuset;
    std::string system_background_cpuset;
    std::string background_cpuset;
    int scaling_min_freq_khz = -1;
    int scaling_max_freq_khz = -1;
};

struct InputBoostConfig {
    int boost_ms = -1;
};

struct ThermalConfig {
    int throttle_temp_c = -1;
    int top_app_uclamp_max = -1;
    int foreground_uclamp_max = -1;
    int background_uclamp_max = -1;
};

struct DisplayConfig {
    bool short_video_refresh_rate_enabled = false;
    int short_video_refresh_rate_hz = 30;
    std::vector<std::string> short_video_apps;
};

struct OmniConfig {
    int poll_interval_seconds = 950;

    VulkanMode vulkan_mode = VulkanMode::OFF;
    std::vector<std::string> vulkan_apps;

    PowerPolicy power_policy = PowerPolicy::BALANCED;
    bool background_little_core_only = false;
    bool auto_optimize = false;
    bool lite_mode = false;
    bool scheduler_tune = false;
    bool memory_tune = false;
    bool io_tune = false;
    bool gpu_tune = false;
    bool input_boost = true;
    bool thermal_guard = true;
    SchedulerConfig scheduler;
    CpuControlConfig cpu;
    InputBoostConfig input;
    ThermalConfig thermal;
    DisplayConfig display;

    static const OmniConfig& get();
    static void reload();
};
