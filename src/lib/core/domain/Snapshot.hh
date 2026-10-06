#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor {

enum class DestinationType { local, s3 };

Result<DestinationType> destinationTypeFromStr(const Str& str);
Str destinationTypeToStr(DestinationType type);

struct Destination {
    DestinationType type;
    Str path;

    static Result<Destination> fromStr(const Str& str);
    Str str() const;
};

enum class SnapshotType { directory, zip, rar };

Result<SnapshotType> snapshotTypeFromStr(const Str& str);
Str snapshotTypeToStr(SnapshotType type);

struct SnapshotConfig {
    SnapshotType type;
    Destination destination;
};

}  // namespace anchor
