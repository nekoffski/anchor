#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "core/domain/Backup.hh"

namespace anchor {

class BackupPlanRepository : public NonCopyable, public NonMovable {
   public:
    virtual ~BackupPlanRepository() = default;

    virtual std::vector<Path> list() const = 0;
    virtual std::vector<BackupPlan> getAll() const = 0;
};

}  // namespace anchor
