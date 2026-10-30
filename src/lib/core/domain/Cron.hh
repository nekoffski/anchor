#pragma once

#include <croncpp.h>

#include <chrono>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

class Cron {
   public:
    static Result<Cron> fromString(const Str& cron);
    bool matches(std::chrono::sys_seconds time) const;
    kc::Opt<std::chrono::sys_seconds> nextAfter(
        std::chrono::sys_seconds time
    ) const;

   private:
    explicit Cron(cron::cronexpr&& expr);

    cron::cronexpr m_expr;
};

}  // namespace anchor
