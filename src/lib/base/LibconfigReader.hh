#pragma once

#include <libconfig.h++>
#include <memory>

#include "Concepts.hh"
#include "Core.hh"
#include "FileSystem.hh"

namespace anchor {

class LibconfigReader : public NonCopyable {
   public:
    static Result<LibconfigReader> create(
        const Path& path, const FileSystem& fs = FileSystem{}
    );

    template <typename T>
    bool lookup(const Str& key, T& value) {
        return m_config->lookupValue(key.c_str(), value);
    }

    bool lookup(const Str& key, u16& value);
    bool lookup(const Str& key, u32& value);
    bool lookup(const Str& key, Path& path);

   private:
    explicit LibconfigReader(std::unique_ptr<libconfig::Config> config);

    std::unique_ptr<libconfig::Config> m_config;
};

}  // namespace anchor

#define LOOKUP_FIELD(config, key, value)                         \
    if (not(config).lookup((key), (value))) {                    \
        return Error::unexpected(                                \
            ErrorCode::invalidConfig, "Missing field: {}", (key) \
        );                                                       \
    }
