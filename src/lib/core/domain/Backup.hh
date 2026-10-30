#pragma once

#include <fmt/core.h>

#include <kcore/Concepts.hh>
#include <kcore/io/FileSystem.hh>

#include "Cron.hh"
#include "Location.hh"
#include "Source.hh"
#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

enum class ArchiveType { directory, zip, tgz };

Result<ArchiveType> archiveTypeFromStr(const Str& str);
Str archiveTypeToStr(ArchiveType type);

struct BackupPlan {
    Str name;
    ArchiveType archiveType;
    Location destination;
    std::vector<Source> sources;
    kc::Opt<Cron> cron;
};

enum class BackupRunState {
    queued,
    starting,
    snapshotting,
    uploading,
    completed,
    failed
};
Str backupRunStateToString(BackupRunState state);
Result<BackupRunState> backupRunStateFromString(const Str& state);
bool isActive(BackupRunState state);

struct BackupRunStatus {
    BackupRunState state;
    kc::Opt<Error> error;
    Uuid uuid;
};

struct Backup {
    BackupPlan plan;
    bool enabled{true};
    kc::Opt<BackupRunStatus> run;
    // snapshots etc..
};

}  // namespace anchor
