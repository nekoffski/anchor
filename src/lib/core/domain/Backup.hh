#pragma once

#include <fmt/core.h>

#include "Snapshot.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/FileSystem.hh"

namespace anchor {

struct BackupPlan {
    Str name;
    SnapshotConfig snapshot;
    std::vector<Path> filters;
};

enum class BackupStatus { ready, inProgress, failed };
Str backupStatusToString(BackupStatus status);

struct Backup {
    BackupPlan plan;
    BackupStatus status{BackupStatus::ready};
    bool enabled{true};
};

}  // namespace anchor
