#pragma once

#include "base/Core.hh"
#include "base/Error.hh"
#include "base/FileSystem.hh"
#include "core/ports/Backup.hh"

namespace anchor {

class OnDiskBackupPlanRepo : public BackupPlanRepo {
   public:
    explicit OnDiskBackupPlanRepo(const Path& plansPath, const FileSystem& fs);

    Result<void> valid() const;

    std::vector<Path> list() const override;
    std::vector<BackupPlan> getAll() const override;

   private:
    Path m_plansPath;
    const FileSystem& m_fs;
};

}  // namespace anchor
