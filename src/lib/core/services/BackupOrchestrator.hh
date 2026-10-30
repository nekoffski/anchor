#pragma once

#include <kcore/Concepts.hh>
#include <kcore/runtime/Thread.hh>
#include <kcore/runtime/ThreadSafeQueue.hh>

#include "BackupRun.hh"
#include "core/Core.hh"
#include "core/Error.hh"
#include "core/ports/Executor.hh"

namespace anchor {

class BackupOrchestrator : public kc::Thread {
   public:
    explicit BackupOrchestrator(BackupRepo& repo, Executor& executor);
    ~BackupOrchestrator() override;

    Result<Uuid> enqueueBackup(BackupRunArgs&& args);

    void stop();

   private:
    void run() override;
    void trigger(std::unique_ptr<BackupRun> run);

    BackupRepo& m_repo;
    Executor& m_executor;
    kc::ThreadSafeQueue<std::unique_ptr<BackupRun>> m_queue;
};

}  // namespace anchor
