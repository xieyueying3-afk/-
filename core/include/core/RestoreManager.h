#pragma once
#include "core/FileUtils.h"

namespace core {
class RestoreManager {
public:
    TransferResult restore(const fs::path& backupDirectory, const fs::path& destination,
                           const TransferOptions& options = {}) const;
};
}
