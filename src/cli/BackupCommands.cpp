#include "BackupCommands.hh"

#include <algorithm>

#include "core/Core.hh"
#include "core/Error.hh"
#include "proto/GetBackupsRequest.hh"
#include "proto/GetBackupsResponse.hh"
#include "proto/ReloadPlansRequest.hh"
#include "proto/TriggerBackupRequest.hh"

namespace anchor::cli {
namespace {

Str validateName(const Str& name) {
    return name.empty() ? "Backup name must not be empty" : "";
}

void printBackups(
    std::ostream& output, const proto::GetBackupsResponse& response
) {
    if (response.backups().empty()) {
        output << "No backups found.\n";
        return;
    }
    auto backups = response.backups();
    const auto name = [](const Backup& backup) { return backup.plan.name; };
    std::ranges::sort(backups, {}, name);
    for (const auto& backup : backups) {
        output << "Backup: " << backup.plan.name << '\n'
               << "  Enabled: " << (backup.enabled ? "yes" : "no") << '\n';
        if (not backup.run) {
            output << "  State: idle\n";
            continue;
        }
        output << "  State: " << backupRunStateToString(backup.run->state)
               << '\n'
               << "  Run ID: " << backup.run->uuid << '\n';
        if (backup.run->error) {
            output << "  Error: " << backup.run->error->message() << '\n';
        }
    }
}

}  // namespace

BackupCommands::BackupCommands(CLI::App& app, CommandContext& context)
    : m_context(context) {
    auto& group = *app.add_subcommand("backup", "Manage backups");
    group.require_subcommand(1, 1);
    group.fallthrough();

    auto& trigger =
        context.addCommand(group, "trigger", "Request a backup run", [this] {
            return this->trigger();
        });
    trigger.add_option("name,--name", m_triggerName, "Backup name")
        ->required()
        ->check(validateName);

    context.addCommand(
        group, "list", "List backups and their latest run",
        [this] { return list(); }
    );
    context.addCommand(
        group, "reload", "Reload backup plans from disk",
        [this] { return reload(); }
    );
}

Result<void> BackupCommands::trigger() {
    return m_context.acknowledge(
        proto::TriggerBackupRequest{m_triggerName},
        fmt::format(
            "Backup '{}' accepted. Use 'anchor backup list' to see its state.",
            m_triggerName
        )
    );
}

Result<void> BackupCommands::list() {
    const auto print = [this](const proto::GetBackupsResponse& response) {
        printBackups(m_context.output(), response);
    };
    return m_context.request<proto::GetBackupsResponse>(
        proto::GetBackupsRequest{}, print
    );
}

Result<void> BackupCommands::reload() {
    return m_context.acknowledge(
        proto::ReloadPlansRequest{}, "Backup plans reloaded."
    );
}

}  // namespace anchor::cli
