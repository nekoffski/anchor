#pragma once

#include "base/Concepts.hh"
#include "base/Config.hh"
#include "base/Core.hh"
#include "core/domain/Backup.hh"
#include "core/ports/BackupPlanRepository.hh"

namespace anchor {

class BackupService : public NonCopyable, public NonMovable {
   public:
    explicit BackupService(const Config& cfg, BackupPlanRepository& planRepo);

   private:
    Config m_cfg;
    BackupPlanRepository& m_planRepo;
    std::unordered_map<Str, Backup> m_backups;
};

}  // namespace anchor
