#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "domain/BackupPlan.hh"

namespace anchor {

class BackupPlanRepository : public NonCopyable, public NonMovable {
   public:
    virtual ~BackupPlanRepository() = default;

    virtual std::vector<Str> list() = 0;
    virtual std::vector<BackupPlan> getAll() = 0;
};

}  // namespace anchor
