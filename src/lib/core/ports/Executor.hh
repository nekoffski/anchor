#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Functional.hh"

namespace anchor {

class Executor : public NonMovable, public NonCopyable {
   public:
    virtual ~Executor() = default;

    virtual void call(MoveOnlyFunction<void()> callback) = 0;
};

}  // namespace anchor
