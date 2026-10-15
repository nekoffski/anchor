#pragma once

#include <yaml-cpp/yaml.h>

#include "Concepts.hh"
#include "Core.hh"
#include "FileSystem.hh"

namespace anchor {

class YamlReader : public NonCopyable {
   public:
    static Result<YamlReader> create(
        const Path& path, const FileSystem& fs = FileSystem{}
    );

    template <typename T>
    bool lookup(const Str& key, T& value) const {
        try {
            T converted;
            if (not YAML::convert<T>::decode(find(key), converted)) {
                return false;
            }
            value = std::move(converted);
            return true;
        } catch (const YAML::Exception&) {
            return false;
        }
    }

    bool lookup(const Str& key, Path& path) const;
    bool lookup(const Str& key, std::vector<Str>& value) const;
    bool lookup(const Str& key, std::vector<Path>& value) const;

    template <typename T>
    Result<T> lookup(const Str& key) const {
        T value;
        if (not lookup(key, value)) {
            return Error::unexpected(
                ErrorCode::invalidConfig, "Missing or invalid field: {}", key
            );
        }
        return value;
    }

   private:
    explicit YamlReader(YAML::Node config);
    YAML::Node find(const Str& key) const;

    YAML::Node m_config;
};

}  // namespace anchor

#define LOOKUP_FIELD(config, key, value)                                    \
    if (not(config).lookup((key), (value))) {                               \
        return Error::unexpected(                                           \
            ErrorCode::invalidConfig, "Missing or invalid field: {}", (key) \
        );                                                                  \
    }
