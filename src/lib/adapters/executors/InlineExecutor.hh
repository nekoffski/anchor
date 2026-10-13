#pragma once

#include "core/ports/Executor.hh"

namespace anchor {

class InlineExecutor : public Executor {
   public:
    void call(MoveOnlyFunction<void()> callback) override { callback(); }
};

}  // namespace anchor
