#pragma once

#include <CLI/CLI.hpp>

#include "BackupCommands.hh"
#include "DiagnosticsCommand.hh"
#include "SyncCommand.hh"
#include "base/Config.hh"

namespace anchor::cli {

class Application : public NonCopyable, public NonMovable {
   public:
    Application();
    int run(int argc, const char** argv);

   private:
    Result<void> runCommand() const;

    CLI::App m_app{"Anchor one-shot CLI", "anchor"};
    u16 m_port{Config::Api{}.port};
    DiagnosticsCommand m_diagnostics{m_app};
    SyncCommand m_sync{m_app};
    BackupCommands m_backup{m_app};
};

}  // namespace anchor::cli
