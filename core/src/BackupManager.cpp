#include "core/BackupManager.h"

namespace core {
TransferResult BackupManager::backup(const fs::path& source, const fs::path& target,
                                     const TransferOptions& options) const {
    return FileUtils::copyDirectory(source, target, options);
}
}
