add_library(anchor STATIC
    "core/Config.cpp"
    "core/Error.cpp"
    # -- proto
    "proto/Message.cpp"
    "proto/AckResponse.cpp"
    "proto/DiagnosticRequest.cpp"
    "proto/ErrorResponse.cpp"
    "proto/TriggerBackupRequest.cpp"
    "proto/GetBackupsRequest.cpp"
    "proto/GetBackupsResponse.cpp"
    "proto/ReloadPlansRequest.cpp"
    # -- adapters
    "adapters/asio/AsioServer.cpp"
    "adapters/asio/AsioSession.cpp"
    "adapters/asio/AsioController.cpp"
    "adapters/backup/DirectorySnapshotter.cpp"
    "adapters/backup/InMemoryBackupRepo.cpp"
    "adapters/backup/OnDiskBackupPlanRepo.cpp"
    "adapters/backup/OnDiskSnapshotStore.cpp"
    "adapters/backup/ArchiveSnapshotter.cpp"
    # -- core
    "core/services/BackupService.cpp"
    "core/services/BackupScheduler.cpp"
    "core/services/BackupOrchestrator.cpp"
    "core/services/BackupRun.cpp"
    "core/services/SourceTreeBuilder.cpp"
    "core/services/RequestQueue.cpp"
    # -- domain
    "core/domain/Backup.cpp"
    "core/domain/Location.cpp"
    "core/domain/Source.cpp"
    "core/domain/Cron.cpp"
)

target_include_directories(anchor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(asio CONFIG REQUIRED)
find_package(LibArchive CONFIG REQUIRED)
find_package(croncpp CONFIG REQUIRED)

target_link_libraries(anchor
    PUBLIC
        kcore::kcore
        asio::asio
        LibArchive::LibArchive
        croncpp::croncpp
)
