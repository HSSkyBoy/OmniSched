#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <functional>
#include <string>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "config.h"
#include "cpu_topology.h"
#include "project_paths.h"
#include "utils.h"

class SchedEventFlow {
private:
    int epoll_fd = -1;
    int inotify_fd = -1;
    std::vector<int> watch_descriptors;

    static constexpr int DEFAULT_TIMEOUT_SECONDS = 900;
    static constexpr uint32_t FILE_WATCH_MASK =
        IN_MODIFY | IN_CLOSE_WRITE | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF;
    static constexpr uint32_t DIR_WATCH_MASK =
        IN_CREATE | IN_MOVED_TO | IN_CLOSE_WRITE | IN_ATTRIB | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF;
    static constexpr int SHORT_VIDEO_REFRESH_TIMEOUT_MS = 2000;

    int get_auto_timeout_seconds() const {
        const auto& topology = CpuTopology::get();
        const int cpu_count = count_cpus_in_cpuset(topology.all_cores);
        const bool dedicated_big_cluster =
            !topology.cluster_big.empty() && topology.cluster_big != topology.cluster_mid;

        if (cpu_count <= 4) return 720;
        if (cpu_count >= 8 && dedicated_big_cluster) return 480;
        return 600;
    }

    bool has_short_video_refresh_task() const {
        OmniConfig::reload();
        const auto& config = OmniConfig::get();
        return config.display.short_video_refresh_rate_enabled && !config.display.short_video_apps.empty();
    }

    int get_poll_timeout_ms() const {
        OmniConfig::reload();
        int timeout_seconds = OmniConfig::get().poll_interval_seconds;
        if (OmniConfig::get().auto_optimize) {
            timeout_seconds = get_auto_timeout_seconds();
        } else if (timeout_seconds <= 0) {
            timeout_seconds = DEFAULT_TIMEOUT_SECONDS;
        }

        timeout_seconds = std::clamp(timeout_seconds, 300, 3600);
        return timeout_seconds * 1000;
    }

    int get_timeout_ms() const {
        int timeout_ms = get_poll_timeout_ms();
        if (has_short_video_refresh_task()) {
            timeout_ms = std::min(timeout_ms, SHORT_VIDEO_REFRESH_TIMEOUT_MS);
        }
        return timeout_ms;
    }

    void clear_watches() {
        for (int wd : watch_descriptors) {
            if (wd >= 0 && inotify_fd >= 0) {
                inotify_rm_watch(inotify_fd, wd);
            }
        }
        watch_descriptors.clear();
    }

    void add_watch(const std::string& path, uint32_t mask) {
        if (inotify_fd < 0 || path.empty()) return;

        const int wd = inotify_add_watch(inotify_fd, path.c_str(), mask);
        if (wd >= 0) {
            watch_descriptors.push_back(wd);
        }
    }

    void refresh_watches() {
        clear_watches();

        const std::vector<std::string> target_nodes = {
            "/dev/cpuset/top-app/cpus",
            "/dev/cpuset/background/cpus",
            "/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
        };

        for (const auto& node : target_nodes) {
            add_watch(node, FILE_WATCH_MASK);
        }
        for (const auto& config_dir : omnisched::config_dir_candidates()) {
            add_watch(config_dir, DIR_WATCH_MASK);
        }
        for (const auto& config_path : omnisched::config_path_candidates()) {
            if (path_exists(config_path.c_str())) {
                add_watch(config_path, FILE_WATCH_MASK);
            }
        }
    }

    bool should_refresh_watches(const inotify_event& event) const {
        if ((event.mask & (IN_IGNORED | IN_DELETE_SELF | IN_MOVE_SELF)) != 0) {
            return true;
        }
        if (event.len == 0) {
            return false;
        }
        return std::string(event.name) == "config.json";
    }

    bool drain_inotify_events() {
        if (inotify_fd < 0) return false;

        bool saw_event = false;
        bool refresh_needed = false;
        char buffer[2048];

        while (true) {
            const ssize_t bytes_read = read(inotify_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    refresh_needed = true;
                }
                break;
            }

            saw_event = true;
            ssize_t offset = 0;
            while (offset < bytes_read) {
                const auto* event = reinterpret_cast<const inotify_event*>(buffer + offset);
                refresh_needed = refresh_needed || should_refresh_watches(*event);
                offset += sizeof(inotify_event) + event->len;
            }
        }

        if (refresh_needed) {
            refresh_watches();
        }

        return saw_event;
    }

public:
    SchedEventFlow() {
        epoll_fd = epoll_create1(0);
        inotify_fd = inotify_init1(IN_NONBLOCK);

        if (epoll_fd < 0 || inotify_fd < 0) {
            return;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = inotify_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &ev) < 0) {
            close(inotify_fd);
            close(epoll_fd);
            inotify_fd = -1;
            epoll_fd = -1;
            return;
        }

        refresh_watches();
    }

    ~SchedEventFlow() {
        clear_watches();
        if (inotify_fd >= 0) close(inotify_fd);
        if (epoll_fd >= 0) close(epoll_fd);
    }

    void collect(const std::function<void()>& action, const std::function<void()>& periodic_action) {
        auto last_action_at = std::chrono::steady_clock::now();

        if (epoll_fd < 0 || inotify_fd < 0) {
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(get_timeout_ms()));
                periodic_action();
                const auto now = std::chrono::steady_clock::now();
                if (now - last_action_at >= std::chrono::milliseconds(get_poll_timeout_ms())) {
                    action();
                    last_action_at = now;
                }
            }
        }

        epoll_event events[4];

        while (true) {
            const int event_count = epoll_wait(epoll_fd, events, 4, get_timeout_ms());

            if (event_count == 0) {
                periodic_action();
                const auto now = std::chrono::steady_clock::now();
                if (now - last_action_at >= std::chrono::milliseconds(get_poll_timeout_ms())) {
                    action();
                    last_action_at = now;
                }
                continue;
            }
            if (event_count < 0) {
                if (errno == EINTR) continue;
                refresh_watches();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                periodic_action();
                action();
                last_action_at = std::chrono::steady_clock::now();
                continue;
            }

            bool should_apply = false;
            for (int index = 0; index < event_count; ++index) {
                if (events[index].data.fd == inotify_fd) {
                    should_apply = drain_inotify_events() || should_apply;
                }
            }

            if (should_apply) {
                action();
                last_action_at = std::chrono::steady_clock::now();
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
    }
};
