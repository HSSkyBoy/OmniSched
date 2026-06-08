#!/system/bin/sh

MODULE_ID="${MODID:-${MODPATH##*/}}"
[ -n "$MODULE_ID" ] || MODULE_ID="zygisk_omnisched"

CANONICAL_CONFIG_DIR="/data/adb/zygisk_omnisched"
CANONICAL_CONFIG_FILE="$CANONICAL_CONFIG_DIR/config.json"
LEGACY_CONFIG_FILE="/data/adb/omnisched/config.json"

CONFIG_DIR="${OMNISCHED_CONFIG_DIR:-$CANONICAL_CONFIG_DIR}"
CONFIG_FILE="${OMNISCHED_CONFIG_PATH:-$CONFIG_DIR/config.json}"

write_default_config() {
    mkdir -p "$CONFIG_DIR"
    cat > "$CONFIG_FILE" <<EOF
{
  "poll_interval_seconds": 950,
  "cpuset": {
    "background_little_core_only": true
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
    "lite_mode": false
  }
}
EOF
}

ensure_config() {
    mkdir -p "$CONFIG_DIR"

    if [ ! -f "$CONFIG_FILE" ] && [ -f "$LEGACY_CONFIG_FILE" ]; then
        cp "$LEGACY_CONFIG_FILE" "$CONFIG_FILE" 2>/dev/null
    fi

    if [ ! -f "$CONFIG_FILE" ]; then
        write_default_config
    fi
}

ui_print "OmniSched installer"
ui_print "Author: HSSkyBoy / nikobe918"
ui_print ""

API="$(getprop ro.build.version.sdk)"
MODEL="$(getprop ro.product.model)"
SOC="$(getprop ro.soc.manufacturer)"

ui_print "- Device: ${MODEL:-unknown}"
ui_print "- SoC: ${SOC:-unknown}"
ui_print "- Android API: ${API:-unknown}"

if [ -z "$API" ] || [ "$API" -lt 31 ]; then
    abort "! OmniSched requires Android 12 / API 31 or newer."
fi

ui_print "- Preparing config..."
ensure_config

ui_print "- Setting permissions..."
set_perm_recursive "$MODPATH" 0 0 0755 0755
set_perm_recursive "$CONFIG_DIR" 0 0 0755 0754

if [ "$CONFIG_FILE" != "$CANONICAL_CONFIG_FILE" ] && [ ! -f "$CANONICAL_CONFIG_FILE" ]; then
    mkdir -p "$CANONICAL_CONFIG_DIR"
    cp "$CONFIG_FILE" "$CANONICAL_CONFIG_FILE" 2>/dev/null
fi

ui_print ""
ui_print "Install complete."
ui_print "Config: $CONFIG_FILE"
ui_print "Use WebUI to change power, Vulkan, Lite, and per-app settings."
ui_print ""
