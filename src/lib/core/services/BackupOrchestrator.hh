#pragma once

#include "BackupRun.hh"
#include "base/Concepts.hh"
#include "base/Config.hh"
#include "base/Core.hh"
#include "base/runtime/Thread.hh"
#include "base/runtime/ThreadSafeQueue.hh"
#include "core/ports/Ports.hh"

namespace anchor {

class BackupOrchestrator : public Thread {
   public:
    explicit BackupOrchestrator(BackupRepo& repo, Executor& executor);

    Result<Uuid> enqueueBackup(BackupRunArgs&& args);

    void stop();

   private:
    void checkFinishedRuns();

    void run() override;
    void trigger(std::unique_ptr<BackupRun> run);

    BackupRepo& m_repo;
    Executor& m_executor;
    ThreadSafeQueue<std::unique_ptr<BackupRun>> m_queue;
};

}  // namespace anchor
