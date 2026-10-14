#pragma once

#include "base/Concepts.hh"
#include "base/Config.hh"
#include "base/Core.hh"
#include "core/domain/Backup.hh"
#include "core/ports/Ports.hh"

namespace anchor {

class BackupService : public NonCopyable, public NonMovable {
   public:
    explicit BackupService(
        const Config& cfg, const FileSystem& fs, Ports ports
    );

    void triggerBackup(const Str& name);
    void tick();

   private:
    void readPlans();
    void scanBackupRuns();

    Config m_cfg;
    const FileSystem& m_fs;
    Ports m_ports;

    // std::unordered_map<Str, std::shared_ptr<BackupJobStatus>> m_activeJobs;
    std::unordered_map<Str, Backup> m_backups;
};

}  // namespace anchor
