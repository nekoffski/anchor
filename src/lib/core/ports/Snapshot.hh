#pragma once

#include <unordered_map>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/FileSystem.hh"
#include "base/InterfaceMap.hh"
#include "core/domain/Backup.hh"
#include "core/domain/Snapshot.hh"

namespace anchor {

struct SnapshotArgs {
    const Str& name;
    std::span<const Filter> filters;
    const Path& workspace;
};

class Snapshotter : public NonCopyable, public NonMovable {
   public:
    virtual ~Snapshotter() = default;

    virtual Result<SnapshotMetadata> snapshot(const SnapshotArgs& args) = 0;
};

using Snapshotters = InterfaceMap<ArchiveType, Snapshotter>;

class SnapshotStore : public NonCopyable, public NonMovable {
   public:
    virtual ~SnapshotStore() = default;

    virtual Result<SnapshotMetadata> put(
        const SnapshotMetadata& metadata, const Location& destination
    ) = 0;
};

using SnapshotStores = InterfaceMap<LocationType, SnapshotStore>;

}  // namespace anchor
