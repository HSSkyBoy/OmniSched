#pragma once

#include <string>
#include <vector>
#include <optional>
#include <dirent.h>
#include <algorithm>
#include <cstring>
#include "utils.h"

struct CpuTopology {
    const bool is_mtk;
    const std::string all_cores;
    const std::string cluster_little;
    const std::string cluster_mid;
    const std::string cluster_big;
    const std::string best_cpu_governor;

    static const CpuTopology& get() {
        static const CpuTopology instance = []() {
            // 讀取基礎資訊
            const std::string platform = execute_command("getprop ro.board.platform");
            const bool mtk = (platform.find("mt") != std::string::npos);
            const std::string cores = read_node_opt("/sys/devices/system/cpu/possible").value_or("0-7");

            // 掃描 CPU 叢集 Policy
            std::vector<std::string> policies;
            const char* cpufreq_dir_path = "/sys/devices/system/cpu/cpufreq/";
            if (DIR* dir = opendir(cpufreq_dir_path); dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    if (strncmp(entry->d_name, "policy", 6) == 0) {
                        policies.push_back(std::string(cpufreq_dir_path) + entry->d_name);
                    }
                }
                closedir(dir);
            }
            std::sort(policies.begin(), policies.end());

            // 解析大小核
            std::string little = "0-3"; 
            std::string mid = cores;
            std::string big = cores;

            if (policies.size() >= 3) {
                little = format_cpuset(read_node((policies[0] + "/affected_cpus").c_str()));
                mid    = format_cpuset(read_node((policies[1] + "/affected_cpus").c_str()));
                big    = format_cpuset(read_node((policies[2] + "/affected_cpus").c_str()));
            } else if (policies.size() == 2) {
                little = format_cpuset(read_node((policies[0] + "/affected_cpus").c_str()));
                big    = format_cpuset(read_node((policies[1] + "/affected_cpus").c_str()));
                mid    = big;
            }

            std::string best_gov = "schedutil";
            if (!policies.empty()) {
                const std::string avail_govs = read_node((policies[0] + "/scaling_available_governors").c_str());
                best_gov = get_best_cpu_governor(avail_govs, mtk);
            }
            return CpuTopology{mtk, cores, little, mid, big, best_gov};
        }();
        
        return instance;
    }

private:
    static std::string get_best_cpu_governor(const std::string& avail_govs, bool is_mtk) {
        if (is_mtk) {
            if (avail_govs.find("sugov_ext") != std::string::npos) return "sugov_ext";
            if (avail_govs.find("schedutil") != std::string::npos) return "schedutil";
        } else {
            if (avail_govs.find("walt") != std::string::npos) return "walt";
            if (avail_govs.find("uag") != std::string::npos) return "uag";
            if (avail_govs.find("schedutil") != std::string::npos) return "schedutil";
        }
        return "schedutil"; 
    }
};
