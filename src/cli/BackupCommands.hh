#pragma once

#include <CLI/CLI.hpp>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor::cli {

class BackupCommands : public NonCopyable, public NonMovable {
   public:
    explicit BackupCommands(CLI::App& app);

    bool selected() const { return m_command.parsed(); }
    Result<void> run() const;

   private:
    void initTrigger();
    void initList();
    void initActivate();
    void initStatus();

    Result<void> trigger() const;
    Result<void> list() const;
    Result<void> activate() const;
    Result<void> status() const;

    CLI::App& m_command;
    Str m_triggerName;
    Str m_activateName;
    Opt<Str> m_statusName;
};

}  // namespace anchor::cli
