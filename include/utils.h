#pragma once

#include <string>
#include <optional>

bool write_node(const char* path, const char* value);
std::string read_node(const char* path);
std::optional<std::string> read_node_opt(const char* path);
std::string format_cpuset(const std::string& cpus);
std::string combine_cpus(const std::string& cpus1, const std::string& cpus2);
std::string execute_command(const char* cmd);
bool path_exists(const char* path);

std::string get_package_name(int pid);
std::string detect_foreground_app();
void set_cpu_governor(const std::string& gov);
