# OmniSched

![Android](https://img.shields.io/badge/Android-12%2B-3DDC84?style=flat-square&logo=android)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=c%2B%2B)
![Zygisk](https://img.shields.io/badge/Zygisk-Enabled-0f172a?style=flat-square)
![KernelSU](https://img.shields.io/badge/KernelSU-WebUI-white?style=flat-square)
![License](https://img.shields.io/badge/License-Mulan_PubL_2.0-red?style=flat-square)

OmniSched 是一个面向 Android 12+ 的 Root 性能模块，主要由三部分组成：

- 原生 daemon：负责套用 scheduler、cpuset、内存、I/O、GPU 与 thermal 调校
- Zygisk 模块：负责渲染属性控制与按应用注入 Vulkan 行为
- WebUI：负责编辑运行时配置

当前代码库使用单一 JSON 配置源。模块开机脚本会在需要时生成默认配置，daemon 会自动重新加载配置，WebUI 和手动修改配置文件都会驱动同一套运行逻辑。

## 功能概览

- 调整 `top-app`、`foreground`、`background`、`system-background` 的 `cpuset`
- 在系统节点存在时套用 `uclamp` 与 `schedtune` 调校
- 调整 `schedutil` 的 rate limit 以及可选的 CPU 频率上下限
- 按平台自动选择 CPU governor，或使用显式覆写值
- 套用内存调校，例如 Multi-Gen LRU、swappiness、watermark、compaction
- 调整 block queue 参数，例如 `read_ahead_kb`、`nr_requests`、`rq_affinity`
- 对常见 Adreno / Mali 风格节点套用 GPU governor 偏好
- 在内核支持时启用 input boost 相关节点
- 通过 thermal guard 在高温时收敛调度激进度
- 支持全局 Vulkan 属性模式与按应用 Vulkan 注入模式
- 支持短视频应用前台 30Hz 刷新率限制

## 工作方式

OmniSched 默认使用以下配置文件：

```text
/data/adb/zygisk_omnisched/config.json
```

旧路径 `/data/adb/omnisched/config.json` 仍会被模块脚本兼容读取，并在需要时迁移到新路径。

daemon 会监看：

- scheduler 相关的 sysfs / cpuset 节点
- 配置目录
- 配置文件本身
- 无文件事件时的 fallback 轮询时间

因此通过 WebUI 保存，或者手动直接编辑配置文件，通常都不需要重启设备就能重新生效。

## 支持环境

- Android 12 及以上
- arm64-v8a
- Magisk
- KernelSU
- APatch

## 配置格式

以下示例对应当前实现：

```json
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
```

### 关键字段

- `power.policy`：`powersave`、`balanced`、`performance`
- `performance.auto_optimize`：根据 CPU 拓扑自动选择较合适的策略
- `render.vulkan_mode`：`off`、`global`、`per_app`
- `render.vulkan_apps`：仅在 `per_app` 模式下使用的应用包名列表
- `scheduler.*`：`uclamp` 与 `schedutil` 覆写参数
- `cpu.*`：cpuset、governor、频率上下限覆写参数
- `input.boost_ms`：input boost 时长覆写
- `thermal.*`：thermal guard 温度门限与 clamp 上限覆写
- `display.short_video_*`：短视频前台命中时的刷新率限制与包名列表

大多数覆写字段使用 `-1` 表示“使用 OmniSched 内置默认值”。

## Vulkan 模式

`render.vulkan_mode` 目前支持三种模式：

- `off`：不强制启用 Vulkan
- `global`：在开机阶段套用全局渲染属性
- `per_app`：仅对 `render.vulkan_apps` 内列出的应用注入 Vulkan 相关环境与属性拦截

按应用 Vulkan 模式由 Zygisk 模块在 app specialize 阶段完成。

## WebUI

当前 WebUI 可直接调整：

- 电源策略
- auto optimize
- 触控 boost 与温控保护开关
- 高级选项里的 scheduler、memory、I/O、GPU 开关
- Vulkan 模式与按应用包名列表
- 短视频 30 帧限制与包名列表
- fallback 轮询时间
- 后台小核限制
- lite mode
- 进阶项：governor override、top-app `uclamp`、input boost 时长、thermal 温度门限

如果你的环境支持 KernelSU WebUI，可以直接从模块管理页打开。

## 安装流程

1. 先卸载、停用或移除旧版 OmniSched 模块，避免残留旧脚本与旧配置。
2. 通过 Magisk、KernelSU 或 APatch 刷入模块 zip。
3. 刷入完成后重启设备。
4. 开机后可直接使用 WebUI，或手动编辑 `/data/adb/zygisk_omnisched/config.json`。

首次开机如果不存在配置文件，模块脚本会自动生成一份完整默认配置。

## 构建方式

项目当前会生成两个原生目标：

- `omnisched`：原生 daemon
- `arm64-v8a.so`：Zygisk 共享库

### 构建依赖

- Android NDK
- CMake 3.30.1 及以上
- Ninja 或兼容的构建生成器

### 示例流程

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-31 \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build
```

项目通过 `FetchContent` 获取 `nlohmann_json`。

## 项目结构

```text
include/              对外头文件
src/                  daemon 与 Zygisk 源码
module/               开机脚本与模块侧运行逻辑
webroot/              WebUI 资源
CMakeLists.txt        原生构建定义
module.prop           模块元数据
customize.sh          安装定制脚本
```

## 补充说明

- 很多调校节点依赖 kernel 与厂商实现差异，OmniSched 只会在节点存在时写入。
- 不同 SoC、内核、散热设计与 ROM patch 下，实际效果可能差异很大。
- 激进设置可能提升温度、增加耗电，甚至让整体体验变差。
- daemon 在应用 cpuset 覆写前会先规范化 CPU 范围字符串，以减少重复或格式混乱的组合。

## 授权

本项目使用 **木兰公共许可证，第 2 版（Mulan PubL v2）** 授权。

为避免 README 中的二次转述与许可证原文发生偏差，下面这部分说明仅用于帮助理解项目分发边界，**不替代许可证原文本身**。具体权利义务请以许可证全文为准。
- 木兰公共许可证，第 2 版：<http://license.coscl.org.cn/MulanPubL-2.0>

您可以将您接收到的“贡献”或您的“衍生作品”以源程序形式或可执行形式重新“分发”，但必须满足下列条件：

1.您必须向接收者提供“本许可证”的副本，并保留“贡献”中的版权、商标、专利及免责声明；并且  
2.如果您“分发”您接收到的 “贡献”，您必须使用“本许可证”提供该“贡献”的源代码副本；如果您 “分发”您的“衍生作品”，您必须：
- 随“衍生作品”提供使用“本许可证”“分发”的您的“衍生作品”的“对应源代码”。如果您通过下载链接提供前述“对应源代码”，则您应将下载链接地址置于“衍生作品”或其随附文档中的明显位置，有效期自该“衍生作品”“分发”之日起不少于三年，并确保接收者可以获得“对应源代码”；或者，
- 随“衍生作品”向接收者提供一个书面要约，表明您愿意提供根据“本许可证”“分发”的您“衍生作品”的“对应源代码”。该书面要约应置于“衍生作品”中的明显位置，并确保接收者根据书面要约可获取“对应源代码”的时间从您接到该请求之日起不得超过三个月，且有效期自该“衍生作品”“分发”之日起不少于三年。

### 关于衍生作品的理解

木兰公共许可证，第 2 版对衍生作品范围有相对明确的定义。按照其文本，一般会把以下情形视为衍生或受许可证约束的修改成果：

- 直接修改本项目源码
- 基于本项目源码进行改写、翻译、注释、重组
- 将本项目代码与其他代码形成许可证定义下的组合或链接成果


## 免责声明

本项目会修改 Android 底层调度、cpuset、渲染属性与部分性能相关节点。使用前请确认你已经理解以下风险：

- 不同 SoC、Kernel、ROM、厂商补丁与温控策略差异很大，效果不保证一致
- 激进参数可能导致发热升高、续航下降、卡顿、异常耗电，甚至系统不稳定
- 某些内核节点不存在、权限不同，或厂商实现与预期不一致时，调校结果可能与文档描述不同
- Vulkan、scheduler、频率与 thermal 相关调整都可能影响前台体验与后台保活行为

使用本项目即表示你愿意自行承担测试、备份、参数回退与设备恢复责任。对于因刷入模块、修改配置、二次分发或设备环境差异导致的数据丢失、系统异常、兼容性问题或其他损失，项目作者与贡献者不承担担保责任。
