#pragma once

#include "BackupOrchestrator.hh"
#include "BackupRun.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/runtime/Thread.hh"
#include "core/domain/Backup.hh"

namespace anchor {

class BackupScheduler : public Thread {
   public:
    explicit BackupScheduler(
        BackupPreflighter& preflighter, BackupOrchestrator& orchestrator
    );

    void stop();

    void add(const Backup& backup);

   private:
    void run();

    BackupPreflighter& m_preflighter;
    BackupOrchestrator& m_orchestrator;
};

}  // namespace anchor
