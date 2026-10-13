#include "OnDiskSnapshotStore.hh"

namespace anchor {

OnDiskSnapshotStore::OnDiskSnapshotStore(const FileSystem& fs) : m_fs(fs) {}

Result<SnapshotMetadata> OnDiskSnapshotStore::put(
    const SnapshotMetadata& metadata, const Location& destination
) {
    if (destination.type != LocationType::local) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid destination type"
        );
    }

    const auto& from = Path{metadata.location.path}.resolveEnv();
    const auto& to = Path::join(destination.path, metadata.name).resolveEnv();

    if (not m_fs.copyDirectoryRecursive(from, to)) {
        return Error::unexpected(
            ErrorCode::fileSystemError,
            "Failed to copy directory from '{}' to '{}'", from.str(), to.str()
        );
    }

    auto newMetadata = metadata;
    newMetadata.location = destination;
    return newMetadata;
}

}  // namespace anchor
