#pragma once

#include <kcore/Log.hh>
#include <kcore/io/FileSystem.hh>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

struct Config {
    struct Log {
        kc::log::Level level{kc::log::Level::trace};
    };

    struct Api {
        u16 port{4448};
    };

    struct Paths {
        kc::Path workspace;
        kc::Path backupPlans;
    };

    static Result<Config> load(const kc::Path& path);

    Log log;
    Api api;
    Paths paths;
};

}  // namespace anchor
