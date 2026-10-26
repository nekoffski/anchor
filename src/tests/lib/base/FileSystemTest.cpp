#include <array>

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
    path.append(".yaml");
    EXPECT_TRUE(path.endsWith(".yaml"));
    EXPECT_TRUE(path.endsWith(""));
    EXPECT_FALSE(path.endsWith(".CONF"));
    EXPECT_FALSE(path.endsWith("longer-than-file.yaml"));
    EXPECT_EQ((path + ".bak").str(), "file.yaml.bak");
    EXPECT_EQ(path.str(), "file.yaml");
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

TEST(Path, NormalizesAndResolvesPathsWithoutRequiringThemToExist) {
    EXPECT_EQ(Path{"/base/./child/../file"}.normalized().str(), "/base/file");
    EXPECT_EQ(Path{"one/../two"}.normalized().str(), "two");
    const auto relative =
        Path{"/base/child/../file"}.lexicallyRelative("/base").normalized();
    EXPECT_EQ(relative.str(), "file");
    EXPECT_TRUE(Path{"/base"}.isAbsolute());
    EXPECT_FALSE(Path{"relative"}.isAbsolute());
    auto resolved = Path{"one/../two"}.absolute();
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->str(), Path::join(Path::cwd(), "two").str());
}

using FileSystemTest = TemporaryDirectory;

TEST_F(FileSystemTest, CreatesDirectoriesAndDistinguishesFilesFromDirectories) {
    const auto directory = path("nested/child");
    auto created = fs.createDirectory(directory);
    ASSERT_TRUE(created);
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

TEST_F(FileSystemTest, CopyRecursiveCopiesFilesAndNestedDirectories) {
    ASSERT_TRUE(fs.createDirectory(path("source/nested")));
    ASSERT_TRUE(fs.createDirectory(path("source/empty")));
    ASSERT_TRUE(fs.writeFile(path("source/nested/file"), "content"));
    ASSERT_TRUE(
        fs.copyRecursive(path("source/nested/file"), path("file-copy"))
    );
    auto file = fs.readFile(path("file-copy"));
    ASSERT_TRUE(file);
    EXPECT_EQ(*file, "content");

    ASSERT_TRUE(fs.copyRecursive(path("source"), path("directory-copy")));
    auto nested = fs.readFile(path("directory-copy/nested/file"));
    ASSERT_TRUE(nested);
    EXPECT_EQ(*nested, "content");
    EXPECT_TRUE(fs.isDirectory(path("directory-copy/empty")));
    EXPECT_TRUE(fs.exists(path("source/nested/file")));
}

TEST_F(FileSystemTest, CopyRecursiveRejectsMissingSourcesAndPreservesTargets) {
    auto missing = fs.copyRecursive(path("absent"), path("target"));
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), ErrorCode::fileSystemError);
    EXPECT_THAT(missing.error().message(), HasSubstr(path("absent").str()));
    EXPECT_FALSE(fs.exists(path("target")));

    ASSERT_TRUE(fs.writeFile(path("target"), "preserved"));
    ASSERT_TRUE(fs.writeFile(path("source-file"), "replacement"));
    ASSERT_TRUE(fs.createDirectory(path("source-directory")));
    for (const auto* source : {"source-file", "source-directory"}) {
        SCOPED_TRACE(source);
        auto result = fs.copyRecursive(path(source), path("target"));
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), ErrorCode::fileSystemError);
        auto target = fs.readFile(path("target"));
        ASSERT_TRUE(target);
        EXPECT_EQ(*target, "preserved");
    }
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

