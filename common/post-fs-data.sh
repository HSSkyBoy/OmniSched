#!/system/bin/sh
MODDIR=${0%/*}

# ZRAM 與記憶體底層優化
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null
echo 4 > /sys/block/zram0/max_comp_streams 2>/dev/null

A_API=$(getprop ro.build.version.sdk)
if [ -n "$A_API" ] && [ "$A_API" -lt 31 ]; then
    exit 0
fi

# 高通 (Snapdragon) 專屬底層屬性
if [ -d "/sys/class/kgsl" ] || echo "$(getprop ro.soc.manufacturer)" | grep -qi "Qualcomm"; then
    resetprop ro.vendor.qti.config.zram true
    resetprop ro.vendor.qti.sys.fw.bservice_enable true

    resetprop ro.vendor.qti.core.ctl_max_cpu 4
    resetprop ro.vendor.qti.core.ctl_min_cpu 2
fi

# SurfaceFlinger
resetprop debug.composition.type auto
resetprop persist.sys.composition.type auto
resetprop hwui.disable_vsync false
resetprop debug.sf.enable_gl_backpressure 0

# Vulkan 核心屬性強制
resetprop ro.hwui.use_vulkan true
resetprop debug.hwui.use_vulkan true
resetprop debug.hwui.renderer skiavk
resetprop debug.renderengine.backend skiavk
resetprop debug.rs.default-GPU-driver vulkan
resetprop hwui.render_dirty_regions true

# 通用硬體加速
resetprop ro.config.enable.hw_accel true
resetprop persist.sys.ui.hw 1
resetprop debug.enabletr true
resetprop debug.overlayui.enable 1
