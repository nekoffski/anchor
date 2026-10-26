#pragma once

#include <CLI/CLI.hpp>

#include "CommandContext.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor::cli {

class BackupCommands : public NonCopyable, public NonMovable {
   public:
    BackupCommands(CLI::App& app, CommandContext& context);

   private:
    Result<void> trigger();
    Result<void> list();
    Result<void> reload();

    CommandContext& m_context;
    Str m_triggerName;
};

}  // namespace anchor::cli
