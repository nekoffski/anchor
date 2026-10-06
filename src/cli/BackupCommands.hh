#pragma once

#include <CLI/CLI.hpp>

#include "Client.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"

namespace anchor::cli {

class BackupCommands : public NonCopyable, public NonMovable {
   public:
    explicit BackupCommands(CLI::App& app);

    bool selected() const { return m_command.parsed(); }
    Result<void> run(u16 port);

   private:
    void initTrigger();
    void initList();
    void initActivate();
    void initStatus();

    Result<void> trigger();
    Result<void> list();
    Result<void> activate();
    Result<void> status();

    CLI::App& m_command;
    Client m_client;
    Str m_triggerName;
    Str m_activateName;
    Opt<Str> m_statusName;
};

}  // namespace anchor::cli
