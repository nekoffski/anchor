#pragma once

#include <kcore/serialization/Json.hh>

#include "core/Error.hh"
#include "core/domain/Snapshot.hh"

namespace anchor {

struct SnapshotManifestHeader {
    Uuid uuid;
    Str backupName;
    ArchiveType format;
    std::chrono::sys_time<std::chrono::nanoseconds> created;
};

Result<Str> serializeSnapshotManifest(const SnapshotManifest& manifest);
Result<SnapshotManifestHeader> parseSnapshotManifestHeader(const Str& contents);

kc::Json serializeSnapshotSummary(const SnapshotSummary& snapshot);
Result<SnapshotSummary> parseSnapshotSummary(const kc::Json& json);

}  // namespace anchor
