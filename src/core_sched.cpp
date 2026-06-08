#include "core_sched.h"
#include "cpu_topology.h"
#include "config.h"
#include "root_adapter.h"
#include "utils.h"
#include <vector>
#include <unistd.h>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <dirent.h>
#include <initializer_list>

namespace {
    constexpr const char* AUTO_TOP_APP_UCLAMP_MIN_ENTRY = "12";
    constexpr const char* AUTO_TOP_APP_UCLAMP_MIN_BALANCED = "16";
    constexpr const char* AUTO_TOP_APP_UCLAMP_MIN_PERF = "20";
    constexpr const char* DEFAULT_TOP_APP_UCLAMP_MIN = "10";
    constexpr const char* LITE_TOP_APP_UCLAMP_MAX = "85";
    constexpr const char* DEFAULT_TOP_APP_UCLAMP_MAX = "max";
    constexpr const char* AUTO_BACKGROUND_UCLAMP_MAX_ENTRY = "45";
    constexpr const char* AUTO_BACKGROUND_UCLAMP_MAX_BALANCED = "40";
    constexpr const char* AUTO_BACKGROUND_UCLAMP_MAX_PERF = "32";
    constexpr const char* AUTO_BACKGROUND_UCLAMP_MAX_DEFAULT = "35";
    constexpr const char* LITE_BACKGROUND_UCLAMP_MAX = "40";
    constexpr const char* DEFAULT_BACKGROUND_UCLAMP_MAX = "50";

    struct AutoOptimizeProfile {
        std::string foreground_cpus;
        std::string system_background_cpus;
        std::string background_cpus;
        const char* top_app_uclamp_min;
        const char* background_uclamp_max;
    };

    struct SchedulerRuntimeProfile {
        std::string foreground_cpus;
        std::string system_background_cpus;
        std::string background_cpus;
        std::string governor;
        int scaling_min_freq_khz = -1;
        int scaling_max_freq_khz = -1;
    };

    std::string first_non_empty(std::initializer_list<std::string> values) {
        for (const auto& value : values) {
            if (!value.empty()) return value;
        }
        return {};
    }

    bool name_starts_with(const char* value, const char* prefix) {
        return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
    }

    bool is_tunable_block_device(const char* name) {
        if (!name || name[0] == '.') return false;
        return !name_starts_with(name, "loop")
            && !name_starts_with(name, "ram")
            && !name_starts_with(name, "zram");
    }

    int get_android_api_level() {
        const std::string api = execute_command("getprop ro.build.version.sdk");
        return std::atoi(api.c_str());
    }

    bool has_dedicated_big_cluster(const CpuTopology& topology) {
        return !topology.cluster_big.empty() && topology.cluster_big != topology.cluster_mid;
    }

    bool has_dedicated_mid_cluster(const CpuTopology& topology) {
        return !topology.cluster_mid.empty() && topology.cluster_mid != topology.cluster_big;
    }

    std::string choose_configured_cpuset(const std::string& configured, const std::string& fallback) {
        return configured.empty() ? fallback : normalize_cpuset(configured);
    }

    std::string choose_int_override(int override_value, const char* fallback) {
        if (override_value < 0) return fallback;
        return std::to_string(override_value);
    }

    std::string resolve_system_background_cpus(const CpuTopology& topology) {
        return first_non_empty({
            combine_cpus(topology.cluster_little, topology.cluster_mid),
            topology.cluster_little,
            topology.cluster_mid,
            topology.all_cores
        });
    }

    std::string resolve_background_cpus(const CpuTopology& topology, bool little_core_only) {
        const std::string system_background_cpus = resolve_system_background_cpus(topology);
        if (!little_core_only) return system_background_cpus;
        return first_non_empty({
            topology.cluster_little,
            system_background_cpus,
            topology.all_cores
        });
    }

    const char* pick_auto_top_app_uclamp_min(int cpu_count, bool dedicated_big_cluster) {
        if (cpu_count <= 4) return AUTO_TOP_APP_UCLAMP_MIN_ENTRY;
        if (cpu_count <= 6) return AUTO_TOP_APP_UCLAMP_MIN_BALANCED;
        return dedicated_big_cluster ? AUTO_TOP_APP_UCLAMP_MIN_PERF : AUTO_TOP_APP_UCLAMP_MIN_BALANCED;
    }

