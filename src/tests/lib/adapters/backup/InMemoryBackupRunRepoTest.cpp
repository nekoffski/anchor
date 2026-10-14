#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRunRepo.hh"
#include "base/FlatMap.hh"
#include "base/InterfaceMap.hh"

namespace anchor::test {
using namespace testing;

TEST(InMemoryBackupRunRepo, StoresReplacesAndReturnsIndependentCopies) {
    InMemoryBackupRunRepo repo;
    EXPECT_FALSE(repo.has("daily"));
    EXPECT_FALSE(repo.get("daily"));
    repo.put("daily", {.state = BackupRunState::starting});
    EXPECT_TRUE(repo.has("daily"));
    auto copy = repo.get("daily");
    ASSERT_TRUE(copy);
    copy->state = BackupRunState::completed;
    EXPECT_EQ(repo.get("daily")->state, BackupRunState::starting);
    repo.put(
        "daily", {.state = BackupRunState::failed,
                  .error = Error{ErrorCode::fileSystemError, "disk full"}}
    );
    auto replaced = repo.get("daily");
    ASSERT_TRUE(replaced->error);
    EXPECT_EQ(replaced->state, BackupRunState::failed);
    EXPECT_EQ(replaced->error->message(), "disk full");
}

TEST(InMemoryBackupRunRepo, IteratesAndRemovesIndividualAndBatchedRuns) {
    InMemoryBackupRunRepo repo;
    repo.put("first", {.state = BackupRunState::starting});
    repo.put("second", {.state = BackupRunState::completed});
    std::vector<Str> names;
    repo.forEach([&](const Str& name, const BackupRunStatus&) {
        names.push_back(name);
    });
    EXPECT_THAT(names, UnorderedElementsAre("first", "second"));
    repo.remove(Str{"missing"});
    repo.remove(Str{"first"});
    EXPECT_FALSE(repo.has("first"));
    const std::vector<Str> batch{"second", "missing"};
    repo.remove(batch);
    EXPECT_FALSE(repo.has("second"));
    repo.forEach([](const Str&, const BackupRunStatus&) {
        FAIL() << "repo should be empty";
    });
}

}  // namespace anchor::test
