#include "Thread.hh"

#include <functional>

#include "base/Log.hh"
#include "base/Profiler.hh"

namespace anchor {

Thread::Thread(const Str& ident) : m_ident(ident) {}

Thread::~Thread() { join(); }

void Thread::join() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Thread::join(std::span<Thread*> threads) {
    for (auto& thread : threads) {
        thread->join();
    }
}

void Thread::sleepFor(std::chrono::milliseconds duration) {
    std::this_thread::sleep_for(duration);
}

void Thread::start() {
    m_thread = std::thread([&] { go(); });
}

void Thread::go() {
    ANCHOR_PROFILE_REGISTER_THREAD();
    log::info("{} - thread starting", m_ident);

    try {
        run();
    } catch (const std::exception& e) {
        log::error("{} - standard exception in thread: {}", m_ident, e.what());
        throw;
    } catch (...) {
        log::error("{} - unknown exception in thread", m_ident);
        throw;
    }
    log::info("{} - thread exiting", m_ident);
}

}  // namespace anchor
