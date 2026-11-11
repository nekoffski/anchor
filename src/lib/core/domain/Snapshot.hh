#pragma once

#include "Backup.hh"
#include "Location.hh"
#include "SnapshotManifest.hh"
#include "core/Core.hh"

namespace anchor {

struct SnapshotSummary {
    Uuid uuid;
    std::chrono::sys_time<std::chrono::nanoseconds> created;
    Location location;
};

struct SnapshotMetadata {
    Str name;
    Location location;
    SnapshotManifest manifest;
    Str manifestSha256;
};

}  // namespace anchor
