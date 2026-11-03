#pragma once

#include "Location.hh"
#include "SnapshotManifest.hh"
#include "core/Core.hh"

namespace anchor {

struct SnapshotMetadata {
    Str name;
    Location location;
    SnapshotManifest manifest;
    Str manifestSha256;
};

}  // namespace anchor
