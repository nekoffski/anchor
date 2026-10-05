#include "BackupCommands.hh"

namespace anchor::cli {

namespace {

Str validateName(const Str& name) {
    return name.empty() ? "Backup name must not be empty" : "";
}

}  // namespace

BackupCommands::BackupCommands(CLI::App& app)
    : m_command(*app.add_subcommand("backup", "Manage backups (mock)")) {
    m_command.require_subcommand(1);
    m_command.fallthrough();
    initTrigger();
    initList();
    initActivate();
    initStatus();
}

void BackupCommands::initTrigger() {
    auto* command =
        m_command.add_subcommand("trigger", "Trigger a backup (mock)");
    command->add_option("--name", m_triggerName, "Backup name")
        ->required()
        ->check(validateName);
}

void BackupCommands::initList() {
    m_command.add_subcommand("list", "List backups (mock)");
}

void BackupCommands::initActivate() {
    auto* command =
        m_command.add_subcommand("activate", "Activate a backup (mock)");
    command->add_option("--name", m_activateName, "Backup name")
        ->required()
        ->check(validateName);
}

void BackupCommands::initStatus() {
    auto* command =
        m_command.add_subcommand("status", "Show backup status (mock)");
    command
        ->add_option(
            "--name", m_statusName, "Backup name (omit for all backups)"
        )
        ->check(validateName);
}

Result<void> BackupCommands::run() const {
    if (m_command.got_subcommand("trigger")) {
        return trigger();
    }
    if (m_command.got_subcommand("list")) {
        return list();
    }
    if (m_command.got_subcommand("activate")) {
        return activate();
    }
    if (m_command.got_subcommand("status")) {
        return status();
    }
    return Error::unexpected(
        ErrorCode::invalidArgument, "Missing backup command"
    );
}

Result<void> BackupCommands::trigger() const {
    fmt::print(
        "Mock: backup trigger for '{}' is not implemented yet.\n", m_triggerName
    );
    return {};
}

Result<void> BackupCommands::list() const {
    fmt::print("Mock: backup list is not implemented yet.\n");
    return {};
}

Result<void> BackupCommands::activate() const {
    fmt::print(
        "Mock: backup activate for '{}' is not implemented yet.\n",
        m_activateName
    );
    return {};
}

Result<void> BackupCommands::status() const {
    fmt::print(
        "Mock: backup status for '{}' is not implemented yet.\n",
        m_statusName.value_or("all backups")
    );
    return {};
}

}  // namespace anchor::cli
