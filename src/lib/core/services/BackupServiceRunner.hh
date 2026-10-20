#pragma once

#include "BackupService.hh"
#include "RequestQueue.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/runtime/Thread.hh"

namespace anchor {

class BackupServiceRunner : public Thread {
    class RequestHandler {
       public:
        explicit RequestHandler(BackupService& service);

        Response operator()(const TriggerBackupRequest& request);
        Response operator()(const DiagnosticRequest& request);

       private:
        BackupService& m_service;
    };

   public:
    explicit BackupServiceRunner(
        BackupService& backupService, RequestReceiver receiver
    );

    void stop();

   private:
    void run() override;

    void acceptRequest(RequestWrapper&& request);

    std::atomic_bool m_running{true};
    BackupService& m_service;
    RequestReceiver m_receiver;
    RequestHandler m_requestHandler;
};

}  // namespace anchor
