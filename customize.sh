#!/system/bin/sh
ui_print "========================================="
ui_print "      OmniSched | 全域灵动分配 (V3)"
ui_print "       Author：HSSkyBoy/nikobe918"
ui_print "========================================="

sleep 0.4
ui_print "- 正在初始化环境与检测设备相容性..."

API=$(getprop ro.build.version.sdk)
DEVICE_MODEL=$(getprop ro.product.model)
SOC_MAKER=$(getprop ro.soc.manufacturer)

sleep 0.6
ui_print "- 设备型号: $DEVICE_MODEL"
ui_print "- 处理器平台: $SOC_MAKER"
ui_print "- 当前系统 API: $API"
ui_print "-"

if [ "$API" -lt 31 ]; then
    ui_print " "
    ui_print "! OmniSched 依賴 Android 12+ 的底層渲染 API。"
    ui_print "! 您的 API 級別為 $API，無法安裝本模組。"
    abort "! 安裝已中止。"
fi

# 核心硬體平台偵測與提示
if [ -d "/sys/class/kgsl" ] || echo "$SOC_MAKER" | grep -qi "Qualcomm"; then
    ui_print "- 侦测到高通 Snapdragon 平台！"
    ui_print "- 已启用 QTI 专属底层优化与 ZRAM 配置。"
elif echo "$SOC_MAKER" | grep -qi "MediaTek" || echo "$SOC_MAKER" | grep -qi "MTK"; then
    ui_print "- 侦测到联发科天玑平台！"
    ui_print "- 已启用 MTK 专属底层优化与调度配置。"
else
    ui_print "- 侦测到通用平台。"
    ui_print "- 将套用动态通用型调度并强制 Vulkan 渲染。"
fi

ui_print "-"
ui_print "****************************************"
ui_print "* 請選擇是否預設開啟強制 Vulkan 渲染   *"
ui_print "* [音量上 +] : 預設開啟                *"
ui_print "* [音量下 -] : 預設關閉                *"
ui_print "******** (按任意音量鍵開始選擇) ********"
sleep 1
while true; do
    key_event=$(getevent -qlc 1 2>/dev/null)
    if echo "$key_event" | grep -q -iE 'KEY_VOLUMEUP'; then
        VK_FORCE=true
        ui_print "-> 已選擇：預設開啟強制 Vulkan"
        break
    elif echo "$key_event" | grep -q -iE 'KEY_VOLUMEDOWN'; then
        VK_FORCE=false
        ui_print "-> 已選擇：預設關閉強制 Vulkan"
        break
    fi
done

ui_print "-"
ui_print "- 正在部署核心调配文件..."
ui_print "-"
ui_print "- 正在建立动态配置环境 (WebUI 支援)..." 
ui_print "- 正在编写核心配置..."
CONFIG_DIR="/data/adb/omnisched"
if [ ! -d "$CONFIG_DIR" ]; then
    mkdir -p "$CONFIG_DIR"
    cat <<EOF > "$CONFIG_DIR/config.json"
{"poll_interval_seconds":950,"cpuset":{"background_little_core_only":true},"render":{"force_vulkan":${VK_FORCE}}}
EOF
else
    sed -i "s/\"force_vulkan\"\s*:\s*[a-z]*/\"force_vulkan\":${VK_FORCE}/g" "$CONFIG_DIR/config.json"
    if ! grep -q '"force_vulkan"' "$CONFIG_DIR/config.json"; then
        sed -i 's/"render"\s*:\s*{/"render":{"force_vulkan":'"${VK_FORCE}"',/g' "$CONFIG_DIR/config.json"
    fi
fi

sleep 0.2
ui_print "- 正在设定权限..."
set_perm_recursive "$MODPATH" 0 0 0755 0755
set_perm_recursive "$CONFIG_DIR" 0 0 0755 0754

ui_print " "
ui_print "========================================="
ui_print "  安装完成！请重启系统以套用 OmniSched V3"
ui_print "========================================="
ui_print " "