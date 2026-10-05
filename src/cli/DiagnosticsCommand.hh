#pragma once

#include <CLI/CLI.hpp>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor::cli {

class DiagnosticsCommand : public NonCopyable, public NonMovable {
   public:
    explicit DiagnosticsCommand(CLI::App& app);

    bool selected() const { return m_command.parsed(); }
    Result<void> run(u16 port) const;

   private:
    CLI::App& m_command;
    Str m_details{"anchor diagnostics"};
};

}  // namespace anchor::cli
