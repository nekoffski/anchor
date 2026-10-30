find_package(GTest CONFIG REQUIRED)

add_executable(anchor-tests
    "cli/ApplicationTest.cpp"
    "lib/adapters/asio/AsioControllerTest.cpp"
    "lib/adapters/backup/InMemoryBackupRepoTest.cpp"
    "lib/adapters/backup/OnDiskBackupPlanRepoTest.cpp"
    "lib/adapters/backup/SnapshotTest.cpp"
    "lib/adapters/backup/ArchiveSnapshotterTest.cpp"
    "lib/core/ConfigTest.cpp"
    "lib/core/ErrorTest.cpp"
    "lib/core/services/RequestQueueTest.cpp"
    "lib/core/domain/DomainTest.cpp"
    "lib/core/services/BackupRunTest.cpp"
    "lib/core/services/BackupSchedulerTest.cpp"
    "lib/core/services/BackupOrchestratorTest.cpp"
    "lib/core/services/BackupServiceTest.cpp"
    "lib/core/services/SourceTreeBuilderTest.cpp"
    "lib/proto/MessageTest.cpp"
)
target_include_directories(anchor-tests PRIVATE lib)
target_compile_definitions(anchor-tests PRIVATE ANCHOR_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(anchor-tests PRIVATE anchor-cli-commands GTest::gmock_main)
include(GoogleTest)
gtest_discover_tests(anchor-tests PROPERTIES LABELS unit TIMEOUT 10)
