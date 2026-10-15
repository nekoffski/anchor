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
        ArchiveFormat format, const std::vector<Path>& input, const Path& output
    ) {
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

        archive_write_open_filename(archive, output.str().c_str());

        archiveDestroyer.dismiss();
        return std::make_unique<ArchiveWriter>(archive, Guard{});
    }

    ArchiveWriter(archive* a, Guard) : m_archive(a) {}

    ~ArchiveWriter() {
        if (m_archive) {
            archive_write_close(m_archive);
            archive_write_free(m_archive);
        }
    }

   private:
    archive* m_archive{nullptr};
};

// void check(int result, archive* a) {
//     if (result != ARCHIVE_OK) {
//         const char* error = archive_error_string(a);
//         throw std::runtime_error(error ? error : "libarchive error");
//     }
// }

// void pack(
//     const fs::path& source, const fs::path& output, ArchiveFormat format
// ) {
//     if (!fs::is_directory(source)) {
//         throw std::runtime_error("Source must be a directory");
//     }

//     using Writer = std::unique_ptr<archive, decltype(&archive_write_free)>;

//     Writer a(archive_write_new(), &archive_write_free);
//     if (!a) {
//         throw std::bad_alloc{};
//     }

//     // Configure archive format and compression.
//     if (format == ArchiveFormat::TarGzip) {
//         check(archive_write_add_filter_gzip(a.get()), a.get());
//         check(archive_write_set_format_pax_restricted(a.get()), a.get());
//     } else {
//         check(archive_write_set_format_zip(a.get()), a.get());
//         check(archive_write_zip_set_compression_deflate(a.get()), a.get());
//     }

//     check(archive_write_open_filename(a.get(), output.c_str()), a.get());

//     for (const auto& item : fs::recursive_directory_iterator(source)) {
//         const fs::path path = item.path();
//         const auto status = fs::symlink_status(path);
//         const bool dir = fs::is_directory(status);

//         // This example only archives directories and regular files.
//         if (!dir && !fs::is_regular_file(status)) {
//             continue;
//         }

//         using Entry =
//             std::unique_ptr<archive_entry, decltype(&archive_entry_free)>;

//         Entry entry(archive_entry_new(), &archive_entry_free);
//         if (!entry) {
//             throw std::bad_alloc{};
//         }

//         const auto name = path.lexically_relative(source).generic_string();

//         archive_entry_set_pathname(entry.get(), name.c_str());
//         archive_entry_set_filetype(entry.get(), dir ? AE_IFDIR : AE_IFREG);
//         archive_entry_set_perm(
//             entry.get(), static_cast<int>(status.permissions()) & 0777
//         );
//         archive_entry_set_size(entry.get(), dir ? 0 : fs::file_size(path));

//         const auto mtime =
//         std::chrono::time_point_cast<std::chrono::seconds>(
//             fs::file_time_type::clock::to_sys(fs::last_write_time(path))
//         );
//         archive_entry_set_mtime(
//             entry.get(), mtime.time_since_epoch().count(), 0
//         );

//         check(archive_write_header(a.get(), entry.get()), a.get());

//         if (!dir) {
//             std::ifstream input(path, std::ios::binary);
//             if (!input) {
//                 throw std::runtime_error("Cannot open: " + path.string());
//             }

//             std::array<char, 64 * 1024> buffer{};

//             while (input) {
//                 input.read(buffer.data(), buffer.size());
//                 auto bytes = input.gcount();
//                 std::streamsize sent = 0;

//                 while (sent < bytes) {
//                     auto n = archive_write_data(
//                         a.get(), buffer.data() + sent,
//                         static_cast<size_t>(bytes - sent)
//                     );

//                     if (n <= 0) {
//                         throw std::runtime_error("Archive write failed");
//                     }

//                     sent += n;
//                 }
//             }

//             if (!input.eof()) {
//                 throw std::runtime_error("File read failed: " +
//                 path.string());
//             }
//         }

//         check(archive_write_finish_entry(a.get()), a.get());
//     }

//     check(archive_write_close(a.get()), a.get());
// }

}  // namespace anchor
