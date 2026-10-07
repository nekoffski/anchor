#include "LibconfigReader.hh"

namespace anchor {

Result<LibconfigReader> LibconfigReader::create(
    const Path& path, const FileSystem& fs
) {
    if (not fs.exists(path)) {
        return Error::unexpected(
            ErrorCode::fileNotFound, "File not found: {}", path.str()
        );
    }

    auto config = std::make_unique<libconfig::Config>();

    try {
        config->readFile(path.str());
    } catch (const libconfig::ConfigException& e) {
        return Error::unexpected(
            ErrorCode::invalidConfig, "Could not parse: {} - {}", path.str(),
            e.what()
        );
    }

    return LibconfigReader{std::move(config)};
}

bool LibconfigReader::lookup(const Str& key, u16& value) const {
    int v;
    if (not m_config->lookupValue(key.c_str(), v) or v < 0 or
        v > maxOf<u16>()) {
        return false;
    }
    value = static_cast<u16>(v);
    return true;
}

bool LibconfigReader::lookup(const Str& key, u32& value) const {
    unsigned int v;

    if (m_config->lookupValue(key.c_str(), v)) {
        value = static_cast<u32>(v);
        return true;
    }

    return false;
}

bool LibconfigReader::lookup(const Str& key, Path& path) const {
    Str v;
    if (m_config->lookupValue(key.c_str(), v)) {
        path = v;
        return true;
    }
    return false;
}

bool LibconfigReader::lookup(const Str& key, std::vector<Str>& value) const {
    auto& group = m_config->lookup(key.c_str());

    if (not group.isList()) {
        return false;
    }

    value.clear();
    for (int i = 0; i < group.getLength(); ++i) {
        value.push_back(group[i].c_str());
    }
    return true;
}

bool LibconfigReader::lookup(const Str& key, std::vector<Path>& value) const {
    std::vector<Str> strValues;

    if (not lookup(key, strValues)) {
        return false;
    }

    value.clear();
    value.reserve(strValues.size());
    for (const auto& str : strValues) {
        value.push_back(Path{str});
    }
    return true;
}

LibconfigReader::LibconfigReader(std::unique_ptr<libconfig::Config> config)
    : m_config(std::move(config)) {}

}  // namespace anchor
