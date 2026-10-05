#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "core/domain/BackupPlan.hh"

namespace anchor {

class BackupPlanRepository : public NonCopyable, public NonMovable {
   public:
    virtual ~BackupPlanRepository() = default;

    virtual std::vector<Str> list() const = 0;
    virtual std::vector<BackupPlan> getAll() const = 0;
};

}  // namespace anchor
