# Molly-Devices

本项目是一个基于乐鑫 ESP32-S3 芯片和 ESP-IDF 框架开发的智能设备项目。

## 硬件平台

- **主控芯片**: ESP32-S3

## 软件架构

项目采用模块化的组件式开发思想，基于 ESP-IDF v5.4.x 进行构建。

- **开发框架**: [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/)
- **构建系统**: CMake
- **核心思想**:
    - **组件化**: 所有功能被封装在 `components/` 目录下的独立组件中，实现了高度的模块化和可复用性。
    - **事件驱动**: 通过 `event_manager` 组件实现消息驱动，降低了各业务模块之间的耦合度。

## 已完成功能模块

以下是当前已开发完成的功能组件列表：

- **`app_logic`**: 存放应用核心业务逻辑。
- **`bsp`**: 板级支持包（Board Support Package），提供基础的硬件引脚定义和初始化。
- **`event_manager`**: 一个轻量级的事件分发器，用于实现组件间的异步通信和解耦。
- **`feature_anim_player`**: 动画播放器，用于驱动显示或灯光效果。
- **`feature_button`**: 通用按键驱动组件，支持按键事件的注册和回调。
- **`feature_buzzer`**: 蜂鸣器驱动，用于播放提示音，已移除。
- **`feature_motor`**: 电机驱动与控制模块。
- **`storage_manager`**: 存储管理器，封装了对设备内部 Flash 或外部存储介质的读写操作。

## 开发环境

- **ESP-IDF 版本**: `v5.4.x`
- **Python 版本**: `3.11.x`
- **构建**: `idf.py build`
- **烧录**: `idf.py -p <PORT> flash`
- **监视**: `idf.py -p <PORT> monitor`

## 目录结构

```
.
├── main/                 # 主程序入口
│   └── main.c
├── components/           # 自定义功能组件
│   ├── app_logic/
│   ├── bsp/
│   ├── event_manager/
│   └── ...
├── storage/              # 存储相关实现
├── managed_components/   # ESP-IDF 包管理器下载的组件
├── CMakeLists.txt        # 顶层 CMake 构建文件
└── partitions.csv        # 分区表
```

## 开发进度汇报

此部分将持续更新，以反映最新的开发状态。

| 功能模块 | 状态 | 负责人 | 备注 |
| :--- | :--- | :--- | :--- |
| 基础框架搭建 | ✅ 已完成 | - | 基于 ESP-IDF 的组件化架构 |
| `bsp` | ✅ 已完成 | - | 完成硬件初始化配置 |
| `event_manager` | ✅ 已完成 | - | 实现事件注册、分发机制 |
| `feature_button` | ✅ 已完成 | - | 实现按键驱动及事件上报 |
| `feature_buzzer` | 已移除 | - | 蜂鸣器驱动 |
| `feature_motor` | ✅ 已完成 | - | 实现电机驱动 |
| `storage_manager` | ✅ 已完成 | - | 封装存储接口 |
| `feature_anim_player` | ✅ 已完成 | - | 实现动画播放逻辑 |

---
*该 README.md 由 AI 编程助手根据项目结构自动生成。*
