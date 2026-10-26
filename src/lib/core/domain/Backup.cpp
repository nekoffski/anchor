#include "Backup.hh"

namespace anchor {

bool isActive(BackupRunState state) {
    return state != BackupRunState::completed &&
           state != BackupRunState::failed;
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
        ErrorCode::invalidArgument, "Invalid snapshot type: {}", str
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
