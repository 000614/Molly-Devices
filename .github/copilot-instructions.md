# Copilot Instructions for Molly-Devices

## 项目架构概览
- 本项目基于 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/)，采用 CMake 构建，主控芯片为 ESP32-S3。
- 主要目录结构：
  - `main/`：主应用入口（如 `app_main.c`）。
  - `components/`：自定义功能组件（如 `event_manager`、`bsp`、`drv_button` 等），每个组件有独立 `CMakeLists.txt`。
  - `storage/`：存储相关实现。
  - `build/`：自动生成的构建输出，勿提交。
  - `managed_components/`：通过 ESP-IDF 管理的外部依赖。
  - `esp-idf/`：ESP-IDF 框架源码（如有）。

## 关键开发流程
- **构建/烧录**：
  - 推荐使用 `idf.py build` 构建，`idf.py -p <PORT> flash` 烧录。
  - Windows 下建议用 VS Code + ESP-IDF 插件，或命令行（CMD/PowerShell）。
  - `CMakeLists.txt` 顶层已包含 ESP-IDF 必要引导。
- **配置**：
  - `sdkconfig` 由 `idf.py menuconfig` 生成和维护，勿手动编辑。
  - 组件依赖通过 `idf_component_register(... PRIV_REQUIRES ...)` 指定。
- **调试/日志**：
  - 默认串口波特率 115200（见 `sdkconfig`）。
  - 日志等级默认 INFO，可通过 `menuconfig` 调整。
- **单元测试**：
  - 启用 Unity 测试框架（见 `sdkconfig`，`CONFIG_UNITY_ENABLE_IDF_TEST_RUNNER=y`）。
  - 测试建议放在各自组件下，命名为 `test_*.c`。

## 约定与模式
- 组件化开发，所有新功能建议以组件形式添加到 `components/`，并在主程序或其他组件中通过 `PRIV_REQUIRES` 依赖。
- 事件驱动推荐使用 `event_manager` 组件，避免直接跨组件调用。
- 配置、分区表等均通过标准 ESP-IDF 机制维护（如 `partitions.csv`、`sdkconfig`）。
- FAT 文件系统相关逻辑参考 `create_fat_image.py` 和 `storage/`。

## 外部依赖与集成
- 依赖 ESP-IDF v5.4.x，路径在 `.vscode/settings.json` 里配置。
- 需安装 Python 3.11.x 及 ESP-IDF 工具链。
- 通过 `managed_components/` 管理部分外部库。

## 其他说明
- `.gitignore` 已排除构建产物和依赖目录。
- 参考 [小智 AI 编程指南](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb) 获取更多开发细节。

---
如需补充或有疑问，请在此文件下方留言或提交 PR。