#include "DirectorySnapshotter.hh"

#include "base/Log.hh"

namespace anchor {

DirectorySnapshotter::DirectorySnapshotter(const FileSystem& fs) : m_fs(fs) {}

Result<SnapshotMetadata> DirectorySnapshotter::snapshot(
    const SnapshotArgs& args
) {
    const auto& [name, filters, workspace] = args;

    const auto snapshotName = newSnapshotName(name);
    const auto path = Path::join(workspace, snapshotName);
    if (auto result = m_fs.createDirectory(path); not result) {
        return Error::unexpected(result.error());
    }

    for (const auto& filter : filters) {
        auto source = filter.resolveEnv();
        auto destination = Path::join(path, source.filenameWithExtension());

        if (auto result = m_fs.copyFile(source, destination); not result) {
            log::error(
                "Failed to copy file from '{}' to '{}' - {}", source.str(),
                destination.str(), result.error()
            );
            return Error::unexpected(result.error());
        }
    }

    SnapshotMetadata metadata{
        .name = snapshotName, .location = Location::local(path.str())
    };

    return metadata;
}

Str DirectorySnapshotter::newSnapshotName(const Str& backupName) {
    const auto ts = std::chrono::system_clock::now();
    return fmt::format("{}_{}", backupName, ts.time_since_epoch().count());
}

}  // namespace anchor
