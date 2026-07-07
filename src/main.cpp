#include "core_sched.h"
#include "event_flow.h"

int main() {
    init_daemon();

    apply_core_optimizations();
    apply_foreground_refresh_rate_limits();

    SchedEventFlow flow;
    
    flow.collect(
        []() {
            apply_core_optimizations();
            apply_foreground_refresh_rate_limits();
        },
        []() {
            apply_foreground_refresh_rate_limits();
        }
    );

    return 0;
}
