#pragma once

#include <string>
#include "config.h"

void init_daemon();
void apply_base_optimizations();
void apply_dynamic_profile(ProfileMode mode, bool is_lite);