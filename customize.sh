#!/system/bin/sh

MODULE_ID="${MODID:-${MODPATH##*/}}"
[ -n "$MODULE_ID" ] || MODULE_ID="zygisk_omnisched"
CONFIG_DIR="${OMNISCHED_CONFIG_DIR:-/data/adb/$MODULE_ID}"
CONFIG_FILE="${OMNISCHED_CONFIG_PATH:-$CONFIG_DIR/config.json}"
VULKAN_MODE="off"

write_config() {
    cat <<EOF > "$CONFIG_FILE"
{
  "poll_interval_seconds": 950,
  "cpuset": { "background_little_core_only": true },
  "render": { "vulkan_mode": "$VULKAN_MODE", "vulkan_apps": [] },
  "power": { "policy": "balanced" },
  "performance": { "auto_optimize": false, "lite_mode": false }
}
EOF
}

ui_print "OmniSched v4 安裝程序"
ui_print "作者：HSSkyBoy / nikobe918"
ui_print ""

API=$(getprop ro.build.version.sdk)
ui_print "- 正在初始化環境並檢查裝置相容性..."
sleep 0.4
ui_print "- 裝置：$(getprop ro.product.model)"
ui_print "- 處理器：$(getprop ro.soc.manufacturer)"
ui_print "- SDK 版本：$API"
ui_print "-"

if [ -z "$API" ] || [ "$API" -lt 31 ]; then
    ui_print ""
    ui_print "! Zygisk OmniSched 需要 Android 12 (API 31) 或以上版本"
    abort "! 安裝已中止"
fi

ui_print "請選擇是否預設開啟「強制 Vulkan 渲染」"
ui_print "注意：部分裝置開啟後可能卡開機，首刷建議選擇「關閉」"
ui_print ""
ui_print " [音量 +]：預設開啟"
ui_print " [音量 -]：預設關閉"

if command -v getevent >/dev/null 2>&1; then
    ui_print "- 等待音量鍵輸入（10 秒後預設關閉）..."
    i=0
    while [ "$i" -lt 10 ]; do
        key_event=$(getevent -qlc 1 2>/dev/null)
        if echo "$key_event" | grep -qi "KEY_VOLUMEUP"; then
            VULKAN_MODE="global"
            ui_print "-> 已選擇：預設開啟強制 Vulkan"
            break
        fi
        if echo "$key_event" | grep -qi "KEY_VOLUMEDOWN"; then
            ui_print "-> 已選擇：預設關閉強制 Vulkan"
            break
        fi
        i=$((i + 1))
        sleep 1
    done
    [ "$i" -ge 10 ] && ui_print "-> 未偵測到按鍵輸入，將使用預設值：關閉"
else
    ui_print "- 當前環境不支援 getevent，將使用預設值：關閉"
fi
ui_print "-"

ui_print "- 正在寫入模組設定檔..."
mkdir -p "$CONFIG_DIR"

if [ -f "$CONFIG_FILE" ] && grep -q '"vulkan_mode"' "$CONFIG_FILE"; then
    sed -i "s/\"vulkan_mode\"[[:space:]]*:[[:space:]]*\"[^\"]*\"/\"vulkan_mode\": \"$VULKAN_MODE\"/" "$CONFIG_FILE"
elif [ -f "$CONFIG_FILE" ] && grep -q '"force_vulkan"' "$CONFIG_FILE"; then
    [ "$VULKAN_MODE" = "global" ] && FORCE_VULKAN=true || FORCE_VULKAN=false
    sed -i "s/\"force_vulkan\"[[:space:]]*:[[:space:]]*\\(true\\|false\\)/\"force_vulkan\": $FORCE_VULKAN/" "$CONFIG_FILE"
else
    write_config
fi

ui_print "- 正在設定檔案權限..."
set_perm_recursive "$MODPATH" 0 0 0755 0755
set_perm_recursive "$CONFIG_DIR" 0 0 0755 0754

ui_print ""
ui_print "安裝完成！"
ui_print "Vulkan 預設模式：$VULKAN_MODE"
ui_print "設定檔路徑：$CONFIG_FILE"
ui_print ""
