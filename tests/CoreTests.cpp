#include "core/BackupManager.h"
#include "core/RestoreManager.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

using namespace core;
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void write(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << data;
    require(static_cast<bool>(stream), "Cannot write fixture");
}
std::string read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    require(static_cast<bool>(stream), "Cannot read fixture");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
std::map<fs::path, std::string> snapshot(const fs::path& root) {
    std::map<fs::path, std::string> result;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        result[entry.path().lexically_relative(root)] = entry.is_directory()
            ? "D" : "F" + read(entry.path());
    }
    return result;
}
void expectError(ErrorCode expected, const std::function<void()>& operation) {
    try { operation(); }
    catch (const CoreError& error) {
        require(error.code() == expected, std::string("Unexpected error: ") + error.what());
        return;
    }
    throw std::runtime_error("Expected CoreError");
}
struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            path = fs::temp_directory_path() / ("core-backup-tests-" + std::to_string(seed)
                                              + "-" + std::to_string(attempt));
            if (fs::create_directory(path)) return;
        }
        throw std::runtime_error("Cannot allocate temporary directory");
    }
    ~TemporaryDirectory() { std::error_code ec; fs::remove_all(path, ec); }
};
}

int main() {
    TemporaryDirectory temporary;
    const auto base = temporary.path;
    BackupManager backup;
    RestoreManager restore;
    int passed = 0, failed = 0, skipped = 0;
    const auto test = [&](const char* name, const std::function<void()>& body) {
        try { body(); ++passed; std::cout << "[PASS] " << name << '\n'; }
        catch (const std::exception& error) {
            ++failed; std::cout << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    };
    const auto source = base / "source";
    write(source / "root.txt", "root data\n");
    write(source / "level1/level2/binary.dat", std::string("a\0b\xff", 4));
    write(source / fs::u8path(u8"中文目录/实验文件.txt"), "UTF-8 filename content");
    fs::create_directories(source / "empty/nested-empty");
    test("backup tree, binary data, Chinese filenames and empty directories", [&] {
        auto result = backup.backup(source, base / "backup");
        require(result.filesCopied == 3 && result.directoriesVisited == 6, "Incorrect statistics");
        require(snapshot(source) == snapshot(base / "backup"), "Backup differs");
    });
    test("restore complete tree", [&] {
        restore.restore(base / "backup", base / "restored");
        require(snapshot(source) == snapshot(base / "restored"), "Restore differs");
    });
    test("empty root backup and restore", [&] {
        fs::create_directory(base / "empty-source");
        auto result = backup.backup(base / "empty-source", base / "empty-backup");
        restore.restore(base / "empty-backup", base / "empty-restored");
        require(result.filesCopied == 0 && fs::is_empty(base / "empty-restored"), "Empty tree differs");
    });
    test("duplicate backup rejects existing file", [&] {
        expectError(ErrorCode::AlreadyExists, [&] { backup.backup(source, base / "backup"); });
        require(snapshot(source) == snapshot(base / "backup"), "Duplicate changed contents");
    });
    test("explicit overwrite updates backup and restore", [&] {
        write(source / "root.txt", "new content");
        TransferOptions options;
        options.existingFiles = ExistingFilePolicy::Overwrite;
        backup.backup(source, base / "backup", options);
        restore.restore(base / "backup", base / "restored", options);
        require(snapshot(source) == snapshot(base / "restored"), "Overwrite differs");
    });
    test("filter prunes subtree and progress reports copied files", [&] {
        TransferOptions options;
        std::size_t calls = 0;
        options.filter = [](const fs::path& path, bool) { return path != "level1"; };
        options.progress = [&](const fs::path& path, const TransferResult& result) {
            ++calls;
            require(fs::is_regular_file(base / "filtered" / path), "Progress before copy");
            require(calls == result.filesCopied, "Progress count differs");
        };
        auto result = backup.backup(source, base / "filtered", options);
        require(result.filesCopied == 2 && result.entriesSkipped == 1 && calls == 2, "Filter statistics differ");
        require(!fs::exists(base / "filtered/level1"), "Excluded subtree copied");
    });
    test("missing source, file source and empty destination", [&] {
        expectError(ErrorCode::InvalidPath, [&] { backup.backup(base / "missing", base / "unused"); });
        expectError(ErrorCode::InvalidPath, [&] { restore.restore(source / "root.txt", base / "unused"); });
        expectError(ErrorCode::InvalidPath, [&] { backup.backup(source, {}); });
        require(!fs::exists(base / "unused"), "Invalid operation created destination");
    });
    test("same path, child destination and ancestor destination", [&] {
        expectError(ErrorCode::PathConflict, [&] { backup.backup(source, source); });
        expectError(ErrorCode::PathConflict, [&] { backup.backup(source, source / "child"); });
        expectError(ErrorCode::PathConflict, [&] { restore.restore(source, base); });
        require(!fs::exists(source / "child"), "Conflict created child");
    });
    test("destination is file and nested type conflict", [&] {
        write(base / "file-target", "original");
        expectError(ErrorCode::InvalidPath, [&] { backup.backup(source, base / "file-target"); });
        fs::create_directories(base / "type-conflict/root.txt");
        expectError(ErrorCode::PathConflict, [&] { backup.backup(source, base / "type-conflict"); });
        require(read(base / "file-target") == "original", "Conflict overwrote file");
    });
    test("FileUtils traversal, directory creation and copying", [&] {
        std::size_t entries = 0;
        FileUtils::walkDirectory(source, [&](const fs::path&, bool) { ++entries; return true; });
        require(entries == snapshot(source).size(), "Traversal missed entries");
        FileUtils::createDirectories(base / "utils/a/b");
        FileUtils::copyFile(source / "root.txt", base / "utils/a/b/c.txt");
        require(read(base / "utils/a/b/c.txt") == read(source / "root.txt"), "Copy differs");
        expectError(ErrorCode::PathConflict, [&] {
            FileUtils::copyFile(source / "root.txt", source / "root.txt", ExistingFilePolicy::Overwrite);
        });
    });
    test("callback exception propagates", [&] {
        TransferOptions options;
        options.filter = [](const fs::path&, bool) -> bool { throw std::logic_error("filter failure"); };
        try { backup.backup(source, base / "callback", options); }
        catch (const std::logic_error& error) { require(std::string(error.what()) == "filter failure", "Wrong callback error"); return; }
        throw std::runtime_error("Callback exception swallowed");
    });
#ifndef _WIN32
    test("symbolic links rejected in source and destination", [&] {
        fs::create_directory(base / "link-source");
        fs::create_symlink(source / "root.txt", base / "link-source/link");
        expectError(ErrorCode::UnsupportedEntry, [&] { backup.backup(base / "link-source", base / "link-backup"); });
        fs::create_directory_symlink(base / "restored", base / "destination-link");
        expectError(ErrorCode::PathConflict, [&] { backup.backup(source, base / "destination-link/child"); });
        fs::create_directories(base / "nested-link");
        fs::create_symlink(source / "root.txt", base / "nested-link/root.txt");
        expectError(ErrorCode::PathConflict, [&] { backup.backup(source, base / "nested-link"); });
    });
    test("hard link self overwrite rejected", [&] {
        fs::create_hard_link(source / "root.txt", base / "hard-link");
        expectError(ErrorCode::PathConflict, [&] {
            FileUtils::copyFile(source / "root.txt", base / "hard-link", ExistingFilePolicy::Overwrite);
        });
    });
    test("permission errors in source and destination", [&] {
        auto blocked = base / "blocked";
        fs::create_directory(blocked);
        fs::permissions(blocked, fs::perms::none);
        std::error_code ec;
        fs::directory_iterator probe(blocked, ec);
        if (!ec) {
            fs::permissions(blocked, fs::perms::owner_all);
            ++skipped;
            std::cout << "[SKIP] permission assertion: current account bypasses permission bits\n";
            return;
        }
        try {
            expectError(ErrorCode::PermissionDenied, [&] { backup.backup(blocked, base / "permission-backup"); });
            expectError(ErrorCode::PermissionDenied, [&] { backup.backup(source, blocked / "output"); });
        } catch (...) { fs::permissions(blocked, fs::perms::owner_all); throw; }
        fs::permissions(blocked, fs::perms::owner_all);
    });
#endif
    std::cout << "Results: " << passed << " passed, " << failed << " failed, " << skipped << " skipped assertions\n";
    return failed == 0 ? 0 : 1;
}
