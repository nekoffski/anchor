#pragma once

#include <chrono>
#include <vector>

#include "Backup.hh"
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
    static constexpr u32 currentVersion = 1;
    static constexpr auto hashAlgorithm = "sha256";

    u32 version{currentVersion};
    std::chrono::sys_time<std::chrono::nanoseconds> created;
    std::vector<SnapshotSource> sources;
    Uuid uuid;
    Str backupName;
    ArchiveType format{ArchiveType::directory};
};

}  // namespace anchor
