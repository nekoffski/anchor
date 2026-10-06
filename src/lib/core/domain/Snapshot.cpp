#include "Snapshot.hh"

#include "base/String.hh"

namespace anchor {

Result<Destination> Destination::fromStr(const Str& str) {
    static const Str separator = "://";
    auto parts = split(str, separator);

    if (parts.size() != 2) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid destination format"
        );
    }

    auto type = destinationTypeFromStr(parts[0]);

    if (not type) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid destination type: {}", parts[0]
        );
    }

    return Destination{.type = *type, .path = parts[1]};
}

Str Destination::str() const {
    return fmt::format("{}://{}", destinationTypeToStr(type), path);
}

Result<DestinationType> destinationTypeFromStr(const Str& str) {
    if (str == "local") {
        return DestinationType::local;
    } else if (str == "s3") {
        return DestinationType::s3;
    }
    return Error::unexpected(
        ErrorCode::invalidArgument, "Invalid destination type: {}", str
    );
}

Str destinationTypeToStr(DestinationType type) {
    switch (type) {
        case DestinationType::local:
            return "local";
        case DestinationType::s3:
            return "s3";
    }
    std::unreachable();
}

Result<SnapshotType> snapshotTypeFromStr(const Str& str) {
    if (str == "directory") {
        return SnapshotType::directory;
    } else if (str == "zip") {
        return SnapshotType::zip;
    } else if (str == "rar") {
        return SnapshotType::rar;
    }
    return Error::unexpected(
        ErrorCode::invalidArgument, "Invalid snapshot type: {}", str
    );
}

Str snapshotTypeToStr(SnapshotType type) {
    switch (type) {
        case SnapshotType::directory:
            return "directory";
        case SnapshotType::zip:
            return "zip";
        case SnapshotType::rar:
            return "rar";
    }
    std::unreachable();
}

}  // namespace anchor