#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"

namespace anchor {

class Snapshotter : public NonCopyable, public NonMovable {
   public:
    virtual ~Snapshotter() = default;
};

}  // namespace anchor
