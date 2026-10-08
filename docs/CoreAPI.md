# 核心 API

全部接口位于 `core` 命名空间，`fs` 是 `std::filesystem` 的别名。
包含 `core/BackupManager.h` 或 `core/RestoreManager.h` 即可使用相应管理器和公共类型。

## 备份与还原

```cpp
TransferResult BackupManager::backup(
    const fs::path& source, const fs::path& backupDirectory,
    const TransferOptions& options = {}) const;

TransferResult RestoreManager::restore(
    const fs::path& backupDirectory, const fs::path& destination,
    const TransferOptions& options = {}) const;
```

源必须是现有目录，目标可不存在或为现有目录。返回结果只代表成功完成的调用，
出错时抛异常。两者都保留源目录内的相对路径与空目录，均支持筛选和显式覆盖。

```cpp
#include "core/BackupManager.h"
#include "core/RestoreManager.h"

core::BackupManager backup;
core::RestoreManager restore;
try {
    const auto result = backup.backup("data", "backups/current");
    restore.restore("backups/current", "restored-data");
    // result.filesCopied 可供界面展示成功复制的文件数。
} catch (const core::CoreError& error) {
    // 根据 error.code() 分类处理；error.what() 用于诊断。
}
```

## 选项与回调

| 字段 | 类型 / 默认值 | 约定 |
| --- | --- | --- |
| existingFiles | ExistingFilePolicy::Fail | 遇到同名文件报错；Overwrite 显式覆盖 |
| filter | function<bool(const fs::path&, bool)>，空 | 相对路径与是否目录；false 跳过条目，目录则剪枝 |
| progress | function<void(const fs::path&, const TransferResult&)>，空 | 文件复制成功后执行；相对路径和当前累计统计 |

```cpp
core::TransferOptions options;
options.existingFiles = core::ExistingFilePolicy::Overwrite;
options.filter = [](const core::fs::path& relative, bool directory) {
    if (directory) return relative.filename() != ".cache";
    return relative.extension() != ".tmp";
};
options.progress = [](const core::fs::path& relative,
                      const core::TransferResult& progress) {
    // 在工作线程中发送进度信号，不直接修改 GUI。
    (void)relative;
    (void)progress;
};
backup.backup("data", "backups/current", options);
```

TransferResult 包含 `filesCopied`、`directoriesVisited`、`entriesSkipped`，均为 size_t。
directoriesVisited 包含根目录与接受的子目录（即使目标目录已经存在）。
回调参数引用仅在该次回调期间有效，需要异步使用时必须复制。
进度不提供总数或百分比；如需总数，可用 walkDirectory 在操作前自行统计。

## 公共文件操作

| FileUtils 静态方法 | 行为 |
| --- | --- |
| checkedDirectory(path) | 验证源目录，返回 canonical 路径 |
| validateTransferPaths(source, target) | 验证目录与包含关系，不写入目标 |
| createDirectories(path) | 创建缺失的父目录及目录；拒绝目标符号链接 |
| copyFile(source, target, policy = Fail) | 复制普通文件，自动创建父目录，拒绝自身复制 |
| walkDirectory(source, visitor) | 深度递归遍历；相对路径和目录标记；visitor 返回 false 时剪枝目录 |
| copyDirectory(source, target, options = {}) | 完整目录复制，返回 TransferResult |

walkDirectory 的文件访问顺序由文件系统决定；普通文件的 visitor 返回值不会影响其他条目。
copyFile 不接受源符号链接，目标为目录时报 PathConflict。

## 错误分类

| ErrorCode | 常见情况 |
| --- | --- |
| InvalidPath | 源不存在、源不是目录、目标根为文件、空路径、父路径无效 |
| PathConflict | 源目标重叠、复制到同一文件、目标文件位置是目录、目标符号链接 |
| AlreadyExists | 默认策略遇到已有文件，或目录创建遇到已有同名文件 |
| PermissionDenied | 操作系统拒绝读取、遍历、创建或写入 |
| UnsupportedEntry | 源树中的符号链接或特殊文件、copyFile 源不是普通文件 |
| IoError | 其他文件系统错误，例如磁盘空间不足 |

CoreError 继承 std::runtime_error，`code() noexcept` 返回分类，`what()` 返回诊断。
操作系统错误分类取决于 filesystem 返回的错误码。异常不承诺撤销已完成写入。
