# 核心备份与还原模块设计

交接对象：组长 A、GUI 与扩展功能负责人 C。开发负责人：B。

## 工程结构

```text
CMakeLists.txt
core/
  include/core/
    BackupManager.h
    RestoreManager.h
    FileUtils.h
  src/
    BackupManager.cpp
    RestoreManager.cpp
    FileUtils.cpp
tests/CoreTests.cpp
docs/
  CoreDesign.md
  CoreAPI.md
  CoreTestResults.md
```

`core_backup` 是独立库，使用 C++17 与 `std::filesystem`，不依赖 Qt。
`core_backup_tests` 是独立测试程序，通过 CTest 执行。
保留仓库原有 README.md 和 .gitignore。

## 职责与流程

- BackupManager 提供目录备份入口。
- RestoreManager 提供目录还原入口。
- FileUtils 负责源目录验证、路径冲突检查、递归遍历、目录创建和文件复制。
- TransferOptions 提供覆盖策略、筛选回调和文件复制进度回调。
- CoreError 提供可供 GUI 判断的错误分类及诊断信息。

备份与还原均为目录到目录的同步复制：验证路径后创建目标根目录，递归遍历源目录，
筛选条目并创建子目录或复制普通文件。空目录也会创建，文件采用二进制复制。
源目录的内容直接写入目标目录，不额外嵌套一个源目录名称。

## 行为约定

默认拒绝覆盖已有文件，调用者可以显式选择 Overwrite。
覆盖只更新源中存在的条目，目标中额外的文件与目录保留；本模块不做镜像删除。
目录统计包含源根目录，跳过统计按被筛除的条目计数，剪枝目录中的后代不计数。
筛选回调作用于根目录以下的条目；根目录始终创建。

源与目标相同、互为祖先或后代时拒绝操作。路径检查使用规范化绝对路径和路径组件比较，
避免将 `source-other` 误判为 `source` 子目录。
目标路径及其父路径中的符号链接被拒绝，源树内的符号链接、设备和其他特殊文件也被拒绝。
源根路径可以通过符号链接解析到真实目录。硬链接文件按普通文件复制，但禁止覆盖同一文件。

文件系统异常被转换为 CoreError。筛选和进度回调抛出的普通异常原样向上传递。
遇到错误立即停止，不保证回滚；目标可能包含已复制文件或已创建目录。
Overwrite 采用标准文件复制，并非事务或原子替换，失败时目标文件可能不完整。
调用者必须处理失败状态，不能把部分结果标记为完整备份。

## 集成与边界

GUI 可在工作线程调用接口，捕获 CoreError 并按 code() 显示错误。
回调在调用线程执行，Qt 界面更新应通过信号或排队调用返回 GUI 线程。
定时模块可按时调用 backup()，筛选模块可构造 filter 回调；核心不管理线程和定时器。
管理器无成员状态；多个操作若共享目标目录，调用方必须串行执行。

本版本不提供压缩、加密、历史版本、取消、完整性校验、权限/时间戳精确保留、硬链接拓扑保留、
运行中文件的一致性快照或跨进程锁。复制期间请保持源目录稳定，不支持敌对并发替换路径。
Windows 使用中文路径时应由 GUI 转换为原生宽字符路径或使用 fs::u8path，不依赖本地窄字符编码。

## 编译

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -V
```

需要 CMake 3.16 及以上和支持 C++17 filesystem 的编译器。
仅构建核心库时增加 `-DBUILD_TESTING=OFF`。
组员的同一 CMake 工程中可使用 `add_subdirectory` 引入本项目，
然后 `target_link_libraries(your_target PRIVATE core_backup)`，公共头文件路径与 C++17 要求自动传播。
