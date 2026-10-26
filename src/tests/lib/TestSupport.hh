#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

#include "base/FileSystem.hh"
#include "core/ports/Backup.hh"
#include "core/ports/Executor.hh"
#include "core/ports/Snapshot.hh"

namespace anchor::test {

class TemporaryDirectory : public testing::Test {
   protected:
    void SetUp() override {
        auto pattern =
            (std::filesystem::temp_directory_path() / "anchor-test-XXXXXX")
                .string();
        auto* created = mkdtemp(pattern.data());
        ASSERT_NE(created, nullptr);
        root = created;
    }
    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(root.str(), error);
        EXPECT_FALSE(error) << error.message();
    }
    Path path(const Str& name) const { return Path::join(root, name); }
    Path root;
    FileSystem fs;
};

class ScopedEnv {
   public:
    ScopedEnv(const Str& name, Opt<Str> value = {}) : name(name) {
        if (const auto* previous = std::getenv(name.c_str())) {
            old = previous;
        }
        if (value) {
            EXPECT_EQ(setenv(name.c_str(), value->c_str(), 1), 0);
        } else {
            EXPECT_EQ(unsetenv(name.c_str()), 0);
        }
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
    ~ScopedEnv() {
        if (old) {
            EXPECT_EQ(setenv(name.c_str(), old->c_str(), 1), 0);
        } else {
            EXPECT_EQ(unsetenv(name.c_str()), 0);
        }
    }

   private:
    Str name;
    Opt<Str> old;
};

class MockFileSystem : public FileSystem {
   public:
    MOCK_METHOD(bool, exists, (const Path&), (const, override));
    MOCK_METHOD(bool, isFile, (const Path&), (const, override));
    MOCK_METHOD(std::vector<Path>, glob, (const Str&), (const, override));
    MOCK_METHOD(Result<Str>, readFile, (const Path&), (const, override));
    MOCK_METHOD(bool, isDirectory, (const Path&), (const, override));
    MOCK_METHOD(
        Result<std::vector<Path>>, listFiles, (const Path&), (const, override)
    );
    MOCK_METHOD(
        Result<void>, createDirectory, (const Path&), (const, override)
    );
    MOCK_METHOD(
        Result<void>, copyRecursive, (const Path&, const Path&),
        (const, override)
    );
    MOCK_METHOD(
        Result<void>, copyFile, (const Path&, const Path&), (const, override)
    );
    MOCK_METHOD(
        Result<void>, copyDirectoryRecursive, (const Path&, const Path&),
        (const, override)
    );
    MOCK_METHOD(Result<void>, removeAll, (const Path&), (const, override));
};

class MockSnapshotter : public Snapshotter {
   public:
    MOCK_METHOD(
        Result<SnapshotMetadata>, snapshot, (const SnapshotArgs&), (override)
    );
};

class MockSnapshotStore : public SnapshotStore {
   public:
    MOCK_METHOD(
        Result<SnapshotMetadata>, put,
        (const SnapshotMetadata&, const Location&), (override)
    );
};

class MockPlanRepo : public BackupPlanRepo {
   public:
    MOCK_METHOD(Result<std::vector<Path>>, list, (), (const, override));
    MOCK_METHOD(Result<void>, forEach, (Callback), (const, override));
};

class MockBackupRepo : public BackupRepo {
   public:
    MOCK_METHOD(Opt<Backup>, get, (const Str&), (const, override));
    MOCK_METHOD(std::vector<Backup>, getAll, (), (const, override));
    MOCK_METHOD(void, put, (const Backup&), (override));
    MOCK_METHOD(void, putPlan, (const BackupPlan&), (override));
    MOCK_METHOD(void, forEach, (Callback), (const, override));
    MOCK_METHOD(
        Opt<BackupRunStatus>, getRunStatus, (const Str&), (const, override)
    );
    MOCK_METHOD(
        void, updateRunStatus, (const Str&, const BackupRunStatus&), (override)
    );
    MOCK_METHOD(void, clearRunStatus, (const Str&), (override));
};

class MockExecutor : public Executor {
   public:
    MOCK_METHOD(void, call, (MoveOnlyFunction<void()>), (override));
};

inline BackupPlan plan() {
    return {
        .name = "daily",
        .archiveType = ArchiveType::directory,
        .destination = Location::local("/backups"),
        .sources = {Source{.root = "/source", .includes = {"file.txt"}}}
    };
}

}  // namespace anchor::test
