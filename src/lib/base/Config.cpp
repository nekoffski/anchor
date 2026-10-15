#include "Config.hh"

#include "Yaml.hh"

namespace anchor {

Result<Config> Config::load(const Path& path) {
    const auto file = loadYaml(path);

    if (not file) {
        return Error::unexpected(file.error());
    }

    const auto api = (*file)["api"];
    auto port = yamlField<u16>(api, "port");
    if (not port) {
        return Error::unexpected(port.error());
    }

    const auto paths = (*file)["paths"];
    auto workspace = yamlField<Str>(paths, "workspace");
    if (not workspace) {
        return Error::unexpected(workspace.error());
    }
    auto backupPlans = yamlField<Str>(paths, "backup_plans");
    if (not backupPlans) {
        return Error::unexpected(backupPlans.error());
    }

    Config config;
    config.api.port = *port;
    config.paths.workspace =
        Path::join(path.parent(), Path{*workspace}.resolveEnv());
    config.paths.backupPlans =
        Path::join(path.parent(), Path{*backupPlans}.resolveEnv());

    return config;
}

}  // namespace anchor
