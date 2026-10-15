
add_library(anchor STATIC
    # -- base
    "base/Core.cpp"
    "base/Error.cpp"
    "base/FileSystem.cpp"
    "base/Log.cpp"
    "base/Profiler.cpp"
    "base/Random.cpp"
    "base/String.cpp"
    "base/Id.cpp"
    "base/Config.cpp"
    "base/Handle.cpp"
    "base/Json.cpp"
    "base/Yaml.cpp"
    "base/runtime/Thread.cpp"
    "base/runtime/SignalHandler.cpp"
    # -- proto
    "proto/Message.cpp"
    "proto/AckResponse.cpp"
    "proto/DiagnosticRequest.cpp"
    "proto/ErrorResponse.cpp"
    "proto/TriggerBackupRequest.cpp"
    # -- adapters
    "adapters/asio/AsioServer.cpp"
    "adapters/asio/AsioSession.cpp"
    "adapters/asio/AsioController.cpp"
    "adapters/backup/DirectorySnapshotter.cpp"
    "adapters/backup/InMemoryBackupRunRepo.cpp"
    "adapters/backup/OnDiskBackupPlanRepo.cpp"
    "adapters/backup/OnDiskSnapshotStore.cpp"
    "adapters/backup/ArchiveSnapshotter.cpp"
    # -- core
    "core/services/BackupService.cpp"
    "core/services/BackupServiceRunner.cpp"
    "core/services/BackupRun.cpp"
    # -- domain
    "core/domain/Backup.cpp"
    "core/domain/Location.cpp"
    "core/domain/Source.cpp"
)

target_include_directories(anchor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(spdlog CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(LibArchive CONFIG REQUIRED)

target_link_libraries(anchor
    PUBLIC
        fmt::fmt
        spdlog::spdlog
        stduuid::stduuid
        nlohmann_json::nlohmann_json
        asio::asio
        yaml-cpp::yaml-cpp
        LibArchive::LibArchive
)

if (ANCHOR_ENABLE_PROFILING)
    target_compile_definitions(anchor PRIVATE ANCHOR_ENABLE_PROFILING=1)
endif()
