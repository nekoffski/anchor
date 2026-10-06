#include "Backup.hh"

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

}  // namespace anchor
