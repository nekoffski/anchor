#pragma once

#include "Core.hh"
#include "Error.hh"
#include "FileSystem.hh"
#include "Log.hh"

namespace anchor {

struct Config {
    struct Log {
        log::Level level{log::Level::trace};
    };

    struct Api {
        u16 port{4448};
    };

    struct Paths {
        Path workspace;
        Path backupPlans;
    };

    static Result<Config> load(const Path& path);

    Log log;
    Api api;
    Paths paths;
};

}  // namespace anchor
