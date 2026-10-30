#pragma once

#include <CLI/CLI.hpp>

#include "BackupCommands.hh"
#include "DiagnosticsCommand.hh"
#include "core/Config.hh"
#include "core/Core.hh"

namespace anchor::cli {

class Application : public kc::NonCopyable, public kc::NonMovable {
   public:
    Application(Client& client, std::ostream& output, std::ostream& errors);
    int run(int argc, const char* const* argv);

   private:
    CLI::App m_app{"Anchor one-shot CLI", "anchor"};
    u16 m_port{Config::Api{}.port};
    std::ostream& m_output;
    std::ostream& m_errors;
    CommandContext m_context;
    DiagnosticsCommand m_diagnostics{m_app, m_context};
    BackupCommands m_backup{m_app, m_context};
};

}  // namespace anchor::cli
