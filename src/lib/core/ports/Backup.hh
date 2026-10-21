#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Functional.hh"
#include "core/domain/Backup.hh"

namespace anchor {

class BackupPlanRepo : public NonCopyable, public NonMovable {
   protected:
    using Callback = MoveOnlyFunction<void(const BackupPlan&)>;

   public:
    virtual ~BackupPlanRepo() = default;

    virtual Result<std::vector<Path>> list() const = 0;
    virtual void forEach(Callback c) const = 0;
};

class BackupRepo : public NonCopyable, public NonMovable {
   protected:
    using Callback = MoveOnlyFunction<void(const Backup&)>;

   public:
    virtual ~BackupRepo() = default;

    virtual Opt<Backup> get(const Str& backupName) const = 0;
    virtual std::vector<Backup> getAll() const = 0;

    virtual void put(const Backup& backup) = 0;
    virtual void forEach(Callback c) const = 0;

    virtual Opt<BackupRunStatus> getRunStatus(const Str& backupName) const = 0;
    virtual void updateRunStatus(
        const Str& backupName, const BackupRunStatus& status
    ) = 0;
    virtual void clearRunStatus(const Str& backupName) = 0;
};

}  // namespace anchor
