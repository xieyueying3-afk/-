#include "core/FileUtils.h"
#include <system_error>

namespace core {
namespace {
[[noreturn]] void failFs(const fs::filesystem_error& error) {
    const auto ec = error.code();
    ErrorCode code = ErrorCode::IoError;
    if (ec == std::errc::permission_denied || ec == std::errc::operation_not_permitted)
        code = ErrorCode::PermissionDenied;
    else if (ec == std::errc::file_exists)
        code = ErrorCode::AlreadyExists;
    else if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory)
        code = ErrorCode::InvalidPath;
    throw CoreError(code, error.what());
}

bool containsPath(const fs::path& parent, const fs::path& child) {
    auto p = parent.begin();
    auto c = child.begin();
    for (; p != parent.end(); ++p, ++c)
        if (c == child.end() || *p != *c) return false;
    return true;
}

void rejectSymlinkComponents(const fs::path& path) {
    fs::path current;
    for (const auto& part : fs::absolute(path)) {
        current /= part;
        if (fs::is_symlink(fs::symlink_status(current)))
            throw CoreError(ErrorCode::PathConflict, "Symbolic link in destination: " + current.u8string());
    }
}
}

CoreError::CoreError(ErrorCode code, const std::string& message)
    : std::runtime_error(message), code_(code) {}

fs::path FileUtils::checkedDirectory(const fs::path& path) {
    try {
        if (path.empty() || !fs::is_directory(path))
            throw CoreError(ErrorCode::InvalidPath, "Source is not a directory: " + path.u8string());
        return fs::canonical(path);
    } catch (const fs::filesystem_error& error) { failFs(error); }
}

void FileUtils::validateTransferPaths(const fs::path& source, const fs::path& target) {
    try {
        const auto from = checkedDirectory(source);
        if (target.empty()) throw CoreError(ErrorCode::InvalidPath, "Empty destination path");
        rejectSymlinkComponents(target);
        const auto to = fs::weakly_canonical(fs::absolute(target));
        // 双向检查，避免目标位于源内，或源位于目标内导致覆盖。
        if (containsPath(from, to) || containsPath(to, from))
            throw CoreError(ErrorCode::PathConflict, "Source and destination overlap");
        if (fs::exists(to) && !fs::is_directory(to))
            throw CoreError(ErrorCode::InvalidPath, "Destination is not a directory: " + to.u8string());
    } catch (const fs::filesystem_error& error) { failFs(error); }
}

void FileUtils::createDirectories(const fs::path& path) {
    try {
        if (path.empty()) throw CoreError(ErrorCode::InvalidPath, "Empty directory path");
        rejectSymlinkComponents(path);
        fs::create_directories(path);
    } catch (const fs::filesystem_error& error) { failFs(error); }
}

void FileUtils::copyFile(const fs::path& source, const fs::path& target,
                         ExistingFilePolicy policy) {
    try {
        if (!fs::is_regular_file(fs::symlink_status(source)))
            throw CoreError(ErrorCode::UnsupportedEntry, "Source is not a regular file: " + source.u8string());
        rejectSymlinkComponents(target);
        if (fs::exists(target)) {
            if (!fs::is_regular_file(target))
                throw CoreError(ErrorCode::PathConflict, "Destination is not a regular file: " + target.u8string());
            if (fs::equivalent(source, target))
                throw CoreError(ErrorCode::PathConflict, "Source and destination refer to the same file");
            if (policy == ExistingFilePolicy::Fail)
                throw CoreError(ErrorCode::AlreadyExists, "Destination file exists: " + target.u8string());
        }
        if (!target.parent_path().empty()) createDirectories(target.parent_path());
        fs::copy_file(source, target, policy == ExistingFilePolicy::Overwrite
            ? fs::copy_options::overwrite_existing : fs::copy_options::none);
    } catch (const fs::filesystem_error& error) { failFs(error); }
}

void FileUtils::walkDirectory(const fs::path& source,
    const std::function<bool(const fs::path&, bool)>& visitor) {
    try {
        const auto root = checkedDirectory(source);
        for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
            const auto status = it->symlink_status();
            if (!fs::is_directory(status) && !fs::is_regular_file(status))
                throw CoreError(ErrorCode::UnsupportedEntry, "Unsupported entry: " + it->path().u8string());
            const bool directory = fs::is_directory(status);
            const auto relative = it->path().lexically_relative(root);
            if (!visitor(relative, directory) && directory) it.disable_recursion_pending();
        }
    } catch (const fs::filesystem_error& error) { failFs(error); }
}

TransferResult FileUtils::copyDirectory(const fs::path& source, const fs::path& target,
                                        const TransferOptions& options) {
    validateTransferPaths(source, target);
    const auto root = checkedDirectory(source);
    const auto destination = fs::absolute(target).lexically_normal();
    createDirectories(destination);
    TransferResult result;
    result.directoriesVisited = 1;
    walkDirectory(root, [&](const fs::path& relative, bool directory) {
        if (options.filter && !options.filter(relative, directory)) {
            ++result.entriesSkipped;
            return false;
        }
        if (directory) {
            createDirectories(destination / relative);
            ++result.directoriesVisited;
        } else {
            copyFile(root / relative, destination / relative, options.existingFiles);
            ++result.filesCopied;
            if (options.progress) options.progress(relative, result);
        }
        return true;
    });
    return result;
}
} // namespace core
