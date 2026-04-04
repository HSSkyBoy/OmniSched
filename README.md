# OmniSched | 全域灵动 🚀

![Android](https://img.shields.io/badge/Android-12%2B-3DDC84?style=flat-square&logo=android)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=c%2B%2B)
![NeoZygisk](https://img.shields.io/badge/NeoZygisk-Enabled-0f172a?style=flat-square)
![KernelSU](https://img.shields.io/badge/KernelSU-WebUI-white?style=flat-square)
![License](https://img.shields.io/badge/License-Mulan_PubL_2.0-red?style=flat-square)

OmniSched 是一款针对 Android 12+ 深度重构的现代化底层效能与渲染最佳化模组。在 v4.0.0 的全新架构中，我们全面导入了 Data-Driven 与 Reactive 设计哲学。核心由原生 C++20 守护行程（Daemon）、Zygisk Hook 以及轻量级 WebUI 所组成。我们摒弃了传统写死参数的作法，将重点放在低开销的事件驱动调度、精确的 Vulkan 渲染控制以及无缝的热重载机制。

## 🌟 核心架构与特性
- **响应式事件驱动 (Event-Driven Daemon)**
  以 Linux 核心级的 `epoll` 与 `inotify` 取代耗电的传统轮询（Polling）。仅在系统调度节点发生变更时唤醒并拦截，达成近乎零功耗的背景常驻。
- **动态介面隔离与多 Root 环境支援 (Polymorphic Root Adapter)**
  实作依赖反转（Dependency Inversion），透过统一的 `IRootAdapter` 介面，动态桥接并完美支援 KernelSU、Magisk 与 APatch 环境，一次刷入全平台自适应。
- **Zygisk 渲染劫持 (Per-App Vulkan)**
  透过 Zygisk API 拦截 `preAppSpecialize` 阶段，提供更细致的 `per_app` 渲染模式。仅针对配置清单内的目标应用程式注入 Vulkan (SkiaVK) 环境变数，不干扰全域系统稳定性。
- **Android 14+ 深度调度与记忆体适配**
  强制启用 Multi-Gen LRU (层级 7)、最佳化 ZRAM Swappiness，并透过 Uclamp 算力钳制，动态为前台 Top-app 设定保底算力，同时严格限制背景处理序的最大频率与核心分配。
- **单一信任来源与热重载 (Single Source of Truth & Hot Reload)**
  安装脚本、守护行程与 WebUI 皆共用 `/data/adb/zygisk_omnisched/config.json` 状态设定档。无论是透过 WebUI 或文字编辑器修改，存档瞬间即触发守护行程的热重载并即时套用。

## ⚙️ 配置与预设 格式
目前的 JSON 设定档采用以下格式，并支援向下相容旧版的 `force_vulkan` 栏位：

```json
{
  "poll_interval_seconds": 950,
  "cpuset": {
    "background_little_core_only": true
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
    "lite_mode": false
  }
}
```

### 🎮 Vulkan 模式解析 (`render.vulkan_mode`)
- `off`：维持系统预设渲染行为（预设值）。
- `global`：在开机流程中套用全域 Vulkan 导向属性（如 `ro.hwui.renderer=skiavk`）。
- `per_app`：由 Zygisk 实作读取 `vulkan_apps` ，在应用程式的 Process Specialize 阶段精准注入 Vulkan 属性。

## 📥 安装指南
1. 请先于 Root 管理器中卸载任何旧版的 OmniSched 模组。(推荐)
2. 下载最新的 `zygisk_omnisched-v4.x.x.zip`。
3. 透过 Magisk / KernelSU / APatch 刷入模组。
4. **互动式安装**：在安装过程中，终端机会提示您透过 **音量键** 选择是否预设启用「全域强制 Vulkan」。若无输入则倒数后预设为关闭。
5. 安装完成后，重新开机一次即可生效。

## 🖥️ WebUI 管理介面
推荐 **KernelSU** 的使用者直接点击模组列表中的齿轮图示开启 WebUI。
WebUI 支援图形化即时调整：
- 电源策略（节能 / 平衡 / 效能）
- Vulkan 模式切换与特定 App 套件名称管理
- 轮询间隔设定
- 背景小核限制开关
- 智慧自动最佳化 (`auto_optimize`)

## ⚖️ 授权条款 (License)
本专案采用 **木兰公共许可证第2版 (Mulan PubL-2.0)** 授权：
1. **强制下游开源**：基于本专案的任何修改与衍生作品，必须采用同等 Copyleft 协议开源，禁止任何形式的闭源二改。
2. **保留署名**：必须显著标注原作者署名及专案连结。
3. **禁止商业牟利与欺诈**：严禁倒卖、作为付费群独家内容或隐瞒原作者资讯。
> 程式码可以免费分享，但劳动成果不容剽窃。尊重开源，遵守约定，否则请勿使用本专案程式码。

### ⚠️ 免责声明
本模组涉及 Android 底层 Cpuset、Uclamp 调度调整，并包含静态的系统属性覆写（如硬体加速与 UI 渲染机制）。
刷机有风险，因使用本模组导致的任何资料遗失或设备异常，开发者不承担任何责任，建议刷入前做好可正常还原的备份。