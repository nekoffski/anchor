
#include <kcore/Functional.hh>
#include <kcore/Log.hh>
#include <kcore/Unwrap.hh>
#include <kcore/runtime/SignalHandler.hh>

#include "adapters/asio/AsioServer.hh"
#include "adapters/backup/ArchiveSnapshotter.hh"
#include "adapters/backup/DirectorySnapshotter.hh"
#include "adapters/backup/InMemoryBackupRepo.hh"
#include "adapters/backup/OnDiskBackupPlanRepo.hh"
#include "adapters/backup/OnDiskSnapshotStore.hh"
#include "adapters/executors/InlineExecutor.hh"
#include "core/services/BackupService.hh"

using namespace anchor;

int main(int argc, const char** argv) {
    kc::log::expect(argc == 2, "Usage: {} <config_file>", argv[0]);

    kc::Path configPath{argv[1]};
    auto cfg = kc::unwrap(Config::load(configPath));

    kc::log::init(
        kc::log::LoggerOptions{
            .enableColors = false,
            .level = kc::log::Level::debug,
            .formatPattern = "%^[%d-%m-%Y %T] [Th: %t] %-7l [anchord]: %v %$",
        }
    );
    kc::log::info("daemon starting");
    KCORE_DEFER { kc::log::info("daemon shutting down, cya!"); };

    kc::FileSystem fs;
    kc::SignalHandler signals;

    OnDiskBackupPlanRepo backupPlanRepo{cfg.paths.backupPlans, fs};
    kc::log::expect(backupPlanRepo.valid());

    Snapshotters snapshotters;
    snapshotters.add<DirectorySnapshotter>(ArchiveType::directory, fs);
    // TODO: we pass the same arg twice, extend interface map so it takes key
    // from value
    snapshotters.add<ArchiveSnapshotter>(
        ArchiveType::zip, fs, ArchiveType::zip
    );
    snapshotters.add<ArchiveSnapshotter>(
        ArchiveType::tgz, fs, ArchiveType::tgz
    );

    SnapshotStores snapshotStores;
    snapshotStores.add<OnDiskSnapshotStore>(LocationType::local, fs);

    InMemoryBackupRepo backupRepo;

    InlineExecutor executor;
    Ports ports{
        backupRepo, backupPlanRepo, snapshotters, snapshotStores, executor
    };

    BackupService backupService{cfg, fs, ports};
    backupService.start();

    AsioController controller{backupService.sender()};
    AsioServer server{cfg, controller};
    server.start();

    signals.wait([&](kc::Signal s) {
        kc::log::info("received signal: {}", toString(s));

        if (s == kc::Signal::sigint || s == kc::Signal::sigterm) {
            kc::log::info("received shutdown request");

            server.stop();
            backupService.stop();

            return kc::SignalAction::stop;
        }
        return kc::SignalAction::waitForNext;
    });

    kc::Thread::join(server, backupService);
    return 0;
}
