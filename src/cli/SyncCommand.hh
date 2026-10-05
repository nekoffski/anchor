#pragma once

#include <CLI/CLI.hpp>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor::cli {

class SyncCommand : public NonCopyable, public NonMovable {
   public:
    explicit SyncCommand(CLI::App& app);

    bool selected() const { return m_command.parsed(); }
    Result<void> run() const;

   private:
    CLI::App& m_command;
};

}  // namespace anchor::cli
