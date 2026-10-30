#pragma once

#include <kcore/Concepts.hh>
#include <kcore/containers/InterfaceMap.hh>
#include <kcore/io/FileSystem.hh>
#include <unordered_map>

#include "core/Core.hh"
#include "core/Error.hh"
#include "core/domain/Backup.hh"
#include "core/domain/Snapshot.hh"

namespace anchor {

struct SnapshotArgs {
    const Str& name;
    std::span<const SourceTree> trees;
    const kc::Path& workspace;
};

class Snapshotter : public kc::NonCopyable, public kc::NonMovable {
   public:
    virtual ~Snapshotter() = default;

    virtual Result<SnapshotMetadata> snapshot(const SnapshotArgs& args) = 0;
};

using Snapshotters = kc::InterfaceMap<ArchiveType, Snapshotter>;

class SnapshotStore : public kc::NonCopyable, public kc::NonMovable {
   public:
    virtual ~SnapshotStore() = default;

    virtual Result<SnapshotMetadata> put(
        const SnapshotMetadata& metadata, const Location& destination
    ) = 0;
};

using SnapshotStores = kc::InterfaceMap<LocationType, SnapshotStore>;

}  // namespace anchor