    const char* pick_auto_background_uclamp_max(int cpu_count, bool dedicated_big_cluster) {
        if (cpu_count <= 4) return AUTO_BACKGROUND_UCLAMP_MAX_ENTRY;
        if (cpu_count <= 6) return AUTO_BACKGROUND_UCLAMP_MAX_BALANCED;
        return dedicated_big_cluster ? AUTO_BACKGROUND_UCLAMP_MAX_PERF : AUTO_BACKGROUND_UCLAMP_MAX_DEFAULT;
    }

    AutoOptimizeProfile build_auto_optimize_profile(const CpuTopology& topology) {
        const int cpu_count = count_cpus_in_cpuset(topology.all_cores);
        const bool dedicated_big_cluster = has_dedicated_big_cluster(topology);
        const bool dedicated_mid_cluster = has_dedicated_mid_cluster(topology);

        std::string system_background_cpus = resolve_system_background_cpus(topology);
        std::string foreground_cpus = topology.all_cores;

        if (cpu_count >= 8 && dedicated_big_cluster && dedicated_mid_cluster) {
            foreground_cpus = combine_cpus(topology.cluster_little, topology.cluster_mid);
        }
        if (foreground_cpus.empty()) foreground_cpus = first_non_empty({topology.cluster_mid, topology.all_cores});

        std::string background_cpus = resolve_background_cpus(topology, true);

        return AutoOptimizeProfile{
                foreground_cpus,
                system_background_cpus,
                background_cpus,
                pick_auto_top_app_uclamp_min(cpu_count, dedicated_big_cluster),
                pick_auto_background_uclamp_max(cpu_count, dedicated_big_cluster)
        };
    }

