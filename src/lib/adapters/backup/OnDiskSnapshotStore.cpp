#include "OnDiskSnapshotStore.hh"

#include "core/Error.hh"

namespace anchor {

OnDiskSnapshotStore::OnDiskSnapshotStore(const kc::FileSystem& fs) : m_fs(fs) {}

Result<SnapshotMetadata> OnDiskSnapshotStore::put(
    const SnapshotMetadata& metadata, const Location& destination
) {
    if (destination.type != LocationType::local) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid destination type"
        );
    }

    const auto& from = kc::Path{metadata.location.path}.resolveEnv();
    const auto& to =
        kc::Path::join(destination.path, metadata.name).resolveEnv();

    if (auto r = m_fs.copyRecursive(from, to); not r) {
        return Error::unexpected(
            kc::ErrorCode::fileSystemError,
            "Failed to copy from '{}' to '{}' - {}", from.str(), to.str(),
            r.error()
        );
    }

    auto newMetadata = metadata;
    newMetadata.location = destination;
    return newMetadata;
}

}  // namespace anchor
