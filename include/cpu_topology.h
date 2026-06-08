#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <optional>
#include <string>
#include <vector>

#include "utils.h"

struct CpuTopology {
    const bool is_mtk;
    const std::string all_cores;
    const std::string cluster_little;
    const std::string cluster_mid;
    const std::string cluster_big;
    const std::string best_cpu_governor;
    const int cpu_count;
    const int max_freq_khz;
    const bool has_cpufreq;

    static const CpuTopology& get() {
        static const CpuTopology instance = []() {
            const std::string platform = execute_command("getprop ro.board.platform");
            const bool mtk = platform.find("mt") != std::string::npos;
            const std::string cores = normalize_cpuset(
                read_node_opt("/sys/devices/system/cpu/possible").value_or("0-7")
            );

            std::vector<std::string> policies;
            const char* cpufreq_dir_path = "/sys/devices/system/cpu/cpufreq/";
            if (DIR* dir = opendir(cpufreq_dir_path); dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    if (std::strncmp(entry->d_name, "policy", 6) == 0) {
                        policies.push_back(std::string(cpufreq_dir_path) + entry->d_name);
                    }
                }
                closedir(dir);
            }

            std::sort(policies.begin(), policies.end(), [](const std::string& left, const std::string& right) {
                const std::size_t left_pos = left.find_last_not_of("0123456789");
                const std::size_t right_pos = right.find_last_not_of("0123456789");
                const int left_id = std::atoi(left.substr(left_pos + 1).c_str());
                const int right_id = std::atoi(right.substr(right_pos + 1).c_str());
                return left_id < right_id;
            });

            std::string little = cores.empty() ? "0-7" : cores;
            std::string mid = cores;
            std::string big = cores;
            int max_freq_khz = 0;

            if (policies.size() >= 3) {
                little = normalize_cpuset(read_node((policies[0] + "/affected_cpus").c_str()));
                mid = normalize_cpuset(read_node((policies[1] + "/affected_cpus").c_str()));
                big = normalize_cpuset(read_node((policies[2] + "/affected_cpus").c_str()));
            } else if (policies.size() == 2) {
                little = normalize_cpuset(read_node((policies[0] + "/affected_cpus").c_str()));
                big = normalize_cpuset(read_node((policies[1] + "/affected_cpus").c_str()));
                mid = big;
            }

            for (const auto& policy : policies) {
                const int policy_max = std::atoi(read_node((policy + "/cpuinfo_max_freq").c_str()).c_str());
                if (policy_max > max_freq_khz) {
                    max_freq_khz = policy_max;
                }
            }

            std::string best_gov = "schedutil";
            if (!policies.empty()) {
                const std::string avail_govs = read_node((policies[0] + "/scaling_available_governors").c_str());
                best_gov = get_best_cpu_governor(avail_govs, mtk);
            }

            return CpuTopology{
                mtk,
                cores,
                little,
                mid,
                big,
                best_gov,
                count_cpus_in_cpuset(cores),
                max_freq_khz,
                !policies.empty()
            };
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
