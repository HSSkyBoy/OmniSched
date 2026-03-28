#include "core_sched.h"
#include "cpu_topology.h"
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

void apply_core_optimizations() {
    const auto& topology = CpuTopology::get(); 
    
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
    write_node("/dev/cpuset/background/cpus", topology.cluster_little.c_str());

    if (path_exists("/dev/cpuset/top-app/uclamp.min")) {
        write_node("/dev/cpuset/top-app/uclamp.max", "max");
        write_node("/dev/cpuset/top-app/uclamp.min", "10");
        
        write_node("/dev/cpuset/background/uclamp.max", "50");
        write_node("/dev/cpuset/system-background/uclamp.max", "50");
    }

    if (!topology.best_cpu_governor.empty()) {
        if (DIR* cpu_dir = opendir("/sys/devices/system/cpu/"); cpu_dir) {
            struct dirent* entry;
            while ((entry = readdir(cpu_dir)) != nullptr) {
                if (strncmp(entry->d_name, "cpu", 3) == 0 && isdigit(entry->d_name[3])) {
                    const std::string gov_path = std::string("/sys/devices/system/cpu/") + entry->d_name + "/cpufreq/scaling_governor";
                    write_node(gov_path.c_str(), topology.best_cpu_governor.c_str());
                }
            }
            closedir(cpu_dir);
        }
    }

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
