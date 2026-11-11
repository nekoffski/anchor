add_library(anchor STATIC
    "core/Config.cpp"
    "core/Error.cpp"
    # -- proto
    "proto/Message.cpp"
    "proto/BackupMessages.cpp"
    "proto/SystemMessages.cpp"
    "proto/SnapshotMessages.cpp"
    # -- adapters
    "adapters/asio/AsioServer.cpp"
    "adapters/asio/AsioSession.cpp"
    "adapters/asio/AsioController.cpp"
    "adapters/backup/DirectorySnapshotter.cpp"
    "adapters/backup/InMemoryBackupRepo.cpp"
    "adapters/backup/OnDiskBackupPlanRepo.cpp"
    "adapters/backup/OnDiskSnapshotStore.cpp"
    "adapters/serialization/SnapshotJson.cpp"
    "adapters/backup/ArchiveSnapshotter.cpp"
    "adapters/backup/SnapshotBuilder.cpp"
    "adapters/crypto/Sha256.cpp"
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
find_package(OpenSSL CONFIG REQUIRED)
find_package(croncpp CONFIG REQUIRED)

target_link_libraries(anchor
    PUBLIC
        kcore::kcore
        asio::asio
        LibArchive::LibArchive
        croncpp::croncpp
    PRIVATE
        OpenSSL::Crypto
)
