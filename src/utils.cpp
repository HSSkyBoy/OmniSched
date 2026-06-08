#include "utils.h"
#include <cstdio>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <sstream>
#include <vector>

bool write_node(const char* path, const char* value) {
    const std::string current_value = read_node(path); // 模擬 val
    if (current_value == value) return true; // 減少無效 I/O

    if (FILE* file = fopen(path, "w"); file) { // 模擬 let scope
        fputs(value, file);
        fclose(file);
        return true;
    }
    return false;
}

std::string read_node(const char* path) {
    char buffer[256];
    std::string value;
    if (FILE* file = fopen(path, "r"); file) {
        if (fgets(buffer, sizeof(buffer), file)) {
            value = buffer;
            while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                value.pop_back();
            }
        }
        fclose(file);
    }
    return value;
}

std::optional<std::string> read_node_opt(const char* path) {
    const std::string val = read_node(path);
    return val.empty() ? std::nullopt : std::make_optional(val); // 模擬 Elvis 操作符
}

std::string format_cpuset(const std::string& cpus) {
    if (cpus.empty()) return cpus;
    std::string result = cpus;
    std::replace(result.begin(), result.end(), ' ', ',');
    while (!result.empty() && (result.back() == ',' || result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::vector<int> expand_cpuset(const std::string& cpuset) {
    std::vector<int> cpus;
    std::stringstream stream(format_cpuset(cpuset));
    std::string token;

    while (std::getline(stream, token, ',')) {
        token.erase(std::remove_if(token.begin(), token.end(),
                                   [](unsigned char ch) { return std::isspace(ch); }),
                    token.end());
        if (token.empty()) continue;

        const std::size_t dash_pos = token.find('-');
        if (dash_pos != std::string::npos) {
            const int start = std::atoi(token.substr(0, dash_pos).c_str());
            const int end = std::atoi(token.substr(dash_pos + 1).c_str());
            if (start < 0 || end < start) continue;

            for (int cpu = start; cpu <= end; ++cpu) {
                cpus.push_back(cpu);
            }
            continue;
        }

        const int core = std::atoi(token.c_str());
        if (core >= 0) {
            cpus.push_back(core);
        }
    }

    std::sort(cpus.begin(), cpus.end());
    cpus.erase(std::unique(cpus.begin(), cpus.end()), cpus.end());
    return cpus;
}

std::string normalize_cpuset(const std::string& cpuset) {
    const std::vector<int> cpus = expand_cpuset(cpuset);
    if (cpus.empty()) return {};

    std::ostringstream normalized;
    for (std::size_t index = 0; index < cpus.size();) {
        const int start = cpus[index];
        int end = start;

        while (index + 1 < cpus.size() && cpus[index + 1] == end + 1) {
            ++index;
            end = cpus[index];
        }

        if (normalized.tellp() > 0) {
            normalized << ',';
        }
        normalized << start;
        if (end != start) {
            normalized << '-' << end;
        }

        ++index;
    }

    return normalized.str();
}

std::string combine_cpus(const std::string& cpus1, const std::string& cpus2) {
    if (cpus1.empty()) return cpus2;
    if (cpus2.empty()) return cpus1;
    return normalize_cpuset(cpus1 + "," + cpus2);
}

int count_cpus_in_cpuset(const std::string& cpuset) {
    return static_cast<int>(expand_cpuset(cpuset).size());
}

bool path_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

std::string execute_command(const char* cmd) {
    char buffer[128];
    std::string result = "";
    if (FILE* pipe = popen(cmd, "r"); pipe) {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
    }
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}
