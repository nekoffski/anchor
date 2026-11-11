#include "DirectorySnapshotter.hh"

#include <ranges>

#include "SnapshotBuilder.hh"

namespace anchor {
namespace {

class DirectoryWriter : public SnapshotWriter {
   public:
    DirectoryWriter(const kc::FileSystem& fs, const kc::Path& root)
        : m_fs(fs), m_root(root) {}

    Result<void> finish() override {
        for (const auto& [path, info] : m_directories | std::views::reverse) {
            if (auto r = applyMetadata(path, info); not r) {
                return r;
            }
        }
        return {};
    }

   private:
    Result<void> beginEntry(
        const Str& name, const kc::FileInfo& info
    ) override {
        m_path = kc::Path::join(m_root, name);
        m_info = info;
        switch (info.type) {
            case kc::FileInfo::Type::directory:
                m_directories.emplace_back(m_path, info);
                return m_fs.createDirectory(m_path);
            case kc::FileInfo::Type::symlink:
                return m_fs.createSymlink(info.linkTarget, m_path);
            case kc::FileInfo::Type::regular: {
                auto file = m_fs.openFile(m_path, kc::FileMode::createNew);
                if (not file) {
                    return Error::unexpected(file.error());
                }
                m_file = std::move(*file);
                return {};
            }
            case kc::FileInfo::Type::other:
                return Error::unexpected(
                    kc::ErrorCode::invalidArgument, "Unsupported snapshot entry"
                );
        }
        std::unreachable();
    }

    Result<void> writeData(std::span<const Byte> data) override {
        return m_file->write(data);
    }

    Result<void> endEntry() override {
        if (m_info.type == kc::FileInfo::Type::directory) {
            return {};
        }
        if (m_file) {
            if (auto r = m_file->close(); not r) {
                return r;
            }
            m_file.reset();
        }
        return applyMetadata(m_path, m_info);
    }

    Result<void> applyMetadata(
        const kc::Path& path, const kc::FileInfo& info
    ) const {
        if (info.type != kc::FileInfo::Type::symlink) {
            const auto permissions =
                m_fs.setPermissions(path, info.permissions);
            if (not permissions) {
                return permissions;
            }
        }
        return m_fs.setModifiedTime(path, info.modified);
    }

    const kc::FileSystem& m_fs;
    kc::Path m_root;
    kc::Path m_path;
    kc::FileInfo m_info{};
    std::unique_ptr<kc::File> m_file;
    std::vector<std::pair<kc::Path, kc::FileInfo>> m_directories;
};

}  // namespace

DirectorySnapshotter::DirectorySnapshotter(const kc::FileSystem& fs)
    : m_fs(fs) {}

Result<SnapshotMetadata> DirectorySnapshotter::snapshot(
    const SnapshotArgs& args
) {
    if (auto r = validateSnapshotArgs(args); not r) {
        return Error::unexpected(r.error());
    }
    const auto path = kc::Path::join(args.workspace, args.uuid);
    if (m_fs.exists(path)) {
        return Error::unexpected(
            kc::ErrorCode::fileSystemError, "Snapshot already exists: '{}'",
            path.str()
        );
    }
    if (auto r = m_fs.createDirectory(path); not r) {
        return Error::unexpected(r.error());
    }
    DirectoryWriter writer{m_fs, path};
    return SnapshotBuilder{m_fs}.write(
        args, path, writer, ArchiveType::directory
    );
}

}  // namespace anchor
