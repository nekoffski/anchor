#pragma once

#include <fmt/core.h>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/FileSystem.hh"

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

enum class ArchiveType { directory, zip, rar };

Result<ArchiveType> archiveTypeFromStr(const Str& str);
Str archiveTypeToStr(ArchiveType type);

using Filter = Path;

struct BackupPlan {
    Str name;
    ArchiveType archiveType;
    Destination destination;
    std::vector<Filter> filters;
};

enum class BackupStatus { ready, inProgress, failed };
Str backupStatusToString(BackupStatus status);

struct Backup {
    BackupPlan plan;
    BackupStatus status{BackupStatus::ready};
    bool enabled{true};
};

}  // namespace anchor
