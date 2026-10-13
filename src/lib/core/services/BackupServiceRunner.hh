#pragma once

#include "BackupService.hh"
#include "RequestQueue.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/runtime/Thread.hh"

namespace anchor {

class BackupServiceRunner : public Thread {
   public:
    explicit BackupServiceRunner(
        BackupService& backupService, RequestReceiver receiver
    );

    void stop();

   private:
    void run() override;

    void acceptRequest(Request request);

    std::atomic_bool m_running{true};
    BackupService& m_service;
    RequestReceiver m_receiver;
};

}  // namespace anchor
