# OmniSched | 全域灵动调度 🚀

一款基于 Snapdragon 8 Gen 3 顶配调度逻辑，并将其“泛用化”的底层内核与系统优化模块。专为 Android 12+ (API 31+) 设备打造。

## 🌟 核心特性

本模块摒弃了传统的“硬编码”方式，采用纯动态计算，真正做到**通杀所有 Android 12+ 设备**，无视处理器品牌与核心数量：

* **🧠 智能 SoC 识别**：动态遍历并识别高通 Adreno 或联发科/猎户座 Mali GPU 节点，自动应用最佳的动态能效调度器 (`msm-adreno-tz` / `mali_ondemand`)。
* **⚙️ 动态 Cpuset 分配**：自动读取设备 `possible_cpus`，动态计算大、中、小核梯度。无论你是 4 核、8 核还是未来的多核异构处理器，都能完美剥离后台进程，让前台游戏独占算力，后台彻底锁死小核省电。
* **🎮 底层渲染劫持**：在系统刚挂载数据的 `post-fs-data` 阶段，强制接管 `ro.` 属性。全局开启 Vulkan 渲染，启用 SkiaVK 后端，并开启“脏区域渲染 (Dirty Regions)”大幅降低 GPU 功耗。
* **⚖️ Schedutil 全域平衡**：强制统一 CPU 调度器为 `schedutil`，利用现代内核的负载感知能力，实现毫秒级的极速频率伸缩。

## 📱 兼容性

* **Android 版本**：仅限 Android 12 及以上 (API 31+) 
* **处理器环境**：不限 (Qualcomm, MediaTek, Exynos 等均可)
* **Root 环境**：Magisk / KernelSU / APatch 均可完美刷入生效

## 📂 模块文件结构

将下载或自己打包的 ZIP 模块解压后，应包含以下结构：

```text
OmniSched/
├── META-INF/
│   └── com/google/android/update-binary & updater-script (刷入脚本)
├── module.prop         # 模块信息文件
├── post-fs-data.sh     # 渲染层属性劫持 (开机前期执行)
├── service.sh          # Cpuset & 调度器动态计算 (开机完成后执行)
└── README.md           # 本说明文件
