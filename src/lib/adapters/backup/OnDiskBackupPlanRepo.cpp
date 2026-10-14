#include "OnDiskBackupPlanRepo.hh"

#include <ranges>

#include "base/LibconfigReader.hh"
#include "base/Log.hh"

namespace anchor {

Result<BackupPlan> parseBackupPlan(const Path& path, const FileSystem& fs) {
    auto reader = LibconfigReader::create(path, fs);

    if (not reader) {
        return Error::unexpected(reader.error());
    }

    BackupPlan plan;
    plan.name = path.filename();

    auto destinationStr = reader->lookup<Str>("settings.destination");
    if (not destinationStr) {
        return Error::unexpected(destinationStr.error());
    }

    auto destination = Location::fromStr(destinationStr.value());
    if (not destination) {
        return Error::unexpected(destination.error());
    }
    plan.destination = *destination;

    auto archiveTypeStr = reader->lookup<Str>("settings.archive");
    if (not archiveTypeStr) {
        return Error::unexpected(archiveTypeStr.error());
    }

    auto archiveType = archiveTypeFromStr(archiveTypeStr.value());
    if (not archiveType) {
        return Error::unexpected(archiveType.error());
    }
    plan.archiveType = *archiveType;

    LOOKUP_FIELD(*reader, "filters", plan.filters);

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
    static auto filterNonConf = [](const Path& p) {
        return p.endsWith(".conf");
    };

    auto files = m_fs.listFiles(m_plansPath);
    if (not files) {
        return Error::unexpected(files.error());
    }
    return *files | std::views::filter(filterNonConf) |
           std::ranges::to<std::vector>();
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
