#pragma once

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>

namespace core {
namespace fs = std::filesystem;

enum class ErrorCode { InvalidPath, PathConflict, AlreadyExists, PermissionDenied,
                       UnsupportedEntry, IoError };

class CoreError : public std::runtime_error {
public:
    CoreError(ErrorCode code, const std::string& message);
    ErrorCode code() const noexcept { return code_; }
private:
    ErrorCode code_;
};

enum class ExistingFilePolicy { Fail, Overwrite };

struct TransferResult {
    std::size_t filesCopied = 0;
    std::size_t directoriesVisited = 0;
    std::size_t entriesSkipped = 0;
};

struct TransferOptions {
    ExistingFilePolicy existingFiles = ExistingFilePolicy::Fail;
    // 相对源目录的路径；目录返回 false 时跳过整个子树。
    std::function<bool(const fs::path&, bool isDirectory)> filter;
    // 每次成功复制文件后调用；同步执行，不依赖 Qt。
    std::function<void(const fs::path&, const TransferResult&)> progress;
};

class FileUtils {
public:
    static fs::path checkedDirectory(const fs::path& path);
    static void validateTransferPaths(const fs::path& source, const fs::path& target);
    static void createDirectories(const fs::path& path);
    static void copyFile(const fs::path& source, const fs::path& target,
                         ExistingFilePolicy policy = ExistingFilePolicy::Fail);
    // visitor 收到相对路径；目录返回 false 会停止遍历该子树。
    static void walkDirectory(const fs::path& source,
        const std::function<bool(const fs::path&, bool)>& visitor);
    static TransferResult copyDirectory(const fs::path& source, const fs::path& target,
                                        const TransferOptions& options = {});
};
} // namespace core
