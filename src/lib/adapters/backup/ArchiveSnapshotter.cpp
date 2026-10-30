#include "ArchiveSnapshotter.hh"

#include <archive.h>
#include <archive_entry.h>

#include <array>
#include <kcore/Functional.hh>
#include <unordered_map>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {
namespace {

constexpr size_t archiveBufferSize = 64 * 1024;
using ArchiveHandle = std::unique_ptr<archive, decltype(&archive_write_free)>;
using EntryHandle =
    std::unique_ptr<archive_entry, decltype(&archive_entry_free)>;

Error archiveError(archive* handle, const Str& operation) {
    const auto* reason = archive_error_string(handle);
    return Error{
        ErrorCode::archiverFailure, "{}: {}", operation,
        reason ? reason : "libarchive failure"
    };
}

Result<void> check(archive* handle, int status, const Str& operation) {
    if (status != ARCHIVE_OK) {
        return Error::unexpected(archiveError(handle, operation));
    }
    return {};
}

Result<Str> relativeName(const kc::Path& root, const kc::Path& path) {
    const auto relative = path.lexicallyRelative(root).normalized();
    if (relative.str().empty() || relative.isAbsolute() ||
        relative.str() == ".." || relative.startsWith("../")) {
        return Error::unexpected(
            ErrorCode::archiverFailure,
            "Archive path is outside its source root: '{}'", path.str()
        );
    }
    return relative.str();
}

class ArchiveWriter : public kc::NonCopyable, public kc::NonMovable {
   public:
    explicit ArchiveWriter(const kc::FileSystem& fs) : m_fs(fs) {}

