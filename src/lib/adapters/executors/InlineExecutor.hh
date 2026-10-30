#pragma once

#include "core/ports/Executor.hh"

namespace anchor {

class InlineExecutor : public Executor {
   public:
    void call(kc::MoveOnlyFunction<void()> callback) override { callback(); }
};

}  // namespace anchor
