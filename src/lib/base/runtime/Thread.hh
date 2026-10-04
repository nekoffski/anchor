#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor {

class Thread : public NonCopyable, public NonMovable {
   public:
    explicit Thread(const Str& ident);
    virtual ~Thread();

    void start();
    void join();

    static void join(std::span<Thread*> threads);

    template <typename... Args>
        requires(std::derived_from<Args, Thread> && ...)
    static void join(Args&... threads) {
        (threads.join(), ...);
    }

   protected:
    void sleepFor(std::chrono::milliseconds duration);

   private:
    virtual void run() = 0;
    void go();

    Str m_ident;
    std::thread m_thread;
};

}  // namespace anchor
