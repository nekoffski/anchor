#pragma once

#include <fmt/core.h>

#include "Location.hh"
#include "Source.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/FileSystem.hh"

namespace anchor {

enum class ArchiveType { directory, zip, tgz };

Result<ArchiveType> archiveTypeFromStr(const Str& str);
Str archiveTypeToStr(ArchiveType type);

struct BackupPlan {
    Str name;
    ArchiveType archiveType;
    Location destination;
    std::vector<Source> sources;
};

enum class BackupRunState {
    starting,
    snapshotting,
    uploading,
    completed,
    failed
};
Str backupRunStateToString(BackupRunState state);

struct BackupRunStatus {
    BackupRunState state;
    Opt<Error> error;
};

struct Backup {
    BackupPlan plan;
    bool enabled{true};
    // snapshots etc..
};

}  // namespace anchor
