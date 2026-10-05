#pragma once

#include "base/Core.hh"
#include "base/Error.hh"
#include "base/FileSystem.hh"
#include "core/ports/BackupPlanRepository.hh"

namespace anchor {

class DiskBackupPlanRepository : public BackupPlanRepository {
   public:
    explicit DiskBackupPlanRepository(
        const Path& plansPath, const FileSystem& fs
    );

    Result<void> valid() const;

    std::vector<Str> list() const override;
    std::vector<BackupPlan> getAll() const override;

   private:
    Path m_plansPath;
    const FileSystem& m_fs;
};

}  // namespace anchor
