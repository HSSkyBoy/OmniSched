# OmniSched | 全域灵动 🚀

![Android](https://img.shields.io/badge/Android-12%2B-3DDC84?style=flat-square&logo=android)
![C++](https://img.shields.io/badge/C++-17-00599C?style=flat-square&logo=c%2B%2B)
![Magisk](https://img.shields.io/badge/Magisk-23.0%2B-000000?style=flat-square)
![KernelSU](https://img.shields.io/badge/KernelSU-WebUI-white?style=flat-square)
![License](https://img.shields.io/badge/License-Mulan_PubL_2.0-red?style=flat-square)

OmniSched 是一款重构安卓调度逻辑的现代化 Android 底层优化模组。**v3.0 迎来史诗级架构转移 (Paradigm Shift)**，我们彻底抛弃了传统模组中「单次 Shell 脚本执行」, 「写死核心数」, 「固定轮询」与「属性硬编码」的过时做法，全面转向 **响应式事件驱动 (Event-Driven)** 与 **资料驱动配置**，并深度适配 Android 14+ 的底层特性。将旗舰级的效能与能耗比带给每一台设备。

## 💡 为什么选择 OmniSched？
市面上多数的开源调度模组往往针对特定机型硬编码，或使用极度耗电的 Shell 循環监听。一旦刷入不同架构的设备，轻则无效，重则导致系统卡死、发热耗电。
* OmniSched v3.0 采用 C++ `epoll` + `inotify` 实现近乎零功耗的背景常驻，并自带 WebUI 仪表板：**一次刷入，全平台自适应，且支援无缝热重载**。

## 🌟 核心黑科技 (v3.0 全新架构)
- ⚡ **原生 C++ 响应式守护进程 (Reactive Daemon)**
  - 舍弃耗电的 `sleep` 轮询，改用 Linux 内核级 `inotify` 事件流监听。仅当系统或其他模组篡改调度节点时瞬间唤醒拦截，达成**零无效轮询 (Zero Polling Overhead)**。
- 🎨 **Kotlin 开发哲学的现代化架构设计 (Kotlin-Inspired Design)**
  - 打破传统底层开发的思维，全面导入 Kotlin 的优雅设计模式，让守护进程极度稳定且易于维护：
    - **唯读快取 (Data Class) & 延迟初始化 (by lazy)**：利用 Meyers Singleton 确保 CPU 拓扑结构只在首次获取时执行一次扫描，彻底消除重复 I/O。
    - **介面隔离 (Sealed Interface)**：实作 `RootAdapter` 依赖反转，利用多型动态桥接 Magisk、KernelSU 与 APatch。
    - **空值安全 (Null Safety)**：使用 `std::optional` 搭配 fallback 机制 (类似 Elvis 运算子 `?:`) 安全解析核心节点与 JSON 配置，杜绝崩溃。
- 🎛️ **KernelSU 原生 WebUI 与动态配置 (Hot Reload)**
  - 告别修改程式码与 `system.prop`！模组内建轻量前端仪表板，所有设定皆序列化至 `/data/adb/omnisched/config.json`。更改配置后，守护进程将瞬间唤醒、热重载并套用，**完全无需重启设备**。
- ⚙️ **Android 14+ 深度调度与记忆体适配**
  - **MGLRU & ZRAM**：强制启用 Multi-Gen LRU (层级 7) 并优化 Swappiness 与 API 34+ 的批次清理 (batch_kill)。
  - **Uclamp 算力钳制**：为前台 Top-app 设定保底算力 (min: 10)，并将 Background 背景进程严格限制最大频率与锁死于省电小核。
  - **底层渲染劫持**：动态切换至 Skia + Graphite (A14+) 或 SkiaVK 渲染，全域强制开启 Vulkan 引擎，大幅降低 GPU 负载。

## 📱 系统相容性
* **系统版本**：Android 12 及以上，并针对 Android 14+ 进行深度最佳化。
* **Root 环境**：完美相容 Magisk (v23.0+) / KernelSU / APatch。

## 📥 安装与使用

1. 若已安装 OmniSched v1.x/2.x 以前版本，请先在 Root 管理器中卸载。
2. 在 Release 页面下载最新版的 `OmniSched-V3-Release.zip`。
3. 透过 Root 管理器选择「从本机安装」，观察安装介面的设备侦测提示，重启手机启用模组。
4. **配置参数 (V3 全新功能)**：
  - **KernelSU 用户**：在模组列表中点击 OmniSched 的齿轮图示，即可开启 WebUI 仪表板，支援图形化即时修改巡检间隔、强制 Vulkan 开关与小核限制。
  - **Magisk / APatch 用户**：可使用任何文字编辑器修改 `/data/adb/omnisched/config.json`，存档瞬间即自动生效，无需重启。

## ⚖️ 律法与授权 (License)
本专案采用 木兰公共许可证第2版 (Mulan PubL-2.0) 授权：
1. **强制要求下游开源**：任何基于本专案的修改、二次开发、程式码引用，其衍生专案必须同样采用 Mulan PubL-2.0 或同等 Copyleft 协议开源。禁止任何形式的闭源二改。
2. **必须保留署名与原出处**：在您的模组安装介面（UI）、README 说明、程式码注解中，必须显著标注原作者署名及本专案仓库连结。严禁抹除作者资讯或伪装成自研。
3. **禁止商业牟利与欺诈**：
  - **禁止倒卖**：严禁将本模组打包在闲鱼、淘宝等平台付费出售。
  - **禁止收费进群**：严禁将本模组作为付费群、付费频道的「独家内容」。
  - **禁止欺骗**：若发现下游专案违反上述条款，原作者有权对违规专案进行公开披露。
> 程式码可以免费分享，但劳动成果不容剽窃。尊重开源，遵守约定，否则请勿使用本专案程式码。

⚠️ 免责声明
本模组涉及 Android 底层 Cpuset 与 GPU/CPU 调度调整。刷机有风险，因使用本模组导致的任何资料遗失或设备异常，开发者不承担任何责任。建议刷入前做好重要资料备份。