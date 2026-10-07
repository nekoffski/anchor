#pragma once

#include "base/FileSystem.hh"
#include "core/ports/Snapshotter.hh"

namespace anchor {

class DirectorySnapshotter : public Snapshotter {
   public:
    explicit DirectorySnapshotter(const FileSystem& fs) : m_fs(fs) {}

    Result<SnapshotMetadata> snapshot(
        const Str& name, std::span<const Filter> filters, const Path& workspace
    ) override {
        const auto snapshotName = newSnapshotName(name);
        const auto path = Path::join(workspace, snapshotName);
        m_fs.createDirectory(path);

        for (const auto& filter : filters) {
            auto source = filter.resolveEnv();
            auto destination = Path::join(path, source.filenameWithExtension());

            if (auto result = m_fs.copyFile(source, destination); not result) {
                log::error(
                    "Failed to copy file from '{}' to '{}' - {}", source.str(),
                    destination.str(), result.error()
                );
            }
        }

        return {};
    }

   private:
    Str newSnapshotName(const Str& backupName) {
        const auto ts = std::chrono::system_clock::now();
        return fmt::format("{}_{}", backupName, ts.time_since_epoch().count());
    }

    const FileSystem& m_fs;
};

}  // namespace anchor
