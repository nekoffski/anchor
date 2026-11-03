#pragma once

#include "core/Error.hh"
#include "core/domain/SnapshotManifest.hh"

namespace anchor {

Result<Str> serializeSnapshotManifest(const SnapshotManifest& manifest);

}  // namespace anchor
