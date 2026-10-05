
#include "adapters/asio/AsioServer.hh"
#include "adapters/disk/DiskBackupPlanRepository.hh"
#include "base/Functional.hh"
#include "base/Log.hh"
#include "base/Unwrap.hh"
#include "base/runtime/SignalHandler.hh"
#include "core/services/BackupOrchestrator.hh"
#include "core/services/RequestQueue.hh"

using namespace anchor;

int main(int argc, const char** argv) {
    log::expect(argc == 2, "Usage: {} <config_file>", argv[0]);

    Path configPath{argv[1]};
    auto cfg = unwrap(Config::load(configPath));

    log::init(
        log::LoggerOptions{
            .formatPattern = "%^[%d-%m-%Y %T] [Th: %t] %-7l [anchord]: %v %$"
        }
    );
    log::info("daemon starting");
    DEFER { log::info("daemon shutting down, cya!"); };

    FileSystem fs;
    SignalHandler signals;

    RequestQueue requestQueue;

    DiskBackupPlanRepository backupPlanRepository{cfg.paths.backupPlans, fs};
    log::expect(backupPlanRepository.valid());

    AsioController controller{RequestSender{requestQueue}};
    AsioServer server{cfg, controller};
    server.start();

    BackupOrchestrator orchestrator{cfg, backupPlanRepository};
    BackupOrchestratorRunner orchestratorRunner{
        orchestrator, RequestReceiver{requestQueue}
    };
    orchestratorRunner.start();

    signals.wait([&](Signal s) {
        log::info("received signal: {}", toString(s));

        if (s == Signal::sigint || s == Signal::sigterm) {
            log::info("received shutdown request");

            server.stop();
            orchestratorRunner.stop();
            requestQueue.close();

            return SignalAction::stop;
        }
        return SignalAction::waitForNext;
    });

    Thread::join(server, orchestratorRunner);
    return 0;
}