    bool is_mtk_soc() {
        std::string soc = execute_command("getprop ro.soc.manufacturer");
        std::transform(soc.begin(), soc.end(), soc.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return soc.find("mediatek") != std::string::npos || soc.find("mtk") != std::string::npos;
    }

    void apply_render_engine_optimizations(const OmniConfig& config, const IRootAdapter& root) {
        if (config.vulkan_mode == VulkanMode::OFF || config.vulkan_mode == VulkanMode::PER_APP) {
            return;
        }

        const int api = get_android_api_level();
        const bool mtk = is_mtk_soc();

        if (mtk || api < 34) {
            root.set_system_prop("ro.hwui.renderer", "skiavk");
            root.set_system_prop("debug.hwui.renderer", "skiavk");
            root.set_system_prop("debug.renderengine.backend", "skiavk");
            root.set_system_prop("ro.hwui.use_vulkan", "true");
            root.set_system_prop("debug.renderengine.graphite", "false");
        } else {
            root.set_system_prop("ro.hwui.renderer", "skia");
            root.set_system_prop("debug.hwui.renderer", "skia");
            root.set_system_prop("debug.renderengine.backend", "skiavk");
            root.set_system_prop("ro.hwui.use_vulkan", "true");
            root.set_system_prop("debug.renderengine.graphite", "true");
        }

        root.set_system_prop("debug.vulkan.layers", "");
        root.set_system_prop("debug.hwui.skia_tracing_enabled", "false");
        root.set_system_prop("debug.renderengine.vulkan.precompile.enabled", "true");
    }

    void write_node_if_exists(const std::string& path, const char* value) {
        if (!value || !path_exists(path.c_str())) return;
        write_node(path.c_str(), value);
    }

    void write_uclamp_group(const char* group, const char* min_value, const char* max_value, const char* latency_sensitive) {
        const std::string group_name(group);
        const std::string cpuset_base = "/dev/cpuset/" + group_name;
        const std::string cpuctl_base = "/dev/cpuctl/" + group_name;

        write_node_if_exists(cpuset_base + "/uclamp.min", min_value);
        write_node_if_exists(cpuset_base + "/uclamp.max", max_value);
        write_node_if_exists(cpuset_base + "/uclamp.latency_sensitive", latency_sensitive);

        write_node_if_exists(cpuctl_base + "/cpu.uclamp.min", min_value);
        write_node_if_exists(cpuctl_base + "/cpu.uclamp.max", max_value);
        write_node_if_exists(cpuctl_base + "/cpu.uclamp.latency_sensitive", latency_sensitive);
    }

    void write_schedtune_group(const char* group, const char* boost, const char* prefer_idle) {
        const std::string base = std::string("/dev/stune/") + group;
        write_node_if_exists(base + "/schedtune.boost", boost);
        write_node_if_exists(base + "/schedtune.prefer_idle", prefer_idle);
    }

    void write_schedutil_rate_limits(const char* up_rate_limit, const char* down_rate_limit, const char* iowait_boost) {
        if (DIR* cpufreq_dir = opendir("/sys/devices/system/cpu/cpufreq/"); cpufreq_dir) {
            struct dirent* entry;
            while ((entry = readdir(cpufreq_dir)) != nullptr) {
                if (strncmp(entry->d_name, "policy", 6) != 0) continue;

                const std::string base = std::string("/sys/devices/system/cpu/cpufreq/") + entry->d_name;
                write_node_if_exists(base + "/schedutil/up_rate_limit_us", up_rate_limit);
                write_node_if_exists(base + "/schedutil/down_rate_limit_us", down_rate_limit);
                write_node_if_exists(base + "/schedutil/iowait_boost_enable", iowait_boost);
            }
            closedir(cpufreq_dir);
        }
    }

    void write_cpu_freq_limits(int min_freq_khz, int max_freq_khz) {
        if (min_freq_khz < 0 && max_freq_khz < 0) return;

        if (DIR* cpufreq_dir = opendir("/sys/devices/system/cpu/cpufreq/"); cpufreq_dir) {
            struct dirent* entry;
            while ((entry = readdir(cpufreq_dir)) != nullptr) {
                if (std::strncmp(entry->d_name, "policy", 6) != 0) continue;

                const std::string base = std::string("/sys/devices/system/cpu/cpufreq/") + entry->d_name;
                if (min_freq_khz >= 0) {
                    write_node_if_exists(base + "/scaling_min_freq", std::to_string(min_freq_khz).c_str());
                }
                if (max_freq_khz >= 0) {
                    write_node_if_exists(base + "/scaling_max_freq", std::to_string(max_freq_khz).c_str());
                }
            }
            closedir(cpufreq_dir);
        }
    }

    void tune_schedutil_rate_limits(const OmniConfig& config) {
        const char* up_rate_limit = "1000";
        const char* down_rate_limit = "20000";
        const char* iowait_boost = "1";

        if (config.power_policy == PowerPolicy::PERFORMANCE) {
            up_rate_limit = "500";
            down_rate_limit = "10000";
        } else if (config.power_policy == PowerPolicy::POWERSAVE) {
            up_rate_limit = "5000";
            down_rate_limit = "50000";
            iowait_boost = "0";
        } else if (config.lite_mode) {
            up_rate_limit = "3000";
            down_rate_limit = "30000";
        } else if (config.auto_optimize) {
            up_rate_limit = "1000";
            down_rate_limit = "18000";
        }

        const std::string up_rate_limit_value =
            choose_int_override(config.scheduler.schedutil_up_rate_limit_us, up_rate_limit);
        const std::string down_rate_limit_value =
            choose_int_override(config.scheduler.schedutil_down_rate_limit_us, down_rate_limit);
        const std::string iowait_boost_value =
            choose_int_override(config.scheduler.schedutil_iowait_boost, iowait_boost);

        write_schedutil_rate_limits(
            up_rate_limit_value.c_str(),
            down_rate_limit_value.c_str(),
            iowait_boost_value.c_str()
        );
    }

    void apply_scheduler_optimizations(const OmniConfig& config, const CpuTopology& topology) {
        if (!config.scheduler_tune) return;

        const char* top_min = DEFAULT_TOP_APP_UCLAMP_MIN;
        const char* top_max = DEFAULT_TOP_APP_UCLAMP_MAX;
        const char* foreground_min = "0";
        const char* group_max = DEFAULT_BACKGROUND_UCLAMP_MAX;
        const char* top_boost = "8";
        const char* foreground_boost = "0";
        const char* background_boost = "0";
        const char* top_prefer_idle = "1";

        if (config.power_policy == PowerPolicy::PERFORMANCE) {
            top_min = "30";
            top_max = "max";
            foreground_min = "8";
            group_max = "45";
            top_boost = "12";
            foreground_boost = "2";
        } else if (config.power_policy == PowerPolicy::POWERSAVE) {
            top_min = "0";
            top_max = "75";
            foreground_min = "0";
            group_max = "30";
            top_boost = "0";
            top_prefer_idle = "0";
        } else if (config.auto_optimize) {
            const auto profile = build_auto_optimize_profile(topology);
            top_min = profile.top_app_uclamp_min;
            top_max = "max";
            group_max = profile.background_uclamp_max;
            top_boost = "6";
        } else if (config.lite_mode) {
            top_min = "8";
            top_max = LITE_TOP_APP_UCLAMP_MAX;
            group_max = LITE_BACKGROUND_UCLAMP_MAX;
            top_boost = "4";
        }

        const std::string top_min_value = choose_int_override(config.scheduler.top_app_uclamp_min, top_min);
        const std::string foreground_min_value =
            choose_int_override(config.scheduler.foreground_uclamp_min, foreground_min);
        const std::string group_max_value =
            choose_int_override(config.scheduler.background_uclamp_max, group_max);

        write_uclamp_group("top-app", top_min_value.c_str(), top_max, "1");
        write_uclamp_group("foreground", foreground_min_value.c_str(), "max", "0");
        write_uclamp_group("background", "0", group_max_value.c_str(), "0");
        write_uclamp_group("system-background", "0", group_max_value.c_str(), "0");

        write_schedtune_group("top-app", top_boost, top_prefer_idle);
        write_schedtune_group("foreground", foreground_boost, "0");
        write_schedtune_group("background", background_boost, "0");
        write_schedtune_group("system-background", background_boost, "0");

        tune_schedutil_rate_limits(config);
    }

    void apply_io_optimizations(const OmniConfig& config) {
        if (!config.io_tune) return;

        const char* read_ahead_kb = "128";
        const char* nr_requests = "64";
        const char* rq_affinity = "2";

        if (config.power_policy == PowerPolicy::PERFORMANCE) {
            read_ahead_kb = "256";
            nr_requests = "128";
        } else if (config.power_policy == PowerPolicy::POWERSAVE) {
            read_ahead_kb = "64";
            nr_requests = "32";
            rq_affinity = "1";
        } else if (config.lite_mode) {
            read_ahead_kb = "96";
            nr_requests = "48";
            rq_affinity = "1";
        }

        if (DIR* block_dir = opendir("/sys/block/"); block_dir) {
            struct dirent* entry;
            while ((entry = readdir(block_dir)) != nullptr) {
                if (!is_tunable_block_device(entry->d_name)) continue;

                const std::string base = std::string("/sys/block/") + entry->d_name + "/queue";
                write_node_if_exists(base + "/read_ahead_kb", read_ahead_kb);
                write_node_if_exists(base + "/nr_requests", nr_requests);
                write_node_if_exists(base + "/rq_affinity", rq_affinity);
                write_node_if_exists(base + "/iostats", "0");
                write_node_if_exists(base + "/add_random", "0");
            }
            closedir(block_dir);
        }
    }

    void apply_gpu_optimizations(const OmniConfig& config) {
        if (!config.gpu_tune || config.lite_mode || config.power_policy == PowerPolicy::POWERSAVE) return;

        const char* adreno_path = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
        if (path_exists(adreno_path)) {
            write_node(adreno_path, "msm-adreno-tz");
        } else if (DIR* devfreq_dir = opendir("/sys/class/devfreq/"); devfreq_dir) {
            struct dirent* entry;
            while ((entry = readdir(devfreq_dir)) != nullptr) {
                if (entry->d_name[0] == '.') continue;
                const std::string gov_path = std::string("/sys/class/devfreq/") + entry->d_name + "/governor";
                const std::string avail_path = std::string("/sys/class/devfreq/") + entry->d_name + "/available_governors";
                const std::string avail_govs = read_node(avail_path.c_str());

                if (avail_govs.find("mali_ondemand") != std::string::npos) {
                    write_node(gov_path.c_str(), "mali_ondemand");
                } else if (avail_govs.find("simple_ondemand") != std::string::npos) {
                    write_node(gov_path.c_str(), "simple_ondemand");
                }
            }
            closedir(devfreq_dir);
        }
    }

    void apply_input_boost_optimizations(const OmniConfig& config) {
        if (!config.input_boost) return;

        const char* boost_ms = "80";
        if (config.power_policy == PowerPolicy::PERFORMANCE) {
            boost_ms = "120";
        } else if (config.power_policy == PowerPolicy::POWERSAVE || config.lite_mode) {
            boost_ms = "40";
        }

        const std::string boost_ms_value = choose_int_override(config.input.boost_ms, boost_ms);

        write_node_if_exists("/sys/module/cpu_boost/parameters/input_boost_enabled", "1");
        write_node_if_exists("/sys/module/cpu_boost/parameters/input_boost_ms", boost_ms_value.c_str());
        write_node_if_exists("/sys/module/cpu_input_boost/parameters/input_boost_enabled", "1");
        write_node_if_exists("/sys/module/cpu_input_boost/parameters/input_boost_ms", boost_ms_value.c_str());
        write_node_if_exists("/sys/module/msm_performance/parameters/touchboost", "1");
    }

    int normalize_temperature_milli_degrees(int value) {
        if (value <= 0) return 0;
        return value < 1000 ? value * 1000 : value;
    }

    int read_max_thermal_milli_degrees() {
        int max_temp = 0;
        if (DIR* thermal_dir = opendir("/sys/class/thermal/"); thermal_dir) {
            struct dirent* entry;
            while ((entry = readdir(thermal_dir)) != nullptr) {
                if (!name_starts_with(entry->d_name, "thermal_zone")) continue;

                const std::string temp_path = std::string("/sys/class/thermal/") + entry->d_name + "/temp";
                const int temp = normalize_temperature_milli_degrees(std::atoi(read_node(temp_path.c_str()).c_str()));
                if (temp > 0 && temp < 125000) max_temp = std::max(max_temp, temp);
            }
            closedir(thermal_dir);
        }
        return max_temp;
    }

    void apply_thermal_guard(const OmniConfig& config) {
        if (!config.thermal_guard) return;

        const int max_temp = read_max_thermal_milli_degrees();
        int threshold = config.power_policy == PowerPolicy::PERFORMANCE
            ? 52000
            : ((config.power_policy == PowerPolicy::POWERSAVE || config.lite_mode) ? 44000 : 48000);
        if (config.thermal.throttle_temp_c >= 0) {
            threshold = config.thermal.throttle_temp_c * 1000;
        }
        if (max_temp < threshold) return;

        const char* top_max = config.power_policy == PowerPolicy::POWERSAVE ? "60" : "70";
        const char* foreground_max = "75";
        const char* background_max = "25";
        const std::string top_max_value = choose_int_override(config.thermal.top_app_uclamp_max, top_max);
        const std::string foreground_max_value =
            choose_int_override(config.thermal.foreground_uclamp_max, foreground_max);
        const std::string background_max_value =
            choose_int_override(config.thermal.background_uclamp_max, background_max);

        write_uclamp_group("top-app", "0", top_max_value.c_str(), "0");
        write_uclamp_group("foreground", "0", foreground_max_value.c_str(), "0");
        write_uclamp_group("background", "0", background_max_value.c_str(), "0");
        write_uclamp_group("system-background", "0", background_max_value.c_str(), "0");
        write_schedtune_group("top-app", "0", "0");
        write_schedtune_group("foreground", "0", "0");
        write_schedutil_rate_limits("5000", "50000", "0");
    }

    SchedulerRuntimeProfile build_scheduler_runtime_profile(const OmniConfig& config, const CpuTopology& topology) {
        SchedulerRuntimeProfile profile;

        const std::string system_background_cpus = resolve_system_background_cpus(topology);
        const std::string strict_background_cpus = resolve_background_cpus(topology, true);
        const std::string relaxed_background_cpus = resolve_background_cpus(topology, false);

        profile.foreground_cpus = topology.all_cores;
        profile.system_background_cpus = system_background_cpus;
        profile.background_cpus = config.background_little_core_only ? strict_background_cpus : relaxed_background_cpus;

        if (config.power_policy == PowerPolicy::PERFORMANCE) {
            profile.foreground_cpus = topology.all_cores;
            profile.system_background_cpus = topology.all_cores;
            profile.background_cpus = relaxed_background_cpus;
        } else if (config.power_policy == PowerPolicy::POWERSAVE) {
            profile.foreground_cpus = first_non_empty({
                combine_cpus(topology.cluster_little, topology.cluster_mid),
                topology.cluster_little,
                system_background_cpus,
                topology.all_cores
            });
            profile.system_background_cpus = strict_background_cpus;
            profile.background_cpus = strict_background_cpus;
        } else if (config.auto_optimize) {
            const auto auto_profile = build_auto_optimize_profile(topology);
            profile.foreground_cpus = auto_profile.foreground_cpus;
            profile.system_background_cpus = auto_profile.system_background_cpus;
            profile.background_cpus = auto_profile.background_cpus;
        }

        profile.foreground_cpus = choose_configured_cpuset(config.cpu.foreground_cpuset, profile.foreground_cpus);
        profile.system_background_cpus = choose_configured_cpuset(
            config.cpu.system_background_cpuset,
            profile.system_background_cpus
        );
        profile.background_cpus = choose_configured_cpuset(config.cpu.background_cpuset, profile.background_cpus);

        profile.scaling_min_freq_khz = config.cpu.scaling_min_freq_khz;
        profile.scaling_max_freq_khz = config.cpu.scaling_max_freq_khz;
        profile.governor = config.cpu.governor_override;
        return profile;
    }
} // namespace

void init_daemon() {
    // 切換目錄至 "/" 並將標準輸出導向 /dev/null
    if (daemon(0, 0) < 0) exit(EXIT_FAILURE);
}

void apply_memory_optimizations(const OmniConfig& config) {
    if (!config.memory_tune) return;

    const bool gentle_profile = config.power_policy == PowerPolicy::POWERSAVE || config.lite_mode;

    // 啟用 Multi-Gen LRU (Android 14+ 預設啟用)
    write_node_if_exists("/sys/kernel/mm/lru_gen/enabled", "7");
    // 最佳化 ZRAM 與 Page Swap 行為
    write_node_if_exists("/proc/sys/vm/swappiness", gentle_profile ? "60" : "100");
    // 降低記憶體分配延遲
    write_node_if_exists("/proc/sys/vm/watermark_scale_factor", gentle_profile ? "35" : "20");
    write_node_if_exists("/proc/sys/vm/page-cluster", "0");
    write_node_if_exists("/proc/sys/vm/compaction_proactiveness", gentle_profile ? "20" : "40");
}

void apply_core_optimizations() {
    OmniConfig::reload();

    const auto& topology = CpuTopology::get();
    const auto& config = OmniConfig::get();
    const auto& root = RootEnvironment::get_adapter();

    apply_memory_optimizations(config);

    write_node("/dev/cpuset/top-app/cpus", topology.all_cores.c_str());
    const auto runtime = build_scheduler_runtime_profile(config, topology);
    write_node("/dev/cpuset/foreground/cpus", runtime.foreground_cpus.c_str());
    write_node("/dev/cpuset/system-background/cpus", runtime.system_background_cpus.c_str());
    write_node("/dev/cpuset/background/cpus", runtime.background_cpus.c_str());

    if (topology.has_cpufreq && !config.lite_mode) {
        if (DIR* cpu_dir = opendir("/sys/devices/system/cpu/"); cpu_dir) {
            struct dirent* entry;
            while ((entry = readdir(cpu_dir)) != nullptr) {
                if (strncmp(entry->d_name, "cpu", 3) == 0 && std::isdigit(static_cast<unsigned char>(entry->d_name[3]))) {
                    const std::string avail_path = std::string("/sys/devices/system/cpu/") + entry->d_name + "/cpufreq/scaling_available_governors";
                    const std::string gov_path = std::string("/sys/devices/system/cpu/") + entry->d_name + "/cpufreq/scaling_governor";

                    std::string target_gov = runtime.governor.empty() ? topology.best_cpu_governor : runtime.governor;
                    const std::string avail_govs = read_node(avail_path.c_str());

                    if (config.power_policy == PowerPolicy::PERFORMANCE && avail_govs.find("performance") != std::string::npos) {
                        target_gov = "performance";
                    } else if (config.power_policy == PowerPolicy::POWERSAVE && avail_govs.find("schedutil") != std::string::npos) {
                        target_gov = "schedutil";
                    } else if (!runtime.governor.empty() && avail_govs.find(runtime.governor) == std::string::npos) {
                        target_gov = topology.best_cpu_governor;
                    }
                    write_node(gov_path.c_str(), target_gov.c_str());
                }
            }
            closedir(cpu_dir);
        }

        int min_freq_khz = runtime.scaling_min_freq_khz;
        int max_freq_khz = runtime.scaling_max_freq_khz;
        if (config.power_policy == PowerPolicy::POWERSAVE && topology.max_freq_khz > 0 && max_freq_khz < 0) {
            max_freq_khz = (topology.max_freq_khz * 75) / 100;
        } else if (config.power_policy == PowerPolicy::PERFORMANCE && topology.max_freq_khz > 0 && min_freq_khz < 0) {
            min_freq_khz = (topology.max_freq_khz * 55) / 100;
        }
        if (max_freq_khz >= 0 && min_freq_khz > max_freq_khz) {
            min_freq_khz = max_freq_khz;
        }
        write_cpu_freq_limits(min_freq_khz, max_freq_khz);
    }

    apply_scheduler_optimizations(config, topology);
    apply_io_optimizations(config);
    apply_gpu_optimizations(config);
    apply_input_boost_optimizations(config);
    apply_thermal_guard(config);
    apply_render_engine_optimizations(config, root);
}
