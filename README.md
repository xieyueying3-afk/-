# 核心备份与还原模块

大学《软件开发综合实验》三人小组项目，当前实现由组员 B 负责的核心备份与还原模块。
核心库使用 C++17 和 `std::filesystem`，不依赖 Qt，可由组员 C 的 GUI、筛选模块和定时备份模块直接调用。

## 当前功能

- 递归备份和还原普通文件、子目录，保留相对目录结构及空目录。
- 默认拒绝覆盖已有文件，支持显式覆盖策略。
- 检查无效路径、源目标重叠、权限不足等错误，提供统一异常接口。
- 支持路径筛选回调和文件复制进度回调。
- 提供独立测试程序；Linux 实测 14 组用例通过，0 失败、0 跳过。

目前尚未实现 GUI 和定时器。复制失败可能留下部分结果，不会自动回滚；覆盖模式不会删除目标中额外的文件。

## 开发环境

| 工具 | 要求 | 当前云环境实测 |
| --- | --- | --- |
| C++ 编译器 | 支持 C++17 和 `std::filesystem` | GCC 14.2.0 |
| CMake | 3.16 或以上 | 4.4.4，安装于 `/tmp/core-backup-tools` |
| 构建工具 | 与 CMake 生成器匹配，例如 Make、Ninja 或 MSBuild | Make |
| Qt | 核心库和测试无需安装 | 未用于核心构建 |

当前云环境仓库路径为 `/workspace/-`，开发分支为 `feature/core-backup`。
Windows 和 Qt 集成尚未实测；Windows 可使用支持 C++17 的 Visual Studio C++ 工具链与 CMake。

## 快速构建与测试

在仓库根目录执行。系统已安装 CMake 时：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel 2
ctest --test-dir build -C Debug --output-on-failure -V
```

成功后生成核心库 `core_backup` 和测试程序 `core_backup_tests`。
Linux 默认 Make 构建可直接运行 `./build/core_backup_tests`；Visual Studio 等多配置生成器的测试程序通常位于 `build/Debug/`。
CTest 注册一个可执行程序，该程序内部运行多组测试。

只构建核心库、不构建测试：

```bash
cmake -S . -B build-core -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --config Debug --parallel 2
```

### 当前云环境的 CMake 命令

本环境的 CMake 未加入系统 PATH，可直接使用已安装的绝对路径：

```bash
cd /workspace/-
/tmp/core-backup-tools/cmake/data/bin/cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
/tmp/core-backup-tools/cmake/data/bin/cmake --build build --parallel 2
/tmp/core-backup-tools/cmake/data/bin/ctest --test-dir build --output-on-failure -V
```

如果环境重建后该临时目录不存在，可在有 Python、pip 和网络的 Linux 环境重新安装：

```bash
pip install --target /tmp/core-backup-tools cmake
```

`/tmp` 下的安装不是仓库依赖，其他组员使用自己的 CMake 即可。

## 项目结构

```text
.
├── CMakeLists.txt                 # 核心库与测试构建入口
├── core/
│   ├── include/core/
│   │   ├── BackupManager.h        # 备份入口
│   │   ├── RestoreManager.h       # 还原入口
│   │   └── FileUtils.h            # 文件操作、选项、结果与异常
│   └── src/
│       ├── BackupManager.cpp
│       ├── RestoreManager.cpp
│       └── FileUtils.cpp
├── tests/CoreTests.cpp            # 独立测试程序
└── docs/
    ├── CoreDesign.md              # 设计与行为边界
    ├── CoreAPI.md                 # 接口和调用示例
    └── CoreTestResults.md         # 实测环境与测试记录
```

构建产物放在 `build/` 或 `build-*` 中，已由 `.gitignore` 排除。

## 组员集成

GUI 目标在同一 CMake 工程中链接核心库：

```cmake
target_link_libraries(your_gui_target PRIVATE core_backup)
```

如果 GUI 使用独立的顶层 CMake 工程，先用 `add_subdirectory` 引入本仓库。
链接后公共头文件路径与 C++17 要求自动传播。

- 组长 A：阅读 [CoreDesign.md](docs/CoreDesign.md) 了解模块边界和集成方式。
- 组员 C：阅读 [CoreAPI.md](docs/CoreAPI.md) 使用 `BackupManager::backup()`、`RestoreManager::restore()` 和回调接口。
- 测试与验收：阅读 [CoreTestResults.md](docs/CoreTestResults.md) 查看覆盖范围和平台限制。

备份与还原是同步调用，GUI 应在工作线程执行，回调中的界面更新通过 Qt 信号或排队调用处理。
多个任务共享同一目标目录时，由调用方串行执行。

## 协作约定

当前核心模块在 `feature/core-backup` 分支开发。各组员在自己的功能分支修改代码，提交前完成构建和测试，
经组内确认后再合并到 `main`。
