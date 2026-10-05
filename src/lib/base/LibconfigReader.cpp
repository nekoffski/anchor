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

bool LibconfigReader::lookup(const Str& key, u16& value) {
    int v;
    if (not m_config->lookupValue(key.c_str(), v) or v < 0 or
        v > maxOf<u16>()) {
        return false;
    }
    value = static_cast<u16>(v);
    return true;
}

bool LibconfigReader::lookup(const Str& key, u32& value) {
    unsigned int v;

    if (m_config->lookupValue(key.c_str(), v)) {
        value = static_cast<u32>(v);
        return true;
    }

    return false;
}

bool LibconfigReader::lookup(const Str& key, Path& path) {
    Str v;
    if (m_config->lookupValue(key.c_str(), v)) {
        path = v;
        return true;
    }
    return false;
}

LibconfigReader::LibconfigReader(std::unique_ptr<libconfig::Config> config)
    : m_config(std::move(config)) {}

}  // namespace anchor
