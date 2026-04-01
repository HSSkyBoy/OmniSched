#!/system/bin/sh
ui_print "OmniSched v3 安装程序"
ui_print "作者：HSSkyBoy / nikobe918"

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
    ui_print ""
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
    ui_print "- 将套用动态通用型调度。"
fi

ui_print "-"
ui_print "- 正在部署核心调度文件..."
ui_print "- 正在建立动态配置环境 (WebUI)..."

ui_print "- 请选择是否「预设开启强制 Vulkan 渲染」"
ui_print "  音量 +：开启"
ui_print "  音量 -：关闭"

FORCE_VULKAN_DEFAULT=false
if chooseport; then
    FORCE_VULKAN_DEFAULT=true
    ui_print "- 已选择：预设开启强制 Vulkan。"
else
    FORCE_VULKAN_DEFAULT=false
    ui_print "- 已选择：预设关闭强制 Vulkan。"
fi

ui_print "- 正在写入核心配置..."
CONFIG_DIR="/data/adb/omnisched"
if [ ! -d "$CONFIG_DIR" ]; then
    mkdir -p "$CONFIG_DIR"
fi

if [ ! -f "$CONFIG_DIR/config.json" ]; then
    cat <<EOF > "$CONFIG_DIR/config.json"
{"poll_interval_seconds":950,"cpuset":{"background_little_core_only":true},"render":{"force_vulkan":$FORCE_VULKAN_DEFAULT}}
EOF
else
    sed -i 's/"force_vulkan"[[:space:]]*:[[:space:]]*\(true\|false\)/"force_vulkan":'"$FORCE_VULKAN_DEFAULT"'/g' "$CONFIG_DIR/config.json" 2>/dev/null
fi

sleep 0.2
ui_print "- 正在设定权限..."
set_perm_recursive "$MODPATH" 0 0 0755 0755
set_perm_recursive "$CONFIG_DIR" 0 0 0755 0754

ui_print ""
ui_print "安装完成：请重启系统以套用 OmniSched v3。"
ui_print "提示：后续可在 WebUI / config.json 随时修改 Vulkan 开关。"
ui_print ""
