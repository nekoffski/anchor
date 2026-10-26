#pragma once

#include <CLI/CLI.hpp>

#include "CommandContext.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor::cli {

class DiagnosticsCommand : public NonCopyable, public NonMovable {
   public:
    DiagnosticsCommand(CLI::App& app, CommandContext& context);

   private:
    Result<void> run();

    CommandContext& m_context;
    Str m_details{"anchor diagnostics"};
};

}  // namespace anchor::cli
