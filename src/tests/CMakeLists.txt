find_package(GTest CONFIG REQUIRED)

add_executable(anchor-tests
    "lib/adapters/asio/AsioControllerTest.cpp"
    "lib/adapters/backup/InMemoryBackupRepoTest.cpp"
    "lib/adapters/backup/OnDiskBackupPlanRepoTest.cpp"
    "lib/adapters/backup/SnapshotTest.cpp"
    "lib/base/ConfigTest.cpp"
    "lib/base/ContainersTest.cpp"
    "lib/base/CoreTest.cpp"
    "lib/base/EnumTest.cpp"
    "lib/base/EnvTest.cpp"
    "lib/base/ErrorTest.cpp"
    "lib/base/FileSystemTest.cpp"
    "lib/base/FunctionalTest.cpp"
    "lib/base/HandleTest.cpp"
    "lib/base/IdTest.cpp"
    "lib/base/JsonTest.cpp"
    "lib/base/LogTest.cpp"
    "lib/base/ProfilerTest.cpp"
    "lib/base/RandomTest.cpp"
    "lib/base/RefTest.cpp"
    "lib/base/SingletonTest.cpp"
    "lib/base/StringTest.cpp"
    "lib/base/SyncTest.cpp"
    "lib/base/TimeTest.cpp"
    "lib/base/UnwrapTest.cpp"
    "lib/base/YamlTest.cpp"
    "lib/base/runtime/RuntimeTest.cpp"
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
target_link_libraries(anchor-tests PRIVATE anchor GTest::gmock_main)
include(GoogleTest)
gtest_discover_tests(anchor-tests PROPERTIES LABELS unit TIMEOUT 10)
