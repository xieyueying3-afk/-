#pragma once
#include "core/FileUtils.h"

namespace core {
class BackupManager {
public:
    TransferResult backup(const fs::path& source, const fs::path& backupDirectory,
                          const TransferOptions& options = {}) const;
};
}
