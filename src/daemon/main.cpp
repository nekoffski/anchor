
#include "adapters/asio/AsioServer.hh"
#include "adapters/backup/DirectorySnapshotter.hh"
#include "adapters/backup/InMemoryBackupRunRepo.hh"
#include "adapters/backup/OnDiskBackupPlanRepo.hh"
#include "adapters/backup/OnDiskSnapshotStore.hh"
#include "adapters/executors/InlineExecutor.hh"
#include "base/Functional.hh"
#include "base/Log.hh"
#include "base/Unwrap.hh"
#include "base/runtime/SignalHandler.hh"
#include "core/services/BackupService.hh"
#include "core/services/BackupServiceRunner.hh"
#include "core/services/RequestQueue.hh"

using namespace anchor;

int main(int argc, const char** argv) {
    log::expect(argc == 2, "Usage: {} <config_file>", argv[0]);

    Path configPath{argv[1]};
    auto cfg = unwrap(Config::load(configPath));

    log::init(
        log::LoggerOptions{
            .enableColors = false,
            .level = log::Level::debug,
            .formatPattern = "%^[%d-%m-%Y %T] [Th: %t] %-7l [anchord]: %v %$",
        }
    );
    log::info("daemon starting");
    DEFER { log::info("daemon shutting down, cya!"); };

    FileSystem fs;
    SignalHandler signals;
    RequestQueue requestQueue;

    OnDiskBackupPlanRepo backupPlanRepo{cfg.paths.backupPlans, fs};
    log::expect(backupPlanRepo.valid());

    AsioController controller{RequestSender{requestQueue}};
    AsioServer server{cfg, controller};
    server.start();

    Snapshotters snapshotters;
    snapshotters.add<DirectorySnapshotter>(ArchiveType::directory, fs);

    SnapshotStores snapshotStores;
    snapshotStores.add<OnDiskSnapshotStore>(LocationType::local, fs);

    InMemoryBackupRunRepo backupRunRepo;

    InlineExecutor executor;
    Ports ports{
        backupRunRepo, backupPlanRepo, snapshotters, snapshotStores, executor
    };

    BackupService backupService{cfg, fs, ports};
    BackupServiceRunner backupServiceRunner{
        backupService, RequestReceiver{requestQueue}
    };
    backupServiceRunner.start();

    signals.wait([&](Signal s) {
        log::info("received signal: {}", toString(s));

        if (s == Signal::sigint || s == Signal::sigterm) {
            log::info("received shutdown request");

            server.stop();
            backupServiceRunner.stop();
            requestQueue.close();

            return SignalAction::stop;
        }
        return SignalAction::waitForNext;
    });

    Thread::join(server, backupServiceRunner);
    return 0;
}
