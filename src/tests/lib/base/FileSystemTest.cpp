#include "TestSupport.hh"
#include "base/Env.hh"

namespace anchor::test {
using namespace testing;

TEST(Path, ExtractsNamesParentsAndFormatsPaths) {
    Path path{"/tmp/archive.tar.gz"};
    EXPECT_EQ(path.parent().str(), "/tmp");
    EXPECT_EQ(path.filename(), "archive.tar");
    EXPECT_EQ(path.filenameWithExtension(), "archive.tar.gz");
    EXPECT_EQ(Path{"file"}.parent().str(), "");
    EXPECT_EQ(Path{".hidden"}.filename(), ".hidden");
    EXPECT_EQ(Path::fmt("{}/{}", "tmp", 42).str(), "tmp/42");
    EXPECT_EQ(Path::cwd().str(), std::filesystem::current_path().string());
}

TEST(Path, JoinsPathsAndAppendsSuffixes) {
    EXPECT_EQ(Path::join("/base", "child").str(), "/base/child");
    EXPECT_EQ(Path::join("/base", "/absolute").str(), "/absolute");
    EXPECT_EQ(Path::join("", "child").str(), "child");
    Path path{"file"};
    path.append(".conf");
    EXPECT_TRUE(path.endsWith(".conf"));
    EXPECT_TRUE(path.endsWith(""));
    EXPECT_FALSE(path.endsWith(".CONF"));
    EXPECT_FALSE(path.endsWith("longer-than-file.conf"));
    EXPECT_EQ((path + ".bak").str(), "file.conf.bak");
    EXPECT_EQ(path.str(), "file.conf");
}

TEST(Path, ResolvesEnvironmentVariablesByPathComponent) {
    ScopedEnv base{"ANCHOR_TEST_PATH", "/tmp/base"};
    ScopedEnv missing{"ANCHOR_TEST_MISSING"};
    EXPECT_EQ(
        Path{"$ANCHOR_TEST_PATH/file"}.resolveEnv().str(), "/tmp/base/file"
    );
    EXPECT_EQ(
        Path{"/prefix/$ANCHOR_TEST_MISSING/file"}.resolveEnv().str(),
        "/prefix//file"
    );
    EXPECT_EQ(
        Path{"/prefix/literal$ANCHOR_TEST_PATH"}.resolveEnv().str(),
        "/prefix/literal$ANCHOR_TEST_PATH"
    );
}

using FileSystemTest = TemporaryDirectory;

TEST_F(FileSystemTest, CreatesDirectoriesAndDistinguishesFilesFromDirectories) {
    const auto directory = path("nested/child");
    auto created = fs.createDirectory(directory);
    ASSERT_TRUE(created);
    EXPECT_EQ(created->str(), directory.str());
    EXPECT_TRUE(fs.createDirectory(directory));
    EXPECT_TRUE(fs.exists(directory));
    EXPECT_TRUE(fs.isDirectory(directory));
    EXPECT_FALSE(fs.isFile(directory));
    const auto file = path("file");
    ASSERT_TRUE(fs.touch(file));
    EXPECT_TRUE(fs.exists(file));
    EXPECT_TRUE(fs.isFile(file));
    EXPECT_FALSE(fs.isDirectory(file));
    EXPECT_FALSE(fs.exists(path("missing")));
    EXPECT_FALSE(fs.isFile(path("missing")));
    EXPECT_FALSE(fs.isDirectory(path("missing")));
    EXPECT_FALSE(fs.createDirectory(Path::join(file, "child")));
}

TEST_F(FileSystemTest, WritesAppendsReadsLinesAndPreservesTouchedContent) {
    const auto file = path("data");
    ASSERT_TRUE(fs.writeFile(file, "first\n"));
    ASSERT_TRUE(fs.appendFile(file, "\nlast"));
    ASSERT_TRUE(fs.touch(file));
    auto content = fs.readFile(file);
    ASSERT_TRUE(content);
    EXPECT_EQ(*content, "first\n\nlast");
    auto lines = fs.readLines(file);
    ASSERT_TRUE(lines);
    EXPECT_THAT(*lines, ElementsAre("first", "", "last"));
    ASSERT_TRUE(fs.writeFile(file, "replacement"));
    EXPECT_EQ(*fs.readFile(file), "replacement");
    ASSERT_TRUE(fs.writeFile(file, ""));
    EXPECT_TRUE(fs.readFile(file)->empty());
    EXPECT_TRUE(fs.readLines(file)->empty());
}

TEST_F(FileSystemTest, ReadsBinaryIncludingNullAndHighBytes) {
    const auto file = path("binary");
    ASSERT_TRUE(fs.writeFile(file, Str{"\x00\x01\x7f\xff", 4}));
    auto bytes = fs.readBinary(file);
    ASSERT_TRUE(bytes);
    EXPECT_THAT(*bytes, ElementsAre(0, 1, 127, 255));
    ASSERT_TRUE(fs.writeFile(file, ""));
    EXPECT_TRUE(fs.readBinary(file)->empty());
}

TEST_F(FileSystemTest, MissingFilesAndParentsReturnErrors) {
    const auto missing = path("missing/file");
    EXPECT_FALSE(fs.readFile(missing));
    EXPECT_FALSE(fs.readLines(missing));
    EXPECT_FALSE(fs.readBinary(missing));
    EXPECT_FALSE(fs.writeFile(missing, "data"));
    EXPECT_FALSE(fs.appendFile(missing, "data"));
    EXPECT_FALSE(fs.touch(missing));
    EXPECT_EQ(fs.readFile(missing).error().code(), ErrorCode::fileSystemError);
}

TEST_F(FileSystemTest, ReadingDirectoriesReturnsErrors) {
    EXPECT_FALSE(fs.readFile(root));
    EXPECT_FALSE(fs.readLines(root));
    EXPECT_FALSE(fs.readBinary(root));
}

TEST_F(FileSystemTest, CopiesFilesWithoutOverwritingAndReportsFailures) {
    const auto source = path("source");
    const auto target = path("target");
    ASSERT_TRUE(fs.writeFile(source, "original"));
    ASSERT_TRUE(fs.copyFile(source, target));
    EXPECT_EQ(*fs.readFile(target), "original");
    ASSERT_TRUE(fs.writeFile(source, "changed"));
    EXPECT_FALSE(fs.copyFile(source, target));
    EXPECT_EQ(*fs.readFile(target), "original");
    EXPECT_FALSE(fs.copyFile(path("absent"), path("new")));
    EXPECT_FALSE(fs.copyFile(source, path("missing/target")));
}

TEST_F(FileSystemTest, CopiesDirectoryTreesAndListsOnlyMatchingEntryTypes) {
    const auto source = path("source");
    ASSERT_TRUE(fs.createDirectory(Path::join(source, "nested")));
    ASSERT_TRUE(fs.writeFile(Path::join(source, "first"), "one"));
    ASSERT_TRUE(fs.writeFile(Path::join(source, "second"), "two"));
    ASSERT_TRUE(fs.writeFile(Path::join(source, "nested/third"), "three"));
    std::vector<Str> files, directories;
    auto listedFiles = fs.listFiles(source);
    auto listedDirectories = fs.listDirectories(source);
    ASSERT_TRUE(listedFiles);
    ASSERT_TRUE(listedDirectories);
    for (const auto& file : *listedFiles) {
        files.push_back(file.filenameWithExtension());
    }
    for (const auto& dir : *listedDirectories) {
        directories.push_back(dir.filenameWithExtension());
    }
    EXPECT_THAT(files, UnorderedElementsAre("first", "second"));
    EXPECT_THAT(directories, ElementsAre("nested"));
    const auto target = path("copy");
    ASSERT_TRUE(fs.copyDirectoryRecursive(source, target));
    EXPECT_EQ(*fs.readFile(Path::join(target, "nested/third")), "three");
    EXPECT_FALSE(fs.copyDirectoryRecursive(path("absent"), path("new")));
}

TEST_F(
    FileSystemTest, RemovingMissingPathsIsIdempotentAndNonemptyDirectoriesFail
) {
    const auto directory = path("directory");
    ASSERT_TRUE(fs.createDirectory(directory));
    const auto file = Path::join(directory, "file");
    ASSERT_TRUE(fs.touch(file));
    EXPECT_FALSE(fs.remove(directory));
    ASSERT_TRUE(fs.remove(file));
    EXPECT_FALSE(fs.exists(file));
    EXPECT_TRUE(fs.remove(file));
    ASSERT_TRUE(fs.touch(file));
    ASSERT_TRUE(fs.removeAll(directory));
    EXPECT_FALSE(fs.exists(directory));
    EXPECT_TRUE(fs.removeAll(directory));
}

TEST_F(FileSystemTest, ListingMissingDirectoriesReturnsErrors) {
    auto files = fs.listFiles(path("absent"));
    auto directories = fs.listDirectories(path("absent"));
    ASSERT_FALSE(files);
    ASSERT_FALSE(directories);
    EXPECT_EQ(files.error().code(), ErrorCode::fileSystemError);
    EXPECT_EQ(directories.error().code(), ErrorCode::fileSystemError);
}

}  // namespace anchor::test
