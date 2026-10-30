#pragma once

#include <kcore/Concepts.hh>
#include <kcore/Functional.hh>

#include "core/Core.hh"
#include "core/Error.hh"
#include "core/domain/Backup.hh"

namespace anchor {

class BackupPlanRepo : public kc::NonCopyable, public kc::NonMovable {
   protected:
    using Callback = kc::MoveOnlyFunction<void(const BackupPlan&)>;

   public:
    virtual ~BackupPlanRepo() = default;

    virtual Result<std::vector<kc::Path>> list() const = 0;
    // Visits valid plans, returning an error if any plan could not be loaded.
    virtual Result<void> forEach(Callback c) const = 0;
};

class BackupRepo : public kc::NonCopyable, public kc::NonMovable {
   protected:
    using Callback = kc::MoveOnlyFunction<void(const Backup&)>;

   public:
    virtual ~BackupRepo() = default;

    virtual kc::Opt<Backup> get(const Str& backupName) const = 0;
    virtual std::vector<Backup> getAll() const = 0;

    virtual void put(const Backup& backup) = 0;
    virtual void putPlan(const BackupPlan& plan) = 0;
    virtual void forEach(Callback c) const = 0;

    virtual kc::Opt<BackupRunStatus> getRunStatus(
        const Str& backupName
    ) const = 0;
    virtual void updateRunStatus(
        const Str& backupName, const BackupRunStatus& status
    ) = 0;
    virtual void clearRunStatus(const Str& backupName) = 0;
};

}  // namespace anchor
