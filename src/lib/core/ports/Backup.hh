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

class BackupRunRepo : public NonCopyable, public NonMovable {
   protected:
    using Callback = MoveOnlyFunction<void(const Str&, const BackupRunStatus&)>;

   public:
    virtual ~BackupRunRepo() = default;

    virtual bool has(const Str& backupName) const = 0;
    virtual Opt<BackupRunStatus> get(const Str& backupName) const = 0;
    virtual void put(const Str& backupName, const BackupRunStatus& status) = 0;
    virtual void remove(const Str& backupName) = 0;
    virtual void remove(std::span<const Str>) = 0;
    virtual void forEach(Callback c) const = 0;
};

}  // namespace anchor