    Result<void> write(
        ArchiveType format, const kc::Path& output,
        std::span<const SourceTree> trees
    ) {
        m_archive.reset(archive_write_new());
        if (not m_archive) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Could not allocate archive writer"
            );
        }
        return kc::pipeline(
            [this] {
                return check(
                    m_archive.get(),
                    archive_write_set_bytes_in_last_block(m_archive.get(), 1),
                    "Could not configure archive output"
                );
            },
            [this, format] { return configure(format); },
            [this, &output] { return open(output); },
            [this, trees] { return writeSources(trees); },
            [this] { return finish(); }
        );
    }

   private:
    Result<void> configure(ArchiveType format) {
        auto* writer = m_archive.get();
        switch (format) {
            case ArchiveType::zip:
                return kc::pipeline(
                    [&] {
                        return check(
                            writer, archive_write_set_format_zip(writer),
                            "Could not set ZIP format"
                        );
                    },
                    [&] {
                        return check(
                            writer,
                            archive_write_zip_set_compression_deflate(writer),
                            "Could not enable ZIP compression"
                        );
                    }
                );
            case ArchiveType::tgz:
                return kc::pipeline(
                    [&] {
                        return check(
                            writer,
                            archive_write_set_format_pax_restricted(writer),
                            "Could not set tar format"
                        );
                    },
                    [&] {
                        // POSIX filenames are bytes; avoid locale-dependent
                        // conversion.
                        return check(
                            writer,
                            archive_write_set_format_option(
                                writer, "pax", "hdrcharset", "BINARY"
                            ),
                            "Could not preserve tar filenames"
                        );
                    },
                    [&] {
                        return check(
                            writer, archive_write_add_filter_gzip(writer),
                            "Could not enable gzip compression"
                        );
                    }
                );
            case ArchiveType::directory:
                return Error::unexpected(
                    ErrorCode::archiverFailure,
                    "Archive snapshotter requires ZIP or TGZ format"
                );
        }
        std::unreachable();
    }

    static la_ssize_t writeOutput(
        archive* writer, void* context, const void* data, size_t size
    ) {
        auto& self = *static_cast<ArchiveWriter*>(context);
        auto result =
            self.m_file->write({static_cast<const Byte*>(data), size});
        if (not result) {
            archive_set_error(
                writer, 0, "%s", result.error().message().c_str()
            );
            return -1;
        }
        return static_cast<la_ssize_t>(size);
    }

    Result<void> open(const kc::Path& output) {
        m_output = output;
        return m_fs.openFile(output, kc::FileMode::createNew)
            .transform_error([&](const Error& error) {
                return Error{
                    ErrorCode::archiverFailure,
                    "Could not open archive '{}': {}", output.str(),
                    error.message()
                };
            })
            .and_then([&](std::unique_ptr<kc::File> file) {
                m_file = std::move(file);
                return check(
                    m_archive.get(),
                    archive_write_open(
                        m_archive.get(), this, nullptr, writeOutput, nullptr
                    ),
                    fmt::format("Could not open archive '{}'", output.str())
                );
            });
    }

    Result<void> finish() {
        return kc::pipeline(
            [&] {
                return check(
                    m_archive.get(), archive_write_close(m_archive.get()),
                    "Could not finalize archive"
                );
            },
            [&] {
                return m_file->close().transform_error([](const Error& error) {
                    return Error{
                        ErrorCode::archiverFailure,
                        "Could not finalize archive: {}", error.message()
                    };
                });
            }
        );
    }

    Result<void> writeSources(std::span<const SourceTree> trees) {
        for (const auto& tree : trees) {
            auto result =
                tree.root.absolute().and_then([&](const kc::Path& root) {
                    return writeNodes(root, tree.nodes);
                });
            if (not result) {
                return result;
            }
        }
        return {};
    }

    Result<void> writeNodes(
        const kc::Path& root, const SourceTree::Nodes& nodes
    ) {
        for (const auto& node : nodes | std::views::values) {
            auto result = kc::pipeline(
                [&] {
                    return writePath(
                        root,
                        kc::Path::join(root, node.relativePath).normalized(),
                        node.children.empty()
                    );
                },
                [&] { return writeNodes(root, node.children); }
            );
            if (not result) {
                return result;
            }
        }
        return {};
    }

    Result<void> writePath(
        const kc::Path& root, const kc::Path& path, bool recursive
    ) {
        auto name = relativeName(root, path);
        if (not name) {
            return Error::unexpected(name.error());
        }
        auto info = m_fs.info(path);
        if (not info) {
            return Error::unexpected(info.error());
        }
        if (not recursive && info->type != kc::FileInfo::Type::directory) {
            return Error::unexpected(
                ErrorCode::archiverFailure,
                "Selected parent is not a directory: '{}'", path.str()
            );
        }
        return writeEntry(path, *name, *info).and_then([&]() -> Result<void> {
            if (not recursive || info->type != kc::FileInfo::Type::directory) {
                return {};
            }
            auto children = m_fs.listEntries(path);
            if (not children) {
                return Error::unexpected(children.error());
            }
            for (const auto& child : *children) {
                if (auto result = writePath(root, child.normalized(), true);
                    not result) {
                    return result;
                }
            }
            return {};
        });
    }

    Result<bool> registerEntry(const Str& name, kc::FileInfo::Type type) {
        const auto [previous, inserted] = m_entries.emplace(name, type);
        if (not inserted &&
            (type != kc::FileInfo::Type::directory ||
             previous->second != kc::FileInfo::Type::directory)) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Conflicting archive path: '{}'",
                name
            );
        }
        return inserted;
    }

    Result<void> writeEntry(
        const kc::Path& path, const Str& name, const kc::FileInfo& info
    ) {
        if (info.type == kc::FileInfo::Type::other) {
            return Error::unexpected(
                ErrorCode::archiverFailure,
                "Unsupported source file type: '{}'", path.str()
            );
        }
        if (info.type == kc::FileInfo::Type::regular) {
            auto same = m_fs.equivalent(path, m_output);
            if (not same) {
                return Error::unexpected(same.error());
            }
            if (*same) {
                return Error::unexpected(
                    ErrorCode::archiverFailure,
                    "Can't add archive to itself: '{}'", path.str()
                );
            }
        }
        auto inserted = registerEntry(name, info.type);
        if (not inserted) {
            return Error::unexpected(inserted.error());
        }
        if (not *inserted) {
            return {};
        }
        EntryHandle entry{archive_entry_new(), archive_entry_free};
        if (not entry) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Could not allocate archive entry"
            );
        }
        const auto type = info.type == kc::FileInfo::Type::directory ? AE_IFDIR
                          : info.type == kc::FileInfo::Type::symlink ? AE_IFLNK
                                                                     : AE_IFREG;
        archive_entry_set_pathname(entry.get(), name.c_str());
        archive_entry_set_filetype(entry.get(), type);
        archive_entry_set_perm(entry.get(), info.permissions);
        archive_entry_set_uid(entry.get(), info.owner);
        archive_entry_set_gid(entry.get(), info.group);
        archive_entry_set_size(entry.get(), info.size);
        const auto seconds =
            std::chrono::floor<std::chrono::seconds>(info.modified);
        archive_entry_set_mtime(
            entry.get(), seconds.time_since_epoch().count(),
            (info.modified - seconds).count()
        );
        if (info.type == kc::FileInfo::Type::symlink) {
            archive_entry_set_symlink(entry.get(), info.linkTarget.c_str());
        }
        auto* writer = m_archive.get();
        return kc::pipeline(
            [&] {
                return check(
                    writer, archive_write_header(writer, entry.get()),
                    fmt::format("Could not write header for '{}'", name)
                );
            },
            [&] {
                return info.type == kc::FileInfo::Type::regular
                           ? copyData(path, info.size)
                           : Result<void>{};
            },
            [&] {
                return check(
                    writer, archive_write_finish_entry(writer),
                    fmt::format("Could not finish entry '{}'", name)
                );
            }
        );
    }

    Result<void> writeData(std::span<const Byte> data, const kc::Path& path) {
        while (not data.empty()) {
            const auto written =
                archive_write_data(m_archive.get(), data.data(), data.size());
            if (written <= 0) {
                return Error::unexpected(archiveError(
                    m_archive.get(),
                    fmt::format("Could not write file '{}'", path.str())
                ));
            }
            data = data.subspan(static_cast<size_t>(written));
        }
        return {};
    }

    Result<void> copyData(const kc::Path& path, u64 expectedSize) {
        auto file = m_fs.openFile(path, kc::FileMode::read);
        if (not file) {
            return Error::unexpected(file.error());
        }
        std::array<Byte, archiveBufferSize> buffer;
        u64 total = 0;
        for (;;) {
            auto count = (*file)->read(buffer);
            if (not count) {
                return Error::unexpected(count.error());
            }
            if (*count == 0) {
                break;
            }
            if (auto result = writeData(std::span{buffer}.first(*count), path);
                not result) {
                return result;
            }
            total += *count;
        }
        if (total != expectedSize) {
            return Error::unexpected(
                ErrorCode::archiverFailure,
                "File size changed while archiving '{}'", path.str()
            );
        }
        return (*file)->close();
    }

    const kc::FileSystem& m_fs;
    kc::Path m_output;
    std::unique_ptr<kc::File> m_file;
    ArchiveHandle m_archive{nullptr, archive_write_free};
    std::unordered_map<Str, kc::FileInfo::Type> m_entries;
};

}  // namespace

ArchiveSnapshotter::ArchiveSnapshotter(
    const kc::FileSystem& fs, ArchiveType format
)
    : m_format(format), m_fs(fs) {}

Result<SnapshotMetadata> ArchiveSnapshotter::snapshot(
    const SnapshotArgs& args
) {
    const auto name = args.name + "." + archiveTypeToStr(m_format);
    const auto output = kc::Path::join(args.workspace, name);
    ArchiveWriter writer{m_fs};
    return writer.write(m_format, output, args.trees)
        .transform([&] {
            return SnapshotMetadata{
                .name = name, .location = Location::local(output)
            };
        })
        .transform_error([](const Error& error) {
            return Error{ErrorCode::archiverFailure, error.message()};
        });
}

}  // namespace anchor
