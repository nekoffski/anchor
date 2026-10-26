#include "Cron.hh"

#include <algorithm>

#include "base/Log.hh"

namespace anchor {
namespace {

Opt<std::chrono::sys_seconds> nextCandidate(
    const cron::cronexpr& expression, std::chrono::sys_seconds time
) {
    const auto native = std::chrono::system_clock::to_time_t(time);
    const auto next = cron::cron_next(expression, native);
    if (next == cron::INVALID_TIME) {
        return {};
    }
    return std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::from_time_t(next)
    );
}

Opt<std::chrono::sys_seconds> searchAfterDstGap(std::chrono::sys_seconds time) {
    const auto native = std::chrono::system_clock::to_time_t(time);
    std::tm date{};
    if (not cron::utils::time_to_tm(&native, &date)) {
        return {};
    }
    ++date.tm_mday;
    date.tm_hour = 0;
    date.tm_min = 0;
    date.tm_sec = 0;
    date.tm_isdst = -1;
    const auto midnight = std::mktime(&date);
    if (midnight == cron::INVALID_TIME || midnight <= native) {
        return {};
    }
    const auto nextDay = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::from_time_t(midnight)
    );
    // Resume before midnight so a matching midnight is included.
    return std::max(
        time + std::chrono::seconds{1}, nextDay - std::chrono::seconds{1}
    );
}

}  // namespace

Result<Cron> Cron::fromString(const Str& cron) {
    try {
        return Cron{cron::make_cron(cron)};
    } catch (const cron::bad_cronexpr& e) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid cron expression: {}", e.what()
        );
    }
}

Cron::Cron(cron::cronexpr&& expr) : m_expr(std::move(expr)) {}

Opt<std::chrono::sys_seconds> Cron::nextAfter(
    std::chrono::sys_seconds time
) const {
    try {
        auto search = Opt{time};
        while (search) {
            const auto candidate = nextCandidate(m_expr, *search);
            if (not candidate) {
                return {};
            }
            if (*candidate > time && matches(*candidate)) {
                return candidate;
            }
            // croncpp can move backwards when the next local time is missing.
            search =
                *candidate > *search ? candidate : searchAfterDstGap(*search);
        }
    } catch (const cron::bad_cronexpr& e) {
        log::warn(
            "Could not find the next occurrence of '{}': {}",
            cron::to_cronstr(m_expr), e.what()
        );
    }
    return {};
}

bool Cron::matches(std::chrono::sys_seconds time) const {
    if (time == std::chrono::sys_seconds::min()) {
        return false;
    }
    try {
        // Adjacent seconds avoid croncpp's DST rollover bug.
        const auto previous = std::chrono::system_clock::to_time_t(
            time - std::chrono::seconds{1}
        );
        const auto current = std::chrono::system_clock::to_time_t(time);
        return cron::cron_next(m_expr, previous) == current;
    } catch (const cron::bad_cronexpr& e) {
        log::warn(
            "Could not evaluate cron expression '{}': {}",
            cron::to_cronstr(m_expr), e.what()
        );
    }
    return false;
}

}  // namespace anchor
