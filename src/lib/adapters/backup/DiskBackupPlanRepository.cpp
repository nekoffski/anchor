#include "DiskBackupPlanRepository.hh"

#include <ranges>

#include "base/LibconfigReader.hh"
#include "base/Log.hh"

namespace anchor {

Result<SnapshotConfig> parseSnapshotConfig(const LibconfigReader& reader) {
    auto snapshotTypeStr = reader.lookup<Str>("settings.snapshot.type");

    if (not snapshotTypeStr) {
        return Error::unexpected(snapshotTypeStr.error());
    }

    auto snapshotType = snapshotTypeFromStr(snapshotTypeStr.value());

    if (not snapshotType) {
        return Error::unexpected(snapshotType.error());
    }

    auto destinationStr = reader.lookup<Str>("settings.snapshot.destination");

    if (not destinationStr) {
        return Error::unexpected(destinationStr.error());
    }

    auto destination = Destination::fromStr(destinationStr.value());
    if (not destination) {
        return Error::unexpected(destination.error());
    }

    return SnapshotConfig{.type = *snapshotType, .destination = *destination};
}

Result<BackupPlan> parseBackupPlan(const Path& path, const FileSystem& fs) {
    auto reader = LibconfigReader::create(path, fs);

    if (not reader) {
        return Error::unexpected(reader.error());
    }

    BackupPlan plan;
    plan.name = path.filename();

    auto snapshotConfig = parseSnapshotConfig(*reader);

    if (not snapshotConfig) {
        return Error::unexpected(snapshotConfig.error());
    }

    plan.snapshot = std::move(*snapshotConfig);
    return plan;
}

DiskBackupPlanRepository::DiskBackupPlanRepository(
    const Path& plansPath, const FileSystem& fs
)
    : m_plansPath(plansPath), m_fs(fs) {}

Result<void> DiskBackupPlanRepository::valid() const {
    if (not m_fs.isDirectory(m_plansPath)) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Plans path is not a directory"
        );
    }
    return {};
}

std::vector<Path> DiskBackupPlanRepository::list() const {
    static auto filterNonConf = [](const Path& p) {
        return p.endsWith(".conf");
    };

    return m_fs.listFiles(m_plansPath) | std::views::filter(filterNonConf) |
           std::ranges::to<std::vector>();
}

std::vector<BackupPlan> DiskBackupPlanRepository::getAll() const {
    const auto& paths = list();
    std::vector<BackupPlan> plans;
    plans.reserve(paths.size());

    for (const auto& path : paths) {
        log::debug("Parsing backup plan: {}", path.str());

        if (auto plan = parseBackupPlan(path, m_fs); plan) {
            plans.push_back(*plan);
        } else {
            log::error(
                "Could not load backup plan: {} due to {}", path.str(),
                plan.error()
            );
        }
    }

    return plans;
}

}  // namespace anchor
