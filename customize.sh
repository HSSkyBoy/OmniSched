#!/system/bin/sh

ui_print " "
sleep 0.4
ui_print "========================================="
ui_print "   OmniSched | 全域靈動調度 (A12+)"
ui_print "     Author：HSSkyBoy/nikobe918"
ui_print "========================================="
ui_print " "
ui_print "- 正在初始化環境與檢測設備相容性..."

API=$(getprop ro.build.version.sdk)
DEVICE_MODEL=$(getprop ro.product.model)
SOC_MAKER=$(getprop ro.soc.manufacturer)

sleep 0.6
ui_print "- 設備型號: $DEVICE_MODEL"
ui_print "- 處理器平台: $SOC_MAKER"
ui_print "- 當前系統 API: $API"

if [ "$API" -lt 31 ]; then
    ui_print " "
    ui_print "! OmniSched 依賴 Android 12+ 的底層渲染 API。"
    ui_print "! 您的 API 級別為 $API，無法安裝本模組。"
    abort "! 安裝已中止。"
fi

if [ -d "/sys/class/kgsl" ] || echo "$SOC_MAKER" | grep -qi "Qualcomm"; then
    ui_print "- 偵測到高通 Snapdragon 平台！"
    ui_print "- 已啟用 QTI 專屬底層優化與 ZRAM 配置。"
else
    ui_print "- 偵測到通用平台。"
    ui_print "- 將套用動態通用型調度與 Vulkan 渲染。"
fi

ui_print " "
ui_print "- 正在部署核心調度文件 (post-fs-data & service)..."
sleep 0.2
ui_print "- 正在設定權限..."

set_perm_recursive "$MODPATH" 0 0 0755 0755

ui_print " "
ui_print "========================================="
ui_print "  安裝完成！請重新啟動手機以套用 OmniSched "
ui_print "========================================="
ui_print " "
