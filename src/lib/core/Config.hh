#pragma once

#include <kcore/io/FileSystem.hh>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

struct Config {
    struct Log {
        log::Level level{log::Level::trace};
    };

    struct Api {
        u16 port{4448};
    };

    struct Paths {
        kc::Path workspace;
        kc::Path backupPlans;
    };

    struct System {
        u16 backendConcurrency{4};
    };

    static Result<Config> load(const kc::Path& path);

    Log log;
    Api api;
    Paths paths;
    System system;
};

}  // namespace anchor
