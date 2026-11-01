#pragma once

#include <kcore/runtime/ThreadPool.hh>

#include "core/ports/Executor.hh"

namespace anchor {

class ThreadPoolExecutor : public Executor {
   public:
    explicit ThreadPoolExecutor(kc::ThreadPool& tp) : m_tp(tp) {}

    void call(kc::MoveOnlyFunction<void()> callback) override {
        m_tp.submit(std::move(callback));
    }

   private:
    kc::ThreadPool& m_tp;
};

}  // namespace anchor
