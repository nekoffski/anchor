#include "Cron.hh"

namespace anchor {

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

}  // namespace anchor
