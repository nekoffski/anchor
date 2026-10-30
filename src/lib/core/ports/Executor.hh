#pragma once

#include <kcore/Concepts.hh>
#include <kcore/Functional.hh>

#include "core/Core.hh"

namespace anchor {

class Executor : public kc::NonMovable, public kc::NonCopyable {
   public:
    virtual ~Executor() = default;

    virtual void call(kc::MoveOnlyFunction<void()> callback) = 0;
};

}  // namespace anchor
