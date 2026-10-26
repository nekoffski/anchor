#include <archive.h>
#include <archive_entry.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <array>
#include <csignal>
#include <map>

#include "TestSupport.hh"
#include "adapters/backup/ArchiveSnapshotter.hh"
#include "core/services/SourceTreeBuilder.hh"

namespace anchor::test {
namespace {

struct ArchivedEntry {
    int type;
    int permissions;
    std::time_t mtime;
    Str target;
    Str contents;
};

struct ArchiveContents {
    std::map<Str, ArchivedEntry> entries;
    int format;
    int filter;
};

Result<ArchiveContents> readArchive(const Path& path) {
    const auto reader = std::unique_ptr<archive, decltype(&archive_read_free)>{
        archive_read_new(), &archive_read_free
    };
    archive_read_support_format_all(reader.get());
    archive_read_support_filter_all(reader.get());
    if (archive_read_open_filename(reader.get(), path.str().c_str(), 8192) !=
        ARCHIVE_OK) {
        return Error::unexpected(
            ErrorCode::archiverFailure, "{}", archive_error_string(reader.get())
        );
    }
    ArchiveContents result{};
    archive_entry* entry;
    int status;
    while ((status = archive_read_next_header(reader.get(), &entry)) ==
           ARCHIVE_OK) {
        auto name = Str{archive_entry_pathname(entry)};
        if (name.ends_with('/')) {
            name.pop_back();
        }
        ArchivedEntry item{
            .type = static_cast<int>(archive_entry_filetype(entry)),
            .permissions = archive_entry_perm(entry),
            .mtime = archive_entry_mtime(entry),
            .target =
                archive_entry_symlink(entry) ? archive_entry_symlink(entry) : ""
        };
        std::array<char, 8192> buffer;
        la_ssize_t count;
        while ((count = archive_read_data(
                    reader.get(), buffer.data(), buffer.size()
                )) > 0) {
            item.contents.append(buffer.data(), static_cast<size_t>(count));
        }
        if (count < 0) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "{}",
                archive_error_string(reader.get())
            );
        }
        if (not result.entries.emplace(name, std::move(item)).second) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Duplicate archive entry: {}", name
            );
        }
    }
    result.format = archive_format(reader.get()) & ARCHIVE_FORMAT_BASE_MASK;
    result.filter = archive_filter_code(reader.get(), 0);
    if (status != ARCHIVE_EOF ||
        archive_read_close(reader.get()) != ARCHIVE_OK) {
        return Error::unexpected(
            ErrorCode::archiverFailure, "{}", archive_error_string(reader.get())
        );
    }
    return result;
}

}  // namespace

class ArchiveSnapshotterTest : public TemporaryDirectory,
                               public testing::WithParamInterface<ArchiveType> {
};

TEST_P(ArchiveSnapshotterTest, IncludesContentsOfSelectedDirectory) {
    ASSERT_TRUE(fs.createDirectory(path("source/assets/nested")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ASSERT_TRUE(fs.writeFile(
        path("source/assets/nested/data.txt"), "directory contents"
    ));
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"assets"}});
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "daily";
    const auto workspace = path("workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_TRUE(snapshot) << snapshot.error().message();
    auto contents = readArchive(snapshot->location.path);
    ASSERT_TRUE(contents) << contents.error().message();
    ASSERT_TRUE(contents->entries.contains("assets/nested/data.txt"));
    EXPECT_EQ(
        contents->entries.at("assets/nested/data.txt").contents,
        "directory contents"
    );
}

