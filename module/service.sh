#!/system/bin/sh
MODDIR=${0%/*}

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
export OMNISCHED_CONFIG_DIR="$CONFIG_DIR"
export OMNISCHED_CONFIG_PATH="$CONFIG_FILE"

DAEMON_BIN="$MODDIR/bin/omnisched"

if [ -f "$DAEMON_BIN" ]; then
    chmod 0755 "$DAEMON_BIN"
    if ! pgrep -f "$DAEMON_BIN" >/dev/null 2>&1; then
        nohup "$DAEMON_BIN" >/dev/null 2>&1 &
    fi
fi
