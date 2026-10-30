#include "Backup.hh"

#include <array>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

bool isActive(BackupRunState state) {
    return state != BackupRunState::completed &&
           state != BackupRunState::failed;
}

Result<BackupRunState> backupRunStateFromString(const Str& state) {
    static constexpr std::array states{
        BackupRunState::queued,       BackupRunState::starting,
        BackupRunState::snapshotting, BackupRunState::uploading,
        BackupRunState::completed,    BackupRunState::failed
    };
    for (const auto value : states) {
        if (backupRunStateToString(value) == state) {
            return value;
        }
    }
    return Error::unexpected(
        kc::ErrorCode::invalidArgument, "Invalid backup run state: {}", state
    );
}

Str backupRunStateToString(BackupRunState state) {
    switch (state) {
        case BackupRunState::queued:
            return "queued";
        case BackupRunState::starting:
            return "starting";
        case BackupRunState::snapshotting:
            return "snapshotting";
        case BackupRunState::uploading:
            return "uploading";
        case BackupRunState::completed:
            return "completed";
        case BackupRunState::failed:
            return "failed";
    }
    std::unreachable();
}

Result<ArchiveType> archiveTypeFromStr(const Str& str) {
    if (str == "directory") {
        return ArchiveType::directory;
    } else if (str == "zip") {
        return ArchiveType::zip;
    } else if (str == "tgz") {
        return ArchiveType::tgz;
    }
    return Error::unexpected(
        kc::ErrorCode::invalidArgument, "Invalid snapshot type: {}", str
    );
}

Str archiveTypeToStr(ArchiveType type) {
    switch (type) {
        case ArchiveType::directory:
            return "directory";
        case ArchiveType::zip:
            return "zip";
        case ArchiveType::tgz:
            return "tgz";
    }
    std::unreachable();
}

}  // namespace anchor
