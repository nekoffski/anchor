#include "SyncCommand.hh"

namespace anchor::cli {

SyncCommand::SyncCommand(CLI::App& app)
    : m_command(*app.add_subcommand("sync", "Synchronize (mock)")) {
    m_command.fallthrough();
}

Result<void> SyncCommand::run() const {
    fmt::print("Mock: sync is not implemented yet.\n");
    return {};
}

}  // namespace anchor::cli
