#include "core_sched.h"
#include "event_flow.h"
#include "config.h"
#include "utils.h"

void update_system_state() {
    static int focus_loss_strikes = 0;
    static ProfileMode current_state = ProfileMode::BALANCE;
    
    OmniConfig::reload();
    const auto& config = OmniConfig::get();

    if (config.current_profile == ProfileMode::POWERSAVE) {
        if (current_state != ProfileMode::POWERSAVE) {
            apply_dynamic_profile(ProfileMode::POWERSAVE, false);
            current_state = ProfileMode::POWERSAVE;
        }
        return;
    }
    if (config.current_profile == ProfileMode::PERFORMANCE) {
        if (current_state != ProfileMode::PERFORMANCE) {
            apply_dynamic_profile(ProfileMode::PERFORMANCE, config.lite_mode_enabled);
            current_state = ProfileMode::PERFORMANCE;
        }
        return;
    }

    std::string fg_app = detect_foreground_app();
    bool is_game = config.gamelist.count(fg_app) > 0;

    if (config.game_mode_enabled && is_game) {
        focus_loss_strikes = 0;
        if (current_state != ProfileMode::PERFORMANCE) {
            apply_dynamic_profile(ProfileMode::PERFORMANCE, config.lite_mode_enabled);
            current_state = ProfileMode::PERFORMANCE;
        }
    } else {
        if (current_state == ProfileMode::PERFORMANCE) {
            focus_loss_strikes++;
            if (focus_loss_strikes < 2) return; 
        }
        
        if (current_state != ProfileMode::BALANCE) {
            apply_dynamic_profile(ProfileMode::BALANCE, false);
            current_state = ProfileMode::BALANCE;
            focus_loss_strikes = 0;
        }
    }
}

int main() {
    init_daemon();
    OmniConfig::reload();
    apply_base_optimizations();
    update_system_state();

    SchedEventFlow flow;
    flow.collect([]() {
        update_system_state();
    });

    return 0;
}
