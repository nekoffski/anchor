#include "BackupScheduler.hh"

#include <algorithm>
#include <functional>

#include "base/Log.hh"

namespace anchor {

BackupScheduler::BackupScheduler(const BackupRepo& repo, RequestSender sender)
    : Thread("BackupScheduler"), m_repo(repo), m_sender(sender) {}

BackupScheduler::~BackupScheduler() {
    stop();
    join();
}

void BackupScheduler::stop() {
    {
        std::lock_guard lock{m_mutex};
        m_stopped = true;
    }
    m_condition.notify_all();
}

void BackupScheduler::add(const Str& name, std::chrono::sys_seconds now) {
    {
        std::lock_guard lock{m_mutex};
        if (m_stopped) {
            return;
        }
        const auto hasSameName = [&name](const Deadline& deadline) {
            return deadline.name == name;
        };
        std::erase_if(m_deadlines, hasSameName);
        std::ranges::make_heap(m_deadlines, std::greater{}, &Deadline::at);
        scheduleNext(name, now);
        ++m_revision;
    }
    m_condition.notify_one();
}

void BackupScheduler::scheduleNext(
    const Str& name, std::chrono::sys_seconds now
) {
    const auto backup = m_repo.get(name);
    if (not backup || not backup->plan.cron) {
        return;
    }
    if (const auto next = backup->plan.cron->nextAfter(now)) {
        m_deadlines.push_back(Deadline{.name = name, .at = *next});
        std::ranges::push_heap(m_deadlines, std::greater{}, &Deadline::at);
    } else {
        log::warn("No future cron occurrence for backup '{}'", name);
    }
}

void BackupScheduler::rebuildDeadlines(std::chrono::sys_seconds now) {
    const auto previous = std::move(m_deadlines);
    m_deadlines.clear();
    for (const auto& deadline : previous) {
        scheduleNext(deadline.name, now);
    }
    m_lastTick = now;
}

bool BackupScheduler::shouldSchedule(
    const Backup& backup, std::chrono::sys_seconds now
) {
    return backup.enabled && backup.plan.cron &&
           (not backup.run || not isActive(backup.run->state)) &&
           backup.plan.cron->matches(now);
}

void BackupScheduler::tick(std::chrono::sys_seconds now) {
    std::vector<Str> due;
    {
        std::lock_guard lock{m_mutex};
        if (m_stopped) {
            return;
        }
        if (m_lastTick && now < *m_lastTick) {
            rebuildDeadlines(now);
        }
        m_lastTick = now;
        while (not m_deadlines.empty() && m_deadlines.front().at <= now) {
            std::ranges::pop_heap(m_deadlines, std::greater{}, &Deadline::at);
            const auto deadline = m_deadlines.back();
            m_deadlines.pop_back();
            const auto backup = m_repo.get(deadline.name);
            if (deadline.at == now && backup && shouldSchedule(*backup, now)) {
                due.push_back(deadline.name);
            }
            scheduleNext(deadline.name, now);
        }
    }
    for (const auto& name : due) {
        auto request = TriggerBackupRequest{.name = name, .scheduled = true};
        if (auto result = m_sender.send(std::move(request)); not result) {
            log::error("Could not submit scheduled backup: {}", result.error());
        }
    }
}

void BackupScheduler::run() {
    std::unique_lock lock{m_mutex};

    if (m_stopped) {
        return;
    }

    const auto startedAt = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now()
    );

    rebuildDeadlines(startedAt);

    const auto hasWorkOrStopped = [this] {
        return m_stopped || not m_deadlines.empty();
    };

    while (not m_stopped) {
        if (m_deadlines.empty()) {
            m_condition.wait(lock, hasWorkOrStopped);
            continue;
        }

        const auto revision = m_revision;
        const auto deadline = m_deadlines.front().at;
        const auto scheduleChangedOrStopped = [this, revision] {
            return m_stopped || m_revision != revision;
        };

        if (m_condition.wait_until(lock, deadline, scheduleChangedOrStopped)) {
            continue;
        }

        lock.unlock();
        const auto now = std::chrono::floor<std::chrono::seconds>(
            std::chrono::system_clock::now()
        );
        tick(now);
        lock.lock();
    }
}

}  // namespace anchor
