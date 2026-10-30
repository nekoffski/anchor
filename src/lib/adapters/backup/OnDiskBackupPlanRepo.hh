#pragma once

#include <kcore/io/FileSystem.hh>

#include "core/Core.hh"
#include "core/Error.hh"
#include "core/ports/Backup.hh"

namespace anchor {

class OnDiskBackupPlanRepo : public BackupPlanRepo {
   public:
    explicit OnDiskBackupPlanRepo(
        const kc::Path& plansPath, const kc::FileSystem& fs
    );

    Result<void> valid() const;

    Result<std::vector<kc::Path>> list() const override;
    Result<void> forEach(Callback c) const override;

   private:
    kc::Path m_plansPath;
    const kc::FileSystem& m_fs;
};

}  // namespace anchor
