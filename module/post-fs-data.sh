#!/system/bin/sh
MODDIR=${0%/*}

write_default_config() {
    cat <<'EOF' > "$CONFIG_FILE"
{
  "poll_interval_seconds": 950,
  "cpuset": {
    "background_little_core_only": false
  },
  "render": {
    "vulkan_mode": "off",
    "vulkan_apps": []
  },
  "power": {
    "policy": "balanced"
  },
  "performance": {
    "auto_optimize": false,
    "lite_mode": false,
    "scheduler_tune": false,
    "memory_tune": false,
    "io_tune": false,
    "gpu_tune": false,
    "input_boost": true,
    "thermal_guard": true
  },
  "scheduler": {
    "top_app_uclamp_min": -1,
    "foreground_uclamp_min": -1,
    "background_uclamp_max": -1,
    "schedutil_up_rate_limit_us": -1,
    "schedutil_down_rate_limit_us": -1,
    "schedutil_iowait_boost": -1
  },
  "cpu": {
    "governor_override": "",
    "foreground_cpuset": "",
    "system_background_cpuset": "",
    "background_cpuset": "",
    "scaling_min_freq_khz": -1,
    "scaling_max_freq_khz": -1
  },
  "input": {
    "boost_ms": -1
  },
  "thermal": {
    "throttle_temp_c": -1,
    "top_app_uclamp_max": -1,
    "foreground_uclamp_max": -1,
    "background_uclamp_max": -1
  }
}
EOF
}

MODULE_ID="${MODID:-${MODDIR##*/}}"
[ -z "$MODULE_ID" ] && MODULE_ID="zygisk_omnisched"
CANONICAL_CONFIG_DIR="/data/adb/zygisk_omnisched"
LEGACY_CONFIG_DIR="/data/adb/omnisched"
LEGACY_CONFIG_FILE="$LEGACY_CONFIG_DIR/config.json"

if [ -n "$OMNISCHED_CONFIG_DIR" ]; then
    CONFIG_DIR="$OMNISCHED_CONFIG_DIR"
else
    CONFIG_DIR="$CANONICAL_CONFIG_DIR"
fi

CONFIG_FILE="${OMNISCHED_CONFIG_PATH:-$CONFIG_DIR/config.json}"
mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_FILE" ] && [ -f "$LEGACY_CONFIG_FILE" ]; then
    cp "$LEGACY_CONFIG_FILE" "$CONFIG_FILE" 2>/dev/null
fi
[ -f "$CONFIG_FILE" ] || write_default_config

MEMORY_TUNE="false"
if [ -f "$CONFIG_FILE" ]; then
    MEMORY_TUNE_VALUE=$(grep -o '"memory_tune"[[:space:]]*:[[:space:]]*\(true\|false\)' "$CONFIG_FILE" 2>/dev/null \
        | tail -n1 \
        | sed 's/.*:[[:space:]]*//')
    [ "$MEMORY_TUNE_VALUE" = "true" ] && MEMORY_TUNE="true"
    [ "$MEMORY_TUNE_VALUE" = "false" ] && MEMORY_TUNE="false"
fi

[ "$MEMORY_TUNE" = "true" ] && echo 0 > /proc/sys/vm/page-cluster 2>/dev/null
A_API=$(getprop ro.build.version.sdk)
[ -z "$A_API" ] && exit 0
[ "$A_API" -lt 31 ] && exit 0

if [ "$MEMORY_TUNE" = "true" ] && [ "$A_API" -ge 34 ]; then
    resetprop -n ro.lmk.use_minfree_levels true
    resetprop -n ro.lmk.enhance_batch_kill false
    resetprop -n ro.lmk.swap_util_max 90
fi

VULKAN_MODE="off"
if [ -f "$CONFIG_FILE" ]; then
    VULKAN_MODE=$(grep -o '"vulkan_mode"[[:space:]]*:[[:space:]]*"[^"]*"' "$CONFIG_FILE" 2>/dev/null \
        | tail -n1 \
        | sed 's/.*:[[:space:]]*"\([^"]*\)"/\1/')

    if [ -z "$VULKAN_MODE" ]; then
        FORCE_VULKAN_VALUE=$(grep -o '"force_vulkan"[[:space:]]*:[[:space:]]*\(true\|false\)' "$CONFIG_FILE" 2>/dev/null \
            | tail -n1 \
            | sed 's/.*:[[:space:]]*//')
        [ "$FORCE_VULKAN_VALUE" = "true" ] && VULKAN_MODE="global"
    fi
fi

if [ "$VULKAN_MODE" != "global" ]; then
    resetprop -n ro.hwui.renderer skiagl
    resetprop -n debug.hwui.renderer skiagl
    resetprop -n debug.renderengine.backend skiagl
    resetprop -n ro.hwui.use_vulkan false
    resetprop -n debug.renderengine.graphite true
    resetprop -n debug.renderengine.vulkan.precompile.enabled false
else
    resetprop -n ro.hwui.renderer skiavk
    resetprop -n debug.hwui.renderer skiavk
    resetprop -n debug.renderengine.backend skiavk
    resetprop -n ro.hwui.use_vulkan true
    resetprop -n debug.renderengine.graphite false
    resetprop -n debug.renderengine.vulkan.precompile.enabled false
    resetprop -n debug.hwui.use_buffer_age true
    resetprop -n debug.hwui.disable_scissor_opt false
fi

SOC=$(getprop ro.soc.manufacturer)
if [ -d "/sys/class/kgsl" ] || echo "$SOC" | grep -qi "qualcomm"; then
    resetprop -n ro.vendor.qti.config.zram true
    resetprop -n ro.vendor.qti.sys.fw.bservice_enable true
    resetprop -n ro.vendor.qti.core.ctl_max_cpu 6
    resetprop -n ro.vendor.qti.core.ctl_min_cpu 0
elif echo "$SOC" | grep -qi "mediatek\|mtk"; then
    resetprop -n ro.mtk_perf_fast_start_win 1
    resetprop -n ro.mtk_perf_response_time 1
    resetprop -n ro.vendor.mtk_zram_extend 1
    resetprop -n ro.vendor.mtk.sensor.support true
    resetprop -n ro.vendor.num_mdm_crashes 0
fi
