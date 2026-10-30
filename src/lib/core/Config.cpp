#include "Config.hh"

#include <kcore/serialization/Yaml.hh>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

Result<Config> Config::load(const kc::Path& path) {
    const auto file = kc::loadYaml(path);

    if (not file) {
        return Error::unexpected(file.error());
    }

    const auto api = (*file)["api"];
    auto port = kc::yamlField<u16>(api, "port");
    if (not port) {
        return Error::unexpected(port.error());
    }

    const auto paths = (*file)["paths"];
    auto workspace = kc::yamlField<Str>(paths, "workspace");
    if (not workspace) {
        return Error::unexpected(workspace.error());
    }
    auto backupPlans = kc::yamlField<Str>(paths, "backup_plans");
    if (not backupPlans) {
        return Error::unexpected(backupPlans.error());
    }

    Config config;
    config.api.port = *port;
    config.paths.workspace =
        kc::Path::join(path.parent(), kc::Path{*workspace}.resolveEnv());
    config.paths.backupPlans =
        kc::Path::join(path.parent(), kc::Path{*backupPlans}.resolveEnv());

    return config;
}

}  // namespace anchor
