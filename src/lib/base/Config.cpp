#include "Config.hh"

#include "LibconfigReader.hh"

namespace anchor {

Result<Config> Config::load(const Path& path) {
    auto file = LibconfigReader::create(path);

    if (not file) {
        return Error::unexpected(file.error());
    }

    Config config;

    LOOKUP_FIELD(*file, "api.port", config.api.port);
    LOOKUP_FIELD(*file, "paths.workspace", config.paths.workspace);
    LOOKUP_FIELD(*file, "paths.backup_plans", config.paths.backupPlans);

    config.paths.workspace =
        Path::join(path.parent(), config.paths.workspace.resolveEnv());
    config.paths.backupPlans =
        Path::join(path.parent(), config.paths.backupPlans.resolveEnv());

    return config;
}

}  // namespace anchor