TEST_F(
    FileSystemTest, OpenFilesSupportChunkedBinaryIOAndCheckedIdempotentClose
) {
    const auto target = path("stream");
    auto writer = fs.openFile(target, FileMode::createNew);
    ASSERT_TRUE(writer);
    const std::array<Byte, 4> contents{0, 1, 127, 255};
    EXPECT_TRUE((*writer)->write(std::span{contents}.first(2)));
    EXPECT_TRUE((*writer)->write(std::span{contents}.subspan(2)));
    EXPECT_TRUE((*writer)->write({}));
    EXPECT_TRUE((*writer)->close());
    EXPECT_TRUE((*writer)->close());
    EXPECT_FALSE((*writer)->write(contents));
    auto reader = fs.openFile(target, FileMode::read);
    ASSERT_TRUE(reader);
    std::array<Byte, 3> buffer{};
    auto first = (*reader)->read(buffer);
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, 3);
    EXPECT_THAT(buffer, ElementsAre(0, 1, 127));
    auto second = (*reader)->read(buffer);
    ASSERT_TRUE(second);
    EXPECT_EQ(*second, 1);
    EXPECT_EQ(buffer[0], 255);
    auto end = (*reader)->read(buffer);
    ASSERT_TRUE(end);
    EXPECT_EQ(*end, 0);
    EXPECT_FALSE((*reader)->write(contents));
    EXPECT_TRUE((*reader)->close());
    EXPECT_FALSE((*reader)->read(buffer));
    EXPECT_THAT(*fs.readBinary(target), ElementsAre(0, 1, 127, 255));
}

TEST_F(
    FileSystemTest, OpenFilesReleaseOnDestructionAndNeverOverwriteExistingFiles
) {
    const auto target = path("existing");
    {
        auto writer = fs.openFile(target, FileMode::createNew);
        ASSERT_TRUE(writer);
        const std::array<Byte, 2> data{1, 2};
        ASSERT_TRUE((*writer)->write(data));
    }
    EXPECT_FALSE(fs.openFile(target, FileMode::createNew));
    EXPECT_THAT(*fs.readBinary(target), ElementsAre(1, 2));
    EXPECT_FALSE(fs.openFile(path("missing"), FileMode::read));
    EXPECT_FALSE(fs.openFile(path("missing/parent"), FileMode::createNew));
    std::filesystem::create_symlink(target.str(), path("link").str());
    EXPECT_FALSE(fs.openFile(path("link"), FileMode::createNew));
    EXPECT_FALSE(fs.openFile(path("link"), FileMode::read));
}

TEST_F(
    FileSystemTest, MetadataAndListingsPreserveLinksWithoutFollowingTargets
) {
    ASSERT_TRUE(fs.writeFile(path("file"), "data"));
    ASSERT_TRUE(fs.createDirectory(path("directory")));
    std::filesystem::create_symlink("file", path("link").str());
    std::filesystem::create_symlink("missing", path("broken").str());
    auto file = fs.info(path("file"));
    ASSERT_TRUE(file);
    EXPECT_EQ(file->type, FileInfo::Type::regular);
    EXPECT_EQ(file->size, 4);
    auto directory = fs.info(path("directory"));
    ASSERT_TRUE(directory);
    EXPECT_EQ(directory->type, FileInfo::Type::directory);
    auto link = fs.info(path("broken"));
    ASSERT_TRUE(link);
    EXPECT_EQ(link->type, FileInfo::Type::symlink);
    EXPECT_EQ(link->linkTarget, "missing");
    EXPECT_FALSE(fs.info(path("missing")));
    auto entries = fs.listEntries(root);
    ASSERT_TRUE(entries);
    std::vector<Str> names;
    for (const auto& entry : *entries) {
        names.push_back(entry.filenameWithExtension());
    }
    EXPECT_THAT(
        names, UnorderedElementsAre("file", "directory", "link", "broken")
    );
    EXPECT_FALSE(fs.listEntries(path("missing")));
    EXPECT_TRUE(*fs.equivalent(path("file"), path("link")));
    EXPECT_FALSE(*fs.equivalent(path("file"), path("directory")));
    EXPECT_FALSE(fs.equivalent(path("file"), path("missing")));
}

}  // namespace anchor::test
