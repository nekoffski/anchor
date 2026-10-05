#pragma once

#include "RequestQueue.hh"
#include "base/Concepts.hh"
#include "base/Config.hh"
#include "base/Core.hh"
#include "base/runtime/Thread.hh"
#include "core/ports/BackupPlanRepository.hh"

namespace anchor {

class BackupOrchestrator : public NonCopyable, public NonMovable {
   public:
    explicit BackupOrchestrator(
        const Config& cfg, BackupPlanRepository& planRepo
    );

    void acceptRequest(Request request);

   private:
    Config m_cfg;
    BackupPlanRepository& m_planRepo;
};

class BackupOrchestratorRunner : public Thread {
   public:
    explicit BackupOrchestratorRunner(
        BackupOrchestrator& orchestrator, RequestReceiver receiver
    );

    void stop();

   private:
    void run() override;

    std::atomic_bool m_running{true};
    BackupOrchestrator& m_orchestrator;
    RequestReceiver m_receiver;
};

}  // namespace anchor
