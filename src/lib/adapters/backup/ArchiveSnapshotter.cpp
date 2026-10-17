#include "ArchiveSnapshotter.hh"

#include <archive.h>
#include <archive_entry.h>

#include <memory>

#include "base/Error.hh"
#include "base/Functional.hh"

namespace anchor {

enum class ArchiveFormat { tgz, zip };

class ArchiveWriter : public NonCopyable {
    struct Guard {};

   public:
    static Result<std::unique_ptr<ArchiveWriter>> create(
        ArchiveFormat format, const Path& output, const FileSystem& fs
    ) {
        log::debug("Creating archive writer for {}", output.str());

        auto* archive = archive_write_new();
        if (not archive) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Failed to create archive writer"
            );
        }
        NAMED_DEFER(archiveDestroyer) { archive_write_free(archive); };

        if (format == ArchiveFormat::tgz) {
            if (archive_write_add_filter_gzip(archive) != ARCHIVE_OK) {
                return Error::unexpected(
                    ErrorCode::archiverFailure, "Failed to add gzip filter"
                );
            }
            if (archive_write_set_format_pax_restricted(archive) !=
                ARCHIVE_OK) {
                return Error::unexpected(
                    ErrorCode::archiverFailure,
                    "Failed to set pax restricted format"
                );
            }
        } else {
            if (archive_write_set_format_zip(archive) != ARCHIVE_OK) {
                return Error::unexpected(
                    ErrorCode::archiverFailure, "Failed to set zip format"
                );
            }
            if (archive_write_zip_set_compression_deflate(archive) !=
                ARCHIVE_OK) {
                return Error::unexpected(
                    ErrorCode::archiverFailure,
                    "Failed to set deflate compression"
                );
            }
        }

        if (archive_write_open_filename(archive, output.str().c_str()) !=
            ARCHIVE_OK) {
            return Error::unexpected(
                ErrorCode::archiverFailure,
                "Failed to open archive for writing: {}", output.str()
            );
        }

        archiveDestroyer.dismiss();
        return std::make_unique<ArchiveWriter>(archive, fs, Guard{});
    }

    Result<void> write(std::span<const SourceTree> trees) {
        for (const auto& tree : trees) {
            if (auto r = writeSource(tree); not r) {
                return r;
            }
        }
        return {};
    }

    ArchiveWriter(archive* a, const FileSystem& fs, Guard)
        : m_archive(a), m_fs(fs) {}

    ~ArchiveWriter() {
        if (m_archive) {
            archive_write_close(m_archive);
            archive_write_free(m_archive);
        }
    }

   private:
    Result<void> writeSource(const SourceTree& tree) {
        return writeLevel(tree.root, tree.nodes);
    }

    Result<void> writeLevel(const Path& root, const SourceTree::Nodes& nodes) {
        for (const auto& node : nodes | std::views::values) {
            writeEntry(root, node);

            if (not node.children.empty()) {
                if (auto r = writeLevel(root, node.children); not r) {
                    return r;
                }
            }
        }

        return {};
    }

    Result<void> writeEntry(const Path& root, const SourceTree::Node& node) {
        using Entry =
            std::unique_ptr<archive_entry, decltype(&archive_entry_free)>;

        Entry entry(archive_entry_new(), &archive_entry_free);

        if (not entry) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Failed to create archive entry"
            );
        }

        const auto fullPath = Path::join(root, node.relativePath);
        bool isDirectory = node.type == SourceTree::NodeType::directory;

        archive_entry_set_pathname(entry.get(), node.relativePath.c_str());
        archive_entry_set_filetype(
            entry.get(), isDirectory ? AE_IFDIR : AE_IFREG
        );

        archive_entry_set_perm(entry.get(), 0777);

        u64 fileSize = 0u;

        if (not isDirectory) {
            if (auto r = m_fs.fileSize(fullPath); not r) {
                return Error::unexpected(r.error());
            } else {
                fileSize = r.value();
            }
        }

        archive_entry_set_size(entry.get(), fileSize);

        // const auto mtime =
        // std::chrono::time_point_cast<std::chrono::seconds>(
        // fs::file_time_type::clock::to_sys(fs::last_write_time(path))
        //         archive_entry_set_mtime(
        //             entry.get(), mtime.time_since_epoch().count(), 0
        //         );

        if (archive_write_header(m_archive, entry.get()) != ARCHIVE_OK) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Failed to write archive header"
            );
        }

        if (not isDirectory) {
            auto data = m_fs.readBinary(fullPath);

            if (not data) {
                return Error::unexpected(
                    ErrorCode::archiverFailure, "Failed to read file: {}",
                    fullPath.str()
                );
            }

            auto n = archive_write_data(
                m_archive, data.value().data(), data.value().size()
            );

            if (n < 0) {
                return Error::unexpected(
                    ErrorCode::archiverFailure, "Failed to write file data: {}",
                    fullPath.str()
                );
            }
        }

        if (archive_write_finish_entry(m_archive) != ARCHIVE_OK) {
            return Error::unexpected(
                ErrorCode::archiverFailure, "Failed to finish archive entry"
            );
        }

        return {};
    }

    archive* m_archive{nullptr};
    const FileSystem& m_fs;
};

ZipSnapshotter::ZipSnapshotter(const FileSystem& fs) : m_fs(fs) {}

Result<SnapshotMetadata> ZipSnapshotter::snapshot(const SnapshotArgs& args) {
    auto dest = Path::join(args.workspace, args.name + ".zip");
    auto archiver = ArchiveWriter::create(ArchiveFormat::zip, dest, m_fs);

    if (not archiver) {
        return Error::unexpected(
            ErrorCode::archiverFailure, "Could not create zip archiver: {}",
            archiver.error()
        );
    }

    if (auto r = archiver->get()->write(args.trees); not r) {
        return Error::unexpected(
            ErrorCode::archiverFailure, "Failed to write snapshot: {}",
            r.error()
        );
    }

    const auto archiveName = args.name + ".zip";

    return SnapshotMetadata{
        .name = archiveName,
        .location = Location::local(Path::join(args.workspace, archiveName)),
    };
}

}  // namespace anchor
