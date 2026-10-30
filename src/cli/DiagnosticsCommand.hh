#pragma once

#include <CLI/CLI.hpp>
#include <kcore/Concepts.hh>

#include "CommandContext.hh"
#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::cli {

class DiagnosticsCommand : public kc::NonCopyable, public kc::NonMovable {
   public:
    DiagnosticsCommand(CLI::App& app, CommandContext& context);

   private:
    Result<void> run();

    CommandContext& m_context;
    Str m_details{"anchor diagnostics"};
};

}  // namespace anchor::cli
