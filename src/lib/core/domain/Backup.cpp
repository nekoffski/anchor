#include "Backup.hh"

#include "base/String.hh"

namespace anchor {

Str backupStatusToString(BackupStatus status) {
    switch (status) {
        case BackupStatus::ready:
            return "ready";
        case BackupStatus::inProgress:
            return "inProgress";
        case BackupStatus::failed:
            return "failed";
    }
    std::unreachable();
}

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

Result<ArchiveType> archiveTypeFromStr(const Str& str) {
    if (str == "directory") {
        return ArchiveType::directory;
    } else if (str == "zip") {
        return ArchiveType::zip;
    } else if (str == "rar") {
        return ArchiveType::rar;
    }
    return Error::unexpected(
        ErrorCode::invalidArgument, "Invalid snapshot type: {}", str
    );
}

Str archiveTypeToStr(ArchiveType type) {
    switch (type) {
        case ArchiveType::directory:
            return "directory";
        case ArchiveType::zip:
            return "zip";
        case ArchiveType::rar:
            return "rar";
    }
    std::unreachable();
}

}  // namespace anchor
