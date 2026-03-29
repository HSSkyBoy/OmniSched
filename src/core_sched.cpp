#include "core_sched.h"
#include "cpu_topology.h"
#include "config.h"
#include "root_adapter.h"
#include "utils.h"
#include <vector>
#include <unistd.h>
#include <dirent.h>

void init_daemon() {
    // 切換目錄至 "/" 並將標準輸出導向 /dev/null
    if (daemon(0, 0) < 0) {
        exit(EXIT_FAILURE);
    }
}

void apply_memory_optimizations() {
    // 啟用 Multi-Gen LRU (Android 14+ 預設啟用)
    if (path_exists("/sys/kernel/mm/lru_gen/enabled")) {
        write_node("/sys/kernel/mm/lru_gen/enabled", "7"); 
    }
    // 最佳化 ZRAM 與 Page Swap 行為
    if (path_exists("/proc/sys/vm/swappiness")) {
        write_node("/proc/sys/vm/swappiness", "100");
    }
    // 降低記憶體分配延遲
    if (path_exists("/proc/sys/vm/watermark_scale_factor")) {
        write_node("/proc/sys/vm/watermark_scale_factor", "20"); 
    }
}

void apply_base_optimizations() {
    const auto& topology = CpuTopology::get(); 
    const auto& config = OmniConfig::get();
    const auto& root = RootEnvironment::get_adapter();

    apply_memory_optimizations();

    write_node("/dev/cpuset/top-app/cpus", topology.all_cores.c_str());
    
    if (!topology.cluster_mid.empty() && topology.cluster_mid != topology.cluster_big) {
        const std::string fg_cpus = combine_cpus(topology.cluster_little, topology.cluster_mid);
        write_node("/dev/cpuset/foreground/cpus", fg_cpus.c_str());
    } else {
        write_node("/dev/cpuset/foreground/cpus", topology.all_cores.c_str());
    }
    
    const std::string sys_bg_cpus = combine_cpus(topology.cluster_little, topology.cluster_mid);
    write_node("/dev/cpuset/system-background/cpus", sys_bg_cpus.c_str());

    if (config.background_little_core_only) {
        write_node("/dev/cpuset/background/cpus", topology.cluster_little.c_str());
    } else {
        write_node("/dev/cpuset/background/cpus", sys_bg_cpus.c_str());
    }

    if (path_exists("/dev/cpuset/background/uclamp.max")) {
        write_node("/dev/cpuset/background/uclamp.max", "50");
        write_node("/dev/cpuset/system-background/uclamp.max", "50");
    }

    const char* adreno_path = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
    if (path_exists(adreno_path)) write_node(adreno_path, "msm-adreno-tz");

    if (config.force_vulkan) {
        root.set_system_prop("debug.hwui.renderer", "skiavk");
    } else {
        root.set_system_prop("debug.hwui.renderer", "skiagl");
    }
}

void apply_dynamic_profile(ProfileMode mode, bool is_lite) {
    const auto& topology = CpuTopology::get();

    if (mode == ProfileMode::PERFORMANCE) {
        // Lite Mode 放寬 CPU 調度器限制
        std::string gov = is_lite ? topology.best_cpu_governor : "performance";
        set_cpu_governor(gov);
        
        if (path_exists("/dev/cpuset/top-app/uclamp.min")) {
            write_node("/dev/cpuset/top-app/uclamp.min", is_lite ? "20" : "50");
            write_node("/dev/cpuset/top-app/uclamp.max", "max");
        }
        write_node("/sys/class/kgsl/kgsl-3d0/force_clk_on", is_lite ? "0" : "1");
    } 
    else if (mode == ProfileMode::BALANCE) {
        set_cpu_governor(topology.best_cpu_governor);
        if (path_exists("/dev/cpuset/top-app/uclamp.min")) {
            write_node("/dev/cpuset/top-app/uclamp.min", "10");
            write_node("/dev/cpuset/top-app/uclamp.max", "max");
        }
        write_node("/sys/class/kgsl/kgsl-3d0/force_clk_on", "0");
    }
    else if (mode == ProfileMode::POWERSAVE) {
        set_cpu_governor("powersave"); 
        if (path_exists("/dev/cpuset/top-app/uclamp.max")) {
            write_node("/dev/cpuset/top-app/uclamp.min", "0");
            write_node("/dev/cpuset/top-app/uclamp.max", "50");
        }
        write_node("/sys/class/kgsl/kgsl-3d0/force_clk_on", "0");
    }
}
