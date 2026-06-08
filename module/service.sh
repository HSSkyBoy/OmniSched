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

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

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
export OMNISCHED_CONFIG_DIR="$CONFIG_DIR"
export OMNISCHED_CONFIG_PATH="$CONFIG_FILE"

DAEMON_BIN="$MODDIR/bin/omnisched"
DAEMON_TAG="OMNISCHED_CONFIG_PATH=$CONFIG_FILE"

if [ -f "$DAEMON_BIN" ]; then
    chmod 0755 "$DAEMON_BIN"
    if ! pgrep -f "$DAEMON_BIN.*$CONFIG_FILE" >/dev/null 2>&1; then
        pkill -f "$DAEMON_BIN" >/dev/null 2>&1
        nohup env "$DAEMON_TAG" "$DAEMON_BIN" >/dev/null 2>&1 &
    fi
fi
