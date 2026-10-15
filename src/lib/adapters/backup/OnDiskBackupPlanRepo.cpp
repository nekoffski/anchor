#include "OnDiskBackupPlanRepo.hh"

#include <ranges>

#include "base/Log.hh"
#include "base/Yaml.hh"

namespace anchor {

Result<BackupPlan> parseBackupPlan(const Path& path, const FileSystem& fs) {
    const auto file = loadYaml(path, fs);

    if (not file) {
        return Error::unexpected(file.error());
    }

    BackupPlan plan;
    plan.name = path.filename();

    const auto settings = (*file)["settings"];
    auto destinationStr = yamlField<Str>(settings, "destination");
    if (not destinationStr) {
        return Error::unexpected(destinationStr.error());
    }

    auto destination = Location::fromStr(destinationStr.value());
    if (not destination) {
        return Error::unexpected(destination.error());
    }
    plan.destination = *destination;

    auto archiveTypeStr = yamlField<Str>(settings, "archive");
    if (not archiveTypeStr) {
        return Error::unexpected(archiveTypeStr.error());
    }

    auto archiveType = archiveTypeFromStr(archiveTypeStr.value());
    if (not archiveType) {
        return Error::unexpected(archiveType.error());
    }
    plan.archiveType = *archiveType;

    const auto filters = (*file)["filters"];
    if (not filters.IsDefined() or not filters.IsSequence()) {
        return Error::unexpected(
            ErrorCode::invalidConfig, "Missing or invalid field: filters"
        );
    }
    for (const auto& filter : filters) {
        if (not filter.IsScalar()) {
            return Error::unexpected(
                ErrorCode::invalidConfig, "Expected a scalar in filters"
            );
        }
        plan.filters.emplace_back(filter.Scalar());
    }

    return plan;
}

OnDiskBackupPlanRepo::OnDiskBackupPlanRepo(
    const Path& plansPath, const FileSystem& fs
)
    : m_plansPath(plansPath), m_fs(fs) {}

Result<void> OnDiskBackupPlanRepo::valid() const {
    if (not m_fs.isDirectory(m_plansPath)) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Plans path is not a directory"
        );
    }
    return {};
}

Result<std::vector<Path>> OnDiskBackupPlanRepo::list() const {
    static auto isYaml = [](const Path& p) {
        return p.endsWith(".yaml") or p.endsWith(".yml");
    };

    auto files = m_fs.listFiles(m_plansPath);
    if (not files) {
        return Error::unexpected(files.error());
    }
    return *files | std::views::filter(isYaml) | std::ranges::to<std::vector>();
}

void OnDiskBackupPlanRepo::forEach(Callback callback) const {
    const auto& paths = list();
    if (not paths) {
        log::error("Could not list backup plans: {}", paths.error());
        return;
    }

    for (const auto& path : *paths) {
        log::debug("Parsing backup plan: {}", path.str());

        if (auto plan = parseBackupPlan(path, m_fs); plan) {
            callback(*plan);
        } else {
            log::error(
                "Could not load backup plan: {} due to {}", path.str(),
                plan.error()
            );
        }
    }
}

}  // namespace anchor
