#include "ArchiveSnapshotter.hh"

#include <archive.h>
#include <archive_entry.h>

#include <array>
#include <kcore/Functional.hh>

#include "SnapshotBuilder.hh"
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

class ArchiveWriter : public SnapshotWriter {
   public:
    explicit ArchiveWriter(const kc::FileSystem& fs) : m_fs(fs) {}

    Result<SnapshotMetadata> write(
        ArchiveType format, const kc::Path& output, const SnapshotArgs& args
    ) {
        m_archive.reset(archive_write_new());
        if (not m_archive) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Could not allocate archive writer"
            );
        }
        auto opened = kc::pipeline(
            [this] {
                return check(
                    m_archive.get(),
                    archive_write_set_bytes_in_last_block(m_archive.get(), 1),
                    "Could not configure archive output"
                );
            },
            [this, format] { return configure(format); },
            [this, &output] { return open(output); }
        );
        if (not opened) {
            return Error::unexpected(opened.error());
        }
        auto snapshot = SnapshotBuilder{m_fs}.write(args, m_output, *this);
        if (not snapshot) {
            return Error::unexpected(snapshot.error());
        }
        if (auto r = finish(); not r) {
            return Error::unexpected(r.error());
        }
        return snapshot;
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

    Result<void> beginEntry(
        const Str& name, const kc::FileInfo& info
    ) override {
        m_entryName = name;
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
        archive_entry_set_size(
            entry.get(),
            info.type == kc::FileInfo::Type::regular ? info.size : 0
        );
        const auto seconds =
            std::chrono::floor<std::chrono::seconds>(info.modified);
        archive_entry_set_mtime(
            entry.get(), seconds.time_since_epoch().count(),
            (info.modified - seconds).count()
        );
        if (info.type == kc::FileInfo::Type::symlink) {
            archive_entry_set_symlink(entry.get(), info.linkTarget.c_str());
        }
        return check(
            m_archive.get(), archive_write_header(m_archive.get(), entry.get()),
            fmt::format("Could not write header for '{}'", name)
        );
    }

    Result<void> writeData(std::span<const Byte> data) override {
        while (not data.empty()) {
            const auto written =
                archive_write_data(m_archive.get(), data.data(), data.size());
            if (written <= 0) {
                return Error::unexpected(archiveError(
                    m_archive.get(),
                    fmt::format("Could not write file '{}'", m_entryName)
                ));
            }
            data = data.subspan(static_cast<size_t>(written));
        }
        return {};
    }

    Result<void> endEntry() override {
        return check(
            m_archive.get(), archive_write_finish_entry(m_archive.get()),
            fmt::format("Could not finish entry '{}'", m_entryName)
        );
    }

    const kc::FileSystem& m_fs;
    kc::Path m_output;
    std::unique_ptr<kc::File> m_file;
    ArchiveHandle m_archive{nullptr, archive_write_free};
    Str m_entryName;
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
    return writer.write(m_format, output, args)
        .transform_error([](const Error& error) {
            return Error{ErrorCode::archiverFailure, error.message()};
        });
}

}  // namespace anchor
