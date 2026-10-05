#include "DiskBackupPlanRepository.hh"

namespace anchor {

DiskBackupPlanRepository::DiskBackupPlanRepository(
    const Path& plansPath, const FileSystem& fs
)
    : m_plansPath(plansPath), m_fs(fs) {}

Result<void> DiskBackupPlanRepository::valid() const {
    if (not m_fs.isDirectory(m_plansPath)) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Plans path is not a directory"
        );
    }
    return {};
}

std::vector<Str> DiskBackupPlanRepository::list() const {}

std::vector<BackupPlan> DiskBackupPlanRepository::getAll() const {}

}  // namespace anchor
