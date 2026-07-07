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
  },
  "display": {
    "short_video_refresh_rate_enabled": false,
    "short_video_refresh_rate_hz": 30,
    "short_video_apps": []
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

reset_global_vulkan_props() {
    resetprop -n ro.hwui.renderer skiagl
    resetprop -n debug.hwui.renderer skiagl
    resetprop -n debug.renderengine.backend skiagl
    resetprop -n ro.hwui.use_vulkan false
    resetprop -n debug.renderengine.graphite true
    resetprop -n debug.renderengine.vulkan false
    resetprop -n debug.renderengine.vulkan.precompile.enabled false
    resetprop -n debug.vulkan.layers ""
    resetprop -n debug.hwui.vulkan_feature_level ""
    resetprop -n debug.hwui.vulkan.auto_detect_features false
    resetprop -n debug.hwui.vulkan.platform_optimized false
    resetprop -n debug.hwui.vulkan.enable_dynamic_rendering false
    resetprop -n debug.hwui.vulkan.synchronization2 false
    resetprop -n debug.hwui.vulkan.enable_descriptor_indexing false
    resetprop -n debug.hwui.vulkan.host_image_copy false
    resetprop -n debug.hwui.vulkan.dynamic_rendering_local_read false
    resetprop -n debug.hwui.vulkan.descriptor_heap false
    resetprop -n debug.hwui.vulkan.fragment_shading_rate false
    resetprop -n debug.hwui.vulkan.maintenance6 false
    resetprop -n debug.hwui.vulkan.pipeline_robustness false
    resetprop -n debug.hwui.vulkan.pipeline_cache_persistent false
    resetprop -n debug.hwui.enable_gpu_pipeline_cache false
    resetprop -n debug.hwui.precompile_shaders false
    resetprop -n debug.hwui.shader_cache_preload false
    resetprop -n debug.hwui.shader_cache_warmup false
    resetprop -n debug.hwui.enable_compute_shaders false
    resetprop -n debug.vulkan.memory.preallocate false
    resetprop -n debug.vulkan.memory.sub_allocation false
    resetprop -n debug.hwui.fallback_renderer skiagl
    resetprop -n debug.hwui.initialize_gl_always false
    resetprop -n debug.hwui.early_preload_gl_context false
    resetprop -n debug.hwui.vulkan_safe_mode false
    resetprop -n debug.hwui.skia_tracing_enabled false
    resetprop -n debug.hwui.skia_use_perfetto_track_events false
    resetprop -n debug.renderengine.skia_atrace_enabled false
    resetprop -n debug.vulkan.force_validation false
    resetprop -n debug.vulkan.validate.memory false
    resetprop -n debug.vulkan.validate false
    resetprop -n debug.hwui.use_hint_manager true
    resetprop -n debug.sf.enable_async_barrier_control false
}

apply_global_vulkan_props() {
    resetprop -n ro.hwui.renderer skiavk
    resetprop -n debug.hwui.renderer skiavk
    resetprop -n debug.renderengine.backend skiavk
    resetprop -n ro.hwui.use_vulkan true
    resetprop -n debug.renderengine.graphite false
    resetprop -n debug.renderengine.vulkan true
    resetprop -n debug.renderengine.vulkan.precompile.enabled true
    resetprop -n debug.vulkan.layers ""
    resetprop -n debug.hwui.vulkan_feature_level 1.3
    resetprop -n debug.hwui.vulkan.auto_detect_features true
    resetprop -n debug.hwui.vulkan.platform_optimized true
    resetprop -n debug.hwui.vulkan.enable_dynamic_rendering true
    resetprop -n debug.hwui.vulkan.synchronization2 true
    resetprop -n debug.hwui.vulkan.enable_descriptor_indexing true
    resetprop -n debug.hwui.vulkan.host_image_copy true
    resetprop -n debug.hwui.vulkan.dynamic_rendering_local_read true
    resetprop -n debug.hwui.vulkan.descriptor_heap true
    resetprop -n debug.hwui.vulkan.fragment_shading_rate true
    resetprop -n debug.hwui.vulkan.maintenance6 true
    resetprop -n debug.hwui.vulkan.pipeline_robustness true
    resetprop -n debug.hwui.vulkan.pipeline_cache_persistent true
    resetprop -n debug.hwui.enable_gpu_pipeline_cache true
    resetprop -n debug.hwui.precompile_shaders true
    resetprop -n debug.hwui.shader_cache_preload true
    resetprop -n debug.hwui.shader_cache_warmup true
    resetprop -n debug.hwui.enable_compute_shaders true
    resetprop -n debug.vulkan.memory.preallocate true
    resetprop -n debug.vulkan.memory.sub_allocation true
    resetprop -n debug.hwui.fallback_renderer skiagl
    resetprop -n debug.hwui.initialize_gl_always false
    resetprop -n debug.hwui.early_preload_gl_context false
    resetprop -n debug.hwui.vulkan_safe_mode false
    resetprop -n debug.hwui.skia_tracing_enabled false
    resetprop -n debug.hwui.skia_use_perfetto_track_events false
    resetprop -n debug.renderengine.skia_atrace_enabled false
    resetprop -n debug.vulkan.force_validation false
    resetprop -n debug.vulkan.validate.memory false
    resetprop -n debug.vulkan.validate false
    resetprop -n debug.hwui.use_hint_manager true
    resetprop -n debug.sf.enable_async_barrier_control true
}

if [ "$VULKAN_MODE" != "global" ]; then
    reset_global_vulkan_props
else
    apply_global_vulkan_props
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
