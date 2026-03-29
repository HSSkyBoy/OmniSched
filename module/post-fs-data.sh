#!/system/bin/sh
MODDIR=${0%/*}

# ZRAM 與記憶體底層優化
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null
A_API=$(getprop ro.build.version.sdk)
if [ -n "$A_API" ] && [ "$A_API" -lt 31 ]; then
    exit 0
fi

# Android 14+ 專屬記憶體調優
if [ "$A_API" -ge 34 ]; then
    resetprop -n ro.lmk.use_minfree_levels true
    resetprop -n ro.lmk.enhance_batch_kill true
    resetprop -n ro.lmk.swap_util_max 90
fi

# 從配置檔同步渲染引擎設定
CONFIG_FILE="/data/adb/omnisched/config.json"
FORCE_VULKAN=false

if [ -f "$CONFIG_FILE" ]; then
    if grep -q '"force_vulkan"\s*:\s*true' "$CONFIG_FILE" || grep -q '"force_vulkan":true' "$CONFIG_FILE"; then
        FORCE_VULKAN=true
    fi
else
    # 預設邏輯
    SOC_MAKER=$(getprop ro.soc.manufacturer)
    if echo "$SOC_MAKER" | grep -qi "MediaTek" || { [ "$A_API" -ge 31 ] && [ "$A_API" -lt 34 ]; }; then
        FORCE_VULKAN=true
    fi
fi

if [ "$FORCE_VULKAN" = "true" ]; then
    resetprop -n ro.hwui.renderer skiavk
    resetprop -n debug.hwui.renderer skiavk
    resetprop -n debug.renderengine.backend skiavk
    resetprop -n ro.hwui.use_vulkan true
    resetprop -n debug.renderengine.graphite false
else
    resetprop -n ro.hwui.renderer skia
    resetprop -n debug.hwui.renderer skiagl
    resetprop -n debug.renderengine.backend skiagl
    resetprop -n ro.hwui.use_vulkan false
    if [ "$A_API" -ge 34 ]; then
        resetprop -n debug.renderengine.graphite true
    fi
fi

# 設備底層屬性調優
if [ -d "/sys/class/kgsl" ] || echo "$SOC_MAKER" | grep -qi "Qualcomm"; then
    # 高通 (Snapdragon) 專屬底層調優
    resetprop ro.vendor.qti.config.zram true
    resetprop ro.vendor.qti.sys.fw.bservice_enable true
    # 限制高通核心控制的最大/最小干預
    resetprop ro.vendor.qti.core.ctl_max_cpu 4
    resetprop ro.vendor.qti.core.ctl_min_cpu 0
    
elif echo "$SOC_MAKER" | grep -qi "MediaTek"; then
    # 聯發科 (MediaTek) 專屬底層調優
    resetprop ro.mtk_perf_fast_start_win 1
    resetprop ro.mtk_perf_response_time 1
    resetprop ro.vendor.mtk_zram_extend 1
    # 針對天璣高階晶片的額外穩定屬性
    resetprop -n ro.vendor.mtk.sensor.support true
    resetprop -n ro.vendor.num_mdm_crashes 0
fi
