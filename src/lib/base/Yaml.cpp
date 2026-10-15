#include "Yaml.hh"

namespace anchor {

Result<YAML::Node> loadYaml(const Path& path, const FileSystem& fs) {
    if (not fs.exists(path)) {
        return Error::unexpected(
            ErrorCode::fileNotFound, "File not found: {}", path.str()
        );
    }
    auto content = fs.readFile(path);
    if (not content) {
        return Error::unexpected(content.error());
    }
    try {
        auto documents = YAML::LoadAll(*content);
        if (documents.size() != 1 or not documents.front().IsMap()) {
            return Error::unexpected(
                ErrorCode::invalidConfig, "Expected one YAML mapping in: {}",
                path.str()
            );
        }
        return documents.front();
    } catch (const YAML::Exception& e) {
        return Error::unexpected(
            ErrorCode::invalidConfig, "Could not parse: {} - {}", path.str(),
            e.what()
        );
    }
}

}  // namespace anchor
