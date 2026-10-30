#include "OnDiskBackupPlanRepo.hh"

#include <kcore/Log.hh>
#include <kcore/serialization/Yaml.hh>
#include <ranges>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

Result<BackupPlan> parseBackupPlan(
    const kc::Path& path, const kc::FileSystem& fs
) {
    const auto file = kc::loadYaml(path, fs);

    if (not file) {
        return Error::unexpected(file.error());
    }

    BackupPlan plan;
    plan.name = path.filename();

    const auto settings = (*file)["settings"];
    auto destinationStr = kc::yamlField<Str>(settings, "destination");
    if (not destinationStr) {
        return Error::unexpected(destinationStr.error());
    }

    auto destination = Location::fromStr(destinationStr.value());
    if (not destination) {
        return Error::unexpected(destination.error());
    }
    plan.destination = *destination;

    auto archiveTypeStr = kc::yamlField<Str>(settings, "archive");
    if (not archiveTypeStr) {
        return Error::unexpected(archiveTypeStr.error());
    }

    auto archiveType = archiveTypeFromStr(archiveTypeStr.value());
    if (not archiveType) {
        return Error::unexpected(archiveType.error());
    }
    plan.archiveType = *archiveType;

    if (settings["cron"].IsDefined()) {
        auto expression = kc::yamlField<Str>(settings, "cron");
        if (not expression) {
            return Error::unexpected(expression.error());
        }
        auto cron = Cron::fromString(*expression);
        if (not cron) {
            return Error::unexpected(
                kc::ErrorCode::invalidConfig, cron.error().message()
            );
        }
        plan.cron = std::move(*cron);
    }

    const auto sources = (*file)["sources"];
    if (not sources.IsDefined() or not sources.IsSequence()) {
        return Error::unexpected(
            kc::ErrorCode::invalidConfig, "Missing or invalid field: sources"
        );
    }

    for (const auto& source : sources) {
        if (not source.IsDefined()) {
            return Error::unexpected(
                kc::ErrorCode::invalidConfig, "Invalid source in sources"
            );
        }
        if (not source.IsMap()) {
            return Error::unexpected(
                kc::ErrorCode::invalidConfig, "Expected a map in sources"
            );
        }

        const auto& root = source["root"];

        if (not root.IsDefined() || not root.IsScalar()) {
            return Error::unexpected(
                kc::ErrorCode::invalidConfig, "Missing or invalid field: root"
            );
        }

        const auto& includes = source["includes"];

        if (not includes.IsDefined() || not includes.IsSequence()) {
            return Error::unexpected(
                kc::ErrorCode::invalidConfig,
                "Missing or invalid field: includes"
            );
        }

        Source src;
        src.root = kc::Path{root.Scalar()}.resolveEnv();

        for (const auto& include : includes) {
            if (not include.IsScalar()) {
                return Error::unexpected(
                    kc::ErrorCode::invalidConfig,
                    "Expected a scalar in includes"
                );
            }
            src.includes.emplace_back(include.Scalar());
        }
        plan.sources.emplace_back(src);
    }

    return plan;
}

OnDiskBackupPlanRepo::OnDiskBackupPlanRepo(
    const kc::Path& plansPath, const kc::FileSystem& fs
)
    : m_plansPath(plansPath), m_fs(fs) {}

Result<void> OnDiskBackupPlanRepo::valid() const {
    if (not m_fs.isDirectory(m_plansPath)) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Plans path is not a directory"
        );
    }
    return {};
}

Result<std::vector<kc::Path>> OnDiskBackupPlanRepo::list() const {
    static auto isYaml = [](const kc::Path& p) {
        return p.endsWith(".yaml") or p.endsWith(".yml");
    };

    auto files = m_fs.listFiles(m_plansPath);
    if (not files) {
        return Error::unexpected(files.error());
    }
    return *files | std::views::filter(isYaml) | std::ranges::to<std::vector>();
}

Result<void> OnDiskBackupPlanRepo::forEach(Callback callback) const {
    const auto& paths = list();
    if (not paths) {
        kc::log::error("Could not list backup plans: {}", paths.error());
        return Error::unexpected(paths.error());
    }

    kc::Opt<Error> failure;
    for (const auto& path : *paths) {
        kc::log::debug("Parsing backup plan: {}", path.str());

        if (auto plan = parseBackupPlan(path, m_fs); plan) {
            callback(*plan);
        } else {
            kc::log::error(
                "Could not load backup plan: {} due to {}", path.str(),
                plan.error()
            );
            if (not failure) {
                failure = Error{
                    plan.error().code(), "Could not load plan '{}': {}",
                    path.str(), plan.error().message()
                };
            }
        }
    }
    if (failure) {
        return Error::unexpected(*failure);
    }
    return {};
}

}  // namespace anchor
