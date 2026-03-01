#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>

namespace fs = std::filesystem;

// 將自身轉化為後台守護行程
void init_daemon() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); 
    
    if (setsid() < 0) exit(EXIT_FAILURE); 
    
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); 
    
    umask(0);
    chdir("/"); 

    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) close(fd);
    }
}

bool write_node(std::string_view path, std::string_view value) {
    std::ofstream file(std::string(path), std::ios::trunc); 
    if (file.is_open()) {
        file << value;
        return true;
    }
    return false;
}

std::string read_node(std::string_view path) {
    std::ifstream file(std::string{path});
    std::string value;
    if (file.is_open()) {
        std::getline(file, value);
    }
    return value;
}

std::string format_cpuset(std::string cpus) {
    if (cpus.empty()) return cpus;

    std::replace(cpus.begin(), cpus.end(), ' ', ',');
    // 清理尾部可能存在的多餘逗號或換行符
    while (!cpus.empty() && (cpus.back() == ',' || cpus.back() == '\n' || cpus.back() == '\r')) {
        cpus.pop_back();
    }
    return cpus;
}

// 避免出現連續逗號 (如 "0-3,,4-7")
std::string combine_cpus(const std::string& cpus1, const std::string& cpus2) {
    if (cpus1.empty()) return cpus2;
    if (cpus2.empty()) return cpus1;
    return cpus1 + "," + cpus2;
}

// 核心調度初始化邏輯
void apply_core_optimizations() {
    std::error_code ec;
    // 取得裝置所有 CPU 核心 (例如: 0-7)
    std::string all_cores = read_node("/sys/devices/system/cpu/possible");
    if (all_cores.empty()) all_cores = "0-7"; // 容錯機制

    // 讀取 CPU 策略並排序
    std::vector<std::string> policies;
    std::string cpufreq_dir = "/sys/devices/system/cpu/cpufreq/";
    if (fs::exists(cpufreq_dir, ec)) {
        for (const auto& entry : fs::directory_iterator(cpufreq_dir, ec)) {
            std::string filename = entry.path().filename().string();
            if (filename.starts_with("policy")) { // C++20 特性，比 find == 0 更直觀且高效
                policies.push_back(entry.path().string());
            }
        }
    }

    std::sort(policies.begin(), policies.end());

    // 劃分大中小核
    std::string cluster_little = "0-3"; // 極端後備方案
    std::string cluster_mid = all_cores;
    std::string cluster_big = all_cores;

    if (policies.size() >= 3) {
        cluster_little = format_cpuset(read_node(policies[0] + "/affected_cpus"));
        cluster_mid    = format_cpuset(read_node(policies[1] + "/affected_cpus"));
        cluster_big    = format_cpuset(read_node(policies[2] + "/affected_cpus"));
    } else if (policies.size() == 2) {
        cluster_little = format_cpuset(read_node(policies[0] + "/affected_cpus"));
        cluster_big    = format_cpuset(read_node(policies[1] + "/affected_cpus"));
        cluster_mid    = cluster_big;
    }

    // 寫入 Cpuset 分配
    write_node("/dev/cpuset/top-app/cpus", all_cores);
    if (!cluster_mid.empty() && cluster_mid != cluster_big) {
        write_node("/dev/cpuset/foreground/cpus", combine_cpus(cluster_little, cluster_mid));
    } else {
        write_node("/dev/cpuset/foreground/cpus", all_cores);
    }
    write_node("/dev/cpuset/background/cpus", cluster_little);
    write_node("/dev/cpuset/system-background/cpus", cluster_little);

    // 強制所有 CPU 使用 schedutil 調度器
    std::string cpu_dir = "/sys/devices/system/cpu/";
    if (fs::exists(cpu_dir, ec)) {
        for (const auto& entry : fs::directory_iterator(cpu_dir, ec)) {
            std::string filename = entry.path().filename().string();
            if (filename.starts_with("cpu") && filename.length() > 3 && std::isdigit(filename[3])) {
                write_node(entry.path().string() + "/cpufreq/scaling_governor", "schedutil");
            }
        }
    }

    // GPU 調度優化
    std::string adreno_path = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
    if (fs::exists(adreno_path, ec)) {
        write_node(adreno_path, "msm-adreno-tz"); // 高通 Adreno
    } else if (fs::exists("/sys/class/devfreq/", ec)) {
        for (const auto& entry : fs::directory_iterator("/sys/class/devfreq/", ec)) {
            std::string gov_path = entry.path().string() + "/governor";
            std::string avail_govs = read_node(entry.path().string() + "/available_governors");

            if (avail_govs.find("mali_ondemand") != std::string::npos) {
                write_node(gov_path, "mali_ondemand");
            } else if (avail_govs.find("simple_ondemand") != std::string::npos) {
                write_node(gov_path, "simple_ondemand");
            }
        }
    }

    // 執行系統屬性設定
    std::system("settings put system config.hw_quickpoweron true");
    std::system("settings put system surface_flinger_use_frame_rate_api true");
}

int main() {
    // 啟動守護模式
    init_daemon();

    while (true) {
        apply_core_optimizations();
        
        std::this_thread::sleep_for(std::chrono::minutes(30));
    }

    return 0;
}