TEST_P(
    ArchiveSnapshotterTest, ReportsEntryReadFailureInsteadOfReturningSuccess
) {
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    const SourceTree tree{
        .root = path("source"),
        .nodes = {
            {"missing",
             {.type = SourceTree::NodeType::file,
              .relativePath = "missing",
              .name = "missing"}}
        }
    };
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "daily";
    const auto workspace = path("workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_EQ(snapshot.error().code(), ErrorCode::archiverFailure);
    EXPECT_THAT(snapshot.error().message(), testing::HasSubstr("missing"));
}

TEST_P(
    ArchiveSnapshotterTest,
    PreservesSelectionsBinaryContentsEmptyFilesAndMetadata
) {
    ASSERT_TRUE(fs.createDirectory(path("source/docs")));
    ASSERT_TRUE(fs.createDirectory(path("source/empty-directory")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    Str binary(192 * 1024 + 17, '\0');
    for (size_t index = 0; index < binary.size(); ++index) {
        binary[index] = static_cast<char>(index % 251);
    }
    ASSERT_TRUE(fs.writeFile(path("source/docs/data.bin"), binary));
    ASSERT_TRUE(fs.writeFile(path("source/docs/excluded.txt"), "not selected"));
    ASSERT_TRUE(fs.writeFile(path("source/empty-file"), ""));
    ASSERT_TRUE(fs.writeFile(path("source/run.sh"), "#!/bin/sh\n"));
    std::filesystem::permissions(
        path("source/run.sh").str(), std::filesystem::perms::owner_all
    );
    const auto modified =
        std::chrono::sys_seconds{std::chrono::seconds{1'712'345'678}};
    std::filesystem::last_write_time(
        path("source/run.sh").str(),
        std::filesystem::file_time_type::clock::from_sys(modified)
    );
    const auto tree = SourceTreeBuilder::build(
        fs, Source{
                path("source"),
                {"docs/data.bin", "empty-directory", "empty-file", "run.sh"}
            }
    );
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "daily";
    const auto workspace = path("workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_TRUE(snapshot) << snapshot.error().message();
    EXPECT_EQ(snapshot->name, "daily." + archiveTypeToStr(GetParam()));
    EXPECT_EQ(
        snapshot->location.path, Path::join(workspace, snapshot->name).str()
    );
    EXPECT_EQ(snapshot->location.type, LocationType::local);
    auto contents = readArchive(snapshot->location.path);
    ASSERT_TRUE(contents) << contents.error().message();
    EXPECT_EQ(
        contents->format,
        GetParam() == ArchiveType::zip ? ARCHIVE_FORMAT_ZIP : ARCHIVE_FORMAT_TAR
    );
    EXPECT_EQ(
        contents->filter, GetParam() == ArchiveType::zip ? ARCHIVE_FILTER_NONE
                                                         : ARCHIVE_FILTER_GZIP
    );
    ASSERT_EQ(contents->entries.size(), 5);
    EXPECT_EQ(contents->entries.at("docs/data.bin").contents, binary);
    EXPECT_EQ(contents->entries.at("empty-file").contents, "");
    EXPECT_EQ(contents->entries.at("empty-directory").type, AE_IFDIR);
    EXPECT_FALSE(contents->entries.contains("docs/excluded.txt"));
    EXPECT_EQ(contents->entries.at("run.sh").permissions, 0700);
    EXPECT_EQ(
        contents->entries.at("run.sh").mtime,
        modified.time_since_epoch().count()
    );
}

TEST_P(
    ArchiveSnapshotterTest,
    MergesDirectoriesAcrossSourcesWithoutDuplicateEntries
) {
    ASSERT_TRUE(fs.createDirectory(path("first/docs")));
    ASSERT_TRUE(fs.createDirectory(path("second/docs")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ASSERT_TRUE(fs.writeFile(path("first/docs/one"), "first"));
    ASSERT_TRUE(fs.writeFile(path("second/docs/two"), "second"));
    const std::vector<Source> sources{
        {path("first"), {"docs/one"}}, {path("second"), {"docs/two"}}
    };
    const auto trees = SourceTreeBuilder::build(fs, sources);
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "daily";
    const auto workspace = path("workspace");
    auto snapshot = snapshotter.snapshot({name, trees, workspace});
    ASSERT_TRUE(snapshot) << snapshot.error().message();
    auto contents = readArchive(snapshot->location.path);
    ASSERT_TRUE(contents) << contents.error().message();
    ASSERT_EQ(contents->entries.size(), 3);
    EXPECT_EQ(contents->entries.at("docs/one").contents, "first");
    EXPECT_EQ(contents->entries.at("docs/two").contents, "second");
}

TEST_P(ArchiveSnapshotterTest, ConflictingFilesAcrossSourcesAreReported) {
    ASSERT_TRUE(fs.createDirectory(path("first")));
    ASSERT_TRUE(fs.createDirectory(path("second")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ASSERT_TRUE(fs.writeFile(path("first/file"), "first"));
    ASSERT_TRUE(fs.writeFile(path("second/file"), "second"));
    const std::vector<Source> sources{
        {path("first"), {"file"}}, {path("second"), {"file"}}
    };
    const auto trees = SourceTreeBuilder::build(fs, sources);
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "daily";
    const auto workspace = path("workspace");
    auto snapshot = snapshotter.snapshot({name, trees, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_THAT(
        snapshot.error().message(),
        testing::HasSubstr("Conflicting archive path: 'file'")
    );
}

TEST_P(ArchiveSnapshotterTest, EmptySelectionsProduceReadableArchives) {
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "empty";
    const auto workspace = path("workspace");
    const std::vector<SourceTree> trees{{.root = path("missing"), .nodes = {}}};
    auto snapshot = snapshotter.snapshot({name, trees, workspace});
    ASSERT_TRUE(snapshot) << snapshot.error().message();
    auto contents = readArchive(snapshot->location.path);
    ASSERT_TRUE(contents) << contents.error().message();
    EXPECT_TRUE(contents->entries.empty());
}

TEST_P(ArchiveSnapshotterTest, UsesCompression) {
    ASSERT_TRUE(fs.createDirectory(path("source")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    const Str data(256 * 1024, 'a');
    ASSERT_TRUE(fs.writeFile(path("source/large"), data));
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"large"}});
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "compressed";
    const auto workspace = path("workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_TRUE(snapshot) << snapshot.error().message();
    auto size = fs.fileSize(snapshot->location.path);
    ASSERT_TRUE(size);
    EXPECT_LT(*size, data.size() / 2);
    auto contents = readArchive(snapshot->location.path);
    ASSERT_TRUE(contents) << contents.error().message();
    EXPECT_EQ(contents->entries.at("large").contents, data);
}

TEST_P(
    ArchiveSnapshotterTest,
    StoresSymlinksWithoutFollowingThemIncludingBrokenAndDirectoryLinks
) {
    ASSERT_TRUE(fs.createDirectory(path("source/assets")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ASSERT_TRUE(fs.writeFile(path("source/assets/file"), "contents"));
    std::filesystem::create_symlink("file", path("source/assets/link").str());
    std::filesystem::create_symlink(
        "missing", path("source/assets/broken").str()
    );
    std::filesystem::create_directory_symlink(
        "..", path("source/assets/cycle").str()
    );
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"assets"}});
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "links";
    const auto workspace = path("workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_TRUE(snapshot) << snapshot.error().message();
    auto contents = readArchive(snapshot->location.path);
    ASSERT_TRUE(contents) << contents.error().message();
    ASSERT_EQ(contents->entries.size(), 5);
    EXPECT_EQ(contents->entries.at("assets/link").type, AE_IFLNK);
    EXPECT_EQ(contents->entries.at("assets/link").target, "file");
    EXPECT_EQ(contents->entries.at("assets/broken").target, "missing");
    EXPECT_EQ(contents->entries.at("assets/cycle").target, "..");
}

TEST_P(ArchiveSnapshotterTest, PreservesUtf8Filenames) {
    ASSERT_TRUE(fs.createDirectory(path("source")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ASSERT_TRUE(fs.writeFile(path("source/zażółć.txt"), "unicode"));
    const Str longName = Str(180, 'a') + ".txt";
    ASSERT_TRUE(fs.writeFile(path("source/" + longName), "long path"));
    const auto tree = SourceTreeBuilder::build(
        fs, Source{path("source"), {"zażółć.txt", longName}}
    );
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "unicode";
    const auto workspace = path("workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_TRUE(snapshot) << snapshot.error().message();
    auto contents = readArchive(snapshot->location.path);
    ASSERT_TRUE(contents) << contents.error().message();
    EXPECT_EQ(contents->entries.at("zażółć.txt").contents, "unicode");
    EXPECT_EQ(contents->entries.at(longName).contents, "long path");
}

TEST_P(ArchiveSnapshotterTest, InvalidOutputDirectoryReturnsAnError) {
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "daily";
    const auto workspace = path("missing/workspace");
    auto snapshot = snapshotter.snapshot({name, {}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_EQ(snapshot.error().code(), ErrorCode::archiverFailure);
    EXPECT_THAT(
        snapshot.error().message(), testing::HasSubstr("Could not open archive")
    );
}

TEST_P(
    ArchiveSnapshotterTest, RejectsPathsOutsideSourceRootAndUnsupportedFileTypes
) {
    ASSERT_TRUE(fs.createDirectory(path("source")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ASSERT_TRUE(fs.writeFile(path("outside"), "outside"));
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "invalid";
    const auto workspace = path("workspace");
    SourceTree tree{
        .root = path("source"),
        .nodes = {
            {"outside",
             {.type = SourceTree::NodeType::file,
              .relativePath = "../outside",
              .name = "outside"}}
        }
    };
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_THAT(
        snapshot.error().message(),
        testing::HasSubstr("outside its source root")
    );
    ASSERT_EQ(mkfifo(path("source/pipe").str().c_str(), 0600), 0);
    tree.nodes.clear();
    tree.nodes.emplace(
        "pipe", SourceTree::Node{
                    .type = SourceTree::NodeType::file,
                    .relativePath = "pipe",
                    .name = "pipe"
                }
    );
    const Str pipeName = "pipe";
    snapshot = snapshotter.snapshot({pipeName, std::span{&tree, 1}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_THAT(snapshot.error().message(), testing::HasSubstr("pipe"));
}

TEST_P(ArchiveSnapshotterTest, RejectsIncludingItsOwnOutput) {
    ASSERT_TRUE(fs.createDirectory(path("source/workspace")));
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"workspace"}});
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "daily";
    const auto workspace = path("source/workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_THAT(
        snapshot.error().message(),
        testing::HasSubstr("Can't add archive to itself")
    );
}

TEST_P(ArchiveSnapshotterTest, ExistingOutputIsNotOverwritten) {
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    const Str name = "existing";
    const auto workspace = path("workspace");
    const auto output =
        Path::join(workspace, name + "." + archiveTypeToStr(GetParam()));
    ASSERT_TRUE(fs.writeFile(output, "original snapshot"));
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    auto snapshot = snapshotter.snapshot({name, {}, workspace});
    ASSERT_FALSE(snapshot);
    auto contents = fs.readFile(output);
    ASSERT_TRUE(contents);
    EXPECT_EQ(*contents, "original snapshot");
}

TEST_P(ArchiveSnapshotterTest, RejectsItsOwnOutputThroughASourceRootAlias) {
    ASSERT_TRUE(fs.createDirectory(path("source/workspace")));
    std::filesystem::create_directory_symlink(
        path("source").str(), path("alias").str()
    );
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("alias"), {"workspace"}});
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "daily";
    const auto workspace = path("source/workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_EQ(snapshot.error().code(), ErrorCode::archiverFailure);
}

TEST_P(
    ArchiveSnapshotterTest, FinalizationFailureDoesNotReturnSuccessfulMetadata
) {
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    struct FileSizeLimit {
        rlimit previous{};
        decltype(std::signal(SIGXFSZ, SIG_IGN)) handler;
        FileSizeLimit() {
            EXPECT_EQ(getrlimit(RLIMIT_FSIZE, &previous), 0);
            handler = std::signal(SIGXFSZ, SIG_IGN);
            auto limited = previous;
            limited.rlim_cur = 0;
            EXPECT_EQ(setrlimit(RLIMIT_FSIZE, &limited), 0);
        }
        ~FileSizeLimit() {
            EXPECT_EQ(setrlimit(RLIMIT_FSIZE, &previous), 0);
            std::signal(SIGXFSZ, handler);
        }
    } limit;
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "empty";
    const auto workspace = path("workspace");
    auto snapshot = snapshotter.snapshot({name, {}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_EQ(snapshot.error().code(), ErrorCode::archiverFailure);
    EXPECT_THAT(
        snapshot.error().message(),
        testing::HasSubstr("Could not finalize archive")
    );
}

TEST_P(
    ArchiveSnapshotterTest,
    SelectingChildrenThroughSymlinkDoesNotCreateEscapingEntries
) {
    ASSERT_TRUE(fs.createDirectory(path("source")));
    ASSERT_TRUE(fs.createDirectory(path("outside")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ASSERT_TRUE(fs.writeFile(path("outside/file"), "outside data"));
    std::filesystem::create_directory_symlink(
        path("outside").str(), path("source/link").str()
    );
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"link/file"}});
    ArchiveSnapshotter snapshotter{fs, GetParam()};
    const Str name = "links";
    const auto workspace = path("workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_THAT(
        snapshot.error().message(),
        testing::HasSubstr("Selected parent is not a directory")
    );
}

TEST_P(
    ArchiveSnapshotterTest, FileAbstractionPropagatesOutputWriteAndCloseFailures
) {
    using namespace testing;
    for (const auto failWrite : {true, false}) {
        StrictMock<MockFileSystem> mocked;
        auto output = std::make_unique<StrictMock<MockFile>>();
        if (failWrite) {
            EXPECT_CALL(*output, write(_))
                .WillOnce(Return(
                    Error::unexpected(
                        ErrorCode::fileSystemError, "output write failed"
                    )
                ));
        } else {
            EXPECT_CALL(*output, write(_))
                .WillRepeatedly(Return(Result<void>{}));
            EXPECT_CALL(*output, close())
                .WillOnce(Return(
                    Error::unexpected(
                        ErrorCode::fileSystemError, "output close failed"
                    )
                ));
        }
        EXPECT_CALL(mocked, openFile(_, FileMode::createNew))
            .WillOnce(Return(ByMove(std::move(output))));
        ArchiveSnapshotter snapshotter{mocked, GetParam()};
        const Str name = "empty";
        const auto workspace = path("workspace");
        auto snapshot = snapshotter.snapshot({name, {}, workspace});
        ASSERT_FALSE(snapshot);
        EXPECT_EQ(snapshot.error().code(), ErrorCode::archiverFailure);
        EXPECT_THAT(
            snapshot.error().message(),
            HasSubstr(failWrite ? "output write failed" : "output close failed")
        );
    }
}

TEST_P(
    ArchiveSnapshotterTest, FileAbstractionPropagatesInputFailuresAndSizeChanges
) {
    using namespace testing;
    ASSERT_TRUE(fs.createDirectory(path("source")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    ASSERT_TRUE(fs.writeFile(path("source/file"), "data"));
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"file"}});
    const auto metadata = fs.info(path("source/file"));
    ASSERT_TRUE(metadata);
    for (const auto* failure :
         {"input read failed", "File size changed", "input close failed"}) {
        StrictMock<MockFileSystem> mocked;
        auto input = std::make_unique<StrictMock<MockFile>>();
        if (Str{failure} == "input read failed") {
            EXPECT_CALL(*input, read(_))
                .WillOnce(Return(
                    Error::unexpected(ErrorCode::fileSystemError, failure)
                ));
        } else if (Str{failure} == "File size changed") {
            EXPECT_CALL(*input, read(_)).WillOnce(Return(size_t{0}));
        } else {
            EXPECT_CALL(*input, read(_))
                .WillOnce(Invoke([](std::span<Byte> buffer) -> Result<size_t> {
                    std::ranges::copy(Str{"data"}, buffer.begin());
                    return 4;
                }))
                .WillOnce(Return(size_t{0}));
            EXPECT_CALL(*input, close())
                .WillOnce(Return(
                    Error::unexpected(ErrorCode::fileSystemError, failure)
                ));
        }
        EXPECT_CALL(mocked, info(_)).WillOnce(Return(*metadata));
        EXPECT_CALL(mocked, equivalent(_, _)).WillOnce(Return(false));
        EXPECT_CALL(mocked, openFile(_, FileMode::read))
            .WillOnce(Return(ByMove(std::move(input))));
        EXPECT_CALL(mocked, openFile(_, FileMode::createNew))
            .WillOnce(Invoke([&](const Path& output, FileMode mode) {
                return fs.openFile(output, mode);
            }));
        ArchiveSnapshotter snapshotter{mocked, GetParam()};
        const Str name = failure;
        const auto workspace = path("workspace");
        auto snapshot =
            snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
        ASSERT_FALSE(snapshot);
        EXPECT_EQ(snapshot.error().code(), ErrorCode::archiverFailure);
        EXPECT_THAT(snapshot.error().message(), HasSubstr(failure));
    }
}

TEST_P(ArchiveSnapshotterTest, FileSystemPropagatesDirectoryListingFailures) {
    using namespace testing;
    ASSERT_TRUE(fs.createDirectory(path("source/directory")));
    ASSERT_TRUE(fs.createDirectory(path("workspace")));
    const auto metadata = fs.info(path("source/directory"));
    ASSERT_TRUE(metadata);
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"directory"}});
    StrictMock<MockFileSystem> mocked;
    EXPECT_CALL(mocked, info(_)).WillOnce(Return(*metadata));
    EXPECT_CALL(mocked, openFile(_, FileMode::createNew))
        .WillOnce(Invoke([&](const Path& output, FileMode mode) {
            return fs.openFile(output, mode);
        }));
    EXPECT_CALL(mocked, listEntries(_))
        .WillOnce(Return(
            Error::unexpected(
                ErrorCode::fileSystemError, "directory listing failed"
            )
        ));
    ArchiveSnapshotter snapshotter{mocked, GetParam()};
    const Str name = "listing";
    const auto workspace = path("workspace");
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_FALSE(snapshot);
    EXPECT_THAT(
        snapshot.error().message(), HasSubstr("directory listing failed")
    );
}

INSTANTIATE_TEST_SUITE_P(
    Formats, ArchiveSnapshotterTest,
    testing::Values(ArchiveType::zip, ArchiveType::tgz),
    [](const testing::TestParamInfo<ArchiveType>& parameter) {
        return parameter.param == ArchiveType::zip ? "Zip" : "Tgz";
    }
);

}  // namespace anchor::test
