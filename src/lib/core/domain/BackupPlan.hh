#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/FileSystem.hh"

namespace anchor {

struct BackupPlan {
    Str name;
    Path destination;
    std::vector<Path> sources;
};

}  // namespace anchor
