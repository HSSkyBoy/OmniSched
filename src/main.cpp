#include "core_sched.h"
#include "event_flow.h"

int main() {
    init_daemon();

    apply_core_optimizations();

    SchedEventFlow flow;
    
    flow.collect([]() {
        apply_core_optimizations();
    });

    return 0;
}
