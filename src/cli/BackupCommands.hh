#pragma once

#include <CLI/CLI.hpp>
#include <kcore/Concepts.hh>

#include "CommandContext.hh"
#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::cli {

class BackupCommands : public kc::NonCopyable, public kc::NonMovable {
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
