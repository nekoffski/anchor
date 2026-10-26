#pragma once

#include <croncpp.h>

#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor {

class Cron {
   public:
    static Result<Cron> fromString(const Str& cron);

   private:
    explicit Cron(cron::cronexpr&& expr);

    cron::cronexpr m_expr;
};

}  // namespace anchor
