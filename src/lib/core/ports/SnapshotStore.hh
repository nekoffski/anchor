#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"

namespace anchor {

class SnapshotStore : public NonCopyable, public NonMovable {
   public:
    ~SnapshotStore() = default;
};

}  // namespace anchor
