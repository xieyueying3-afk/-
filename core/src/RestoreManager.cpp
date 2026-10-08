#include "core/RestoreManager.h"

namespace core {
TransferResult RestoreManager::restore(const fs::path& source, const fs::path& target,
                                       const TransferOptions& options) const {
    return FileUtils::copyDirectory(source, target, options);
}
}
