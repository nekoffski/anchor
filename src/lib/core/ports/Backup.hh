#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "core/domain/Backup.hh"

namespace anchor {

class BackupPlanRepo : public NonCopyable, public NonMovable {
   public:
    virtual ~BackupPlanRepo() = default;

    virtual std::vector<Path> list() const = 0;
    virtual std::vector<BackupPlan> getAll() const = 0;
};

class BackupRunRepo : public NonCopyable, public NonMovable {
   public:
    virtual ~BackupRunRepo() = default;

    virtual bool has(const Str& backupName) const = 0;
    virtual Opt<BackupRunStatus> get(const Str& backupName) const = 0;
    virtual void put(const Str& backupName, const BackupRunStatus& status) = 0;
    virtual void remove(const Str& backupName) = 0;
};

}  // namespace anchor
