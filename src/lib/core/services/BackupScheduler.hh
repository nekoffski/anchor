#pragma once

#include <chrono>
#include <condition_variable>
#include <kcore/runtime/Thread.hh>
#include <mutex>

#include "RequestQueue.hh"
#include "core/Core.hh"
#include "core/ports/Backup.hh"

namespace anchor {

class BackupScheduler : public kc::Thread {
   public:
    explicit BackupScheduler(const BackupRepo& repo, RequestSender sender);
    ~BackupScheduler() override;

    void stop();
    void add(
        const Str& name,
        std::chrono::sys_seconds now = std::chrono::floor<std::chrono::seconds>(
            std::chrono::system_clock::now()
        )
    );
    // Called by the worker, or directly without starting the thread.
    void tick(std::chrono::sys_seconds now);

   private:
    struct Deadline {
        Str name;
        std::chrono::sys_seconds at;
    };

    void run() override;
    void scheduleNext(const Str& name, std::chrono::sys_seconds now);
    void rebuildDeadlines(std::chrono::sys_seconds now);
    static bool shouldSchedule(
        const Backup& backup, std::chrono::sys_seconds now
    );

    const BackupRepo& m_repo;
    RequestSender m_sender;
    // A heap vector allows replacing deadlines without retaining stale entries.
    std::vector<Deadline> m_deadlines;
    kc::Opt<std::chrono::sys_seconds> m_lastTick;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    u64 m_revision{0};
    bool m_stopped{false};
};

}  // namespace anchor
