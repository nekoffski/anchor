#pragma once

#include <chrono>
#include <vector>

#include "core/Core.hh"

namespace anchor {

struct SnapshotEntry {
    enum class Type { file, directory, symlink };

    Str path;
    Type type;
    u32 permissions;
    u64 owner;
    u64 group;
    std::chrono::sys_time<std::chrono::nanoseconds> modified;
    u64 size{};
    Str sha256;
    Str linkTarget;
};

struct SnapshotSource {
    u64 id;
    Str root;
    std::vector<SnapshotEntry> entries;
};

struct SnapshotManifest {
    u32 version{1};
    Str name;
    std::chrono::sys_time<std::chrono::nanoseconds> created;
    std::vector<SnapshotSource> sources;
};

}  // namespace anchor
