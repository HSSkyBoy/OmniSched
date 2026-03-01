#!/system/bin/sh
MODDIR=${0%/*}

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

# CPU & GPU 调度优化
for GOV in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
  if [ -e "$GOV" ]; then
    echo "schedutil" > "$GOV" 2>/dev/null
  fi
done

GPU_ADRENO="/sys/class/kgsl/kgsl-3d0/devfreq/governor"
if [ -f "$GPU_ADRENO" ]; then
  # 高通 Adreno
  echo "msm-adreno-tz" > "$GPU_ADRENO" 2>/dev/null
else
  # 其他 SoC
  for DEVFREQ in /sys/class/devfreq/*; do
    if [ -f "$DEVFREQ/governor" ]; then
      GOV_LIST=$(cat "$DEVFREQ/available_governors" 2>/dev/null)
      if echo "$GOV_LIST" | grep -q "mali_ondemand"; then
        echo "mali_ondemand" > "$DEVFREQ/governor" 2>/dev/null
      elif echo "$GOV_LIST" | grep -q "simple_ondemand"; then
        echo "simple_ondemand" > "$DEVFREQ/governor" 2>/dev/null
      fi
    fi
  done
fi

# Cpuset 核心分配
POSSIBLE_CPUS=$(cat /sys/devices/system/cpu/possible 2>/dev/null)
MAX_CORE=$(echo "$POSSIBLE_CPUS" | awk -F'-' '{print $2}')
if [ -z "$MAX_CORE" ]; then MAX_CORE=7; fi

MID_CORE=$((MAX_CORE - 1)) # 排除最大超大核 (例如8核就是6)
HALF_CORE=$((MAX_CORE / 2)) # 取一半作为小核区 (例如8核就是3，即0-3)

# Top-app
echo "0-$MAX_CORE" > /dev/cpuset/top-app/cpus 2>/dev/null
# Foreground
echo "0-$MID_CORE" > /dev/cpuset/foreground/cpus 2>/dev/null
echo "0-$MAX_CORE" > /dev/cpuset/foreground/boost/cpus 2>/dev/null
# Background & System
echo "0-$HALF_CORE" > /dev/cpuset/background/cpus 2>/dev/null
echo "0-$HALF_CORE" > /dev/cpuset/system-background/cpus 2>/dev/null

# 系统 Settings 参数修改
settings put system config.hw_quickpoweron true
settings put system surface_flinger_use_frame_rate_api true
