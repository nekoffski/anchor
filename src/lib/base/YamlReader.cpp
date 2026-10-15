#include "YamlReader.hh"

#include "String.hh"

namespace anchor {

Result<YamlReader> YamlReader::create(const Path& path, const FileSystem& fs) {
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
        return YamlReader{documents.front()};
    } catch (const YAML::Exception& e) {
        return Error::unexpected(
            ErrorCode::invalidConfig, "Could not parse: {} - {}", path.str(),
            e.what()
        );
    }
}

YAML::Node YamlReader::find(const Str& key) const {
    auto node = m_config;
    for (const auto& part : split(key, ".")) {
        if (not node.IsMap()) {
            return YAML::Node{YAML::NodeType::Undefined};
        }
        const auto child = std::as_const(node)[part];
        if (not child.IsDefined()) {
            return YAML::Node{YAML::NodeType::Undefined};
        }
        node.reset(child);
    }
    return node;
}

bool YamlReader::lookup(const Str& key, Path& path) const {
    Str value;
    if (not lookup(key, value)) {
        return false;
    }
    path = value;
    return true;
}

bool YamlReader::lookup(const Str& key, std::vector<Str>& value) const {
    const auto node = find(key);
    if (not node.IsSequence()) {
        return false;
    }
    std::vector<Str> values;
    values.reserve(node.size());
    for (const auto& item : node) {
        if (not item.IsScalar()) {
            return false;
        }
        values.push_back(item.Scalar());
    }
    value = std::move(values);
    return true;
}

bool YamlReader::lookup(const Str& key, std::vector<Path>& value) const {
    std::vector<Str> strings;
    if (not lookup(key, strings)) {
        return false;
    }
    value.clear();
    value.reserve(strings.size());
    for (const auto& str : strings) {
        value.emplace_back(str);
    }
    return true;
}

YamlReader::YamlReader(YAML::Node config) : m_config(std::move(config)) {}

}  // namespace anchor
