#include "SnapshotCommands.hh"

#include <fmt/chrono.h>

#include <kcore/Random.hh>

#include "proto/SnapshotMessages.hh"

namespace anchor::cli {

SnapshotCommands::SnapshotCommands(CLI::App& app, CommandContext& context)
    : m_context(context) {
    auto& group = *app.add_subcommand("snapshot", "Manage snapshots");
    group.require_subcommand(1, 1);
    group.fallthrough();

    auto& list = context.addCommand(
        group, "list", "List snapshots for a backup",
        [this] { return this->list(); }
    );
    auto& inspect =
        context.addCommand(group, "inspect", "Inspect a snapshot", [this] {
            return this->inspect();
        });
    auto& remove = context.addCommand(
        group, "remove", "Remove a snapshot", [this] { return this->remove(); }
    );
    for (auto* command : {&list, &inspect, &remove}) {
        command->add_option("backup", m_backupName, "Backup name")
            ->required()
            ->check([](const Str& name) -> Str {
                return name.empty() ? "Backup name must not be empty" : "";
            });
    }
    for (auto* command : {&inspect, &remove}) {
        command->add_option("uuid", m_uuid, "Snapshot UUID")
            ->required()
            ->check([](const Str& uuid) -> Str {
                return kc::isValidUuid(uuid) ? "" : "Invalid snapshot UUID";
            });
    }
}

Result<void> SnapshotCommands::list() {
    return m_context.request<proto::GetSnapshotsResponse>(
        proto::GetSnapshotsRequest{m_backupName},
        [this](const proto::GetSnapshotsResponse& response) {
            if (not response.snapshots().empty()) {
                for (const auto& snapshot : response.snapshots()) {
                    m_context.output()
                        << "Snapshot: " << snapshot.uuid << '\n'
                        << "  Created: "
                        << fmt::format("{:%FT%TZ}", snapshot.created) << '\n'
                        << "  Location: " << snapshot.location.str() << '\n';
                }
                return;
            }
            m_context.output()
                << "No snapshots found for backup '" << m_backupName << "'.\n";
        }
    );
}

Result<void> SnapshotCommands::inspect() {
    return m_context.request<proto::GetSnapshotResponse>(
        proto::GetSnapshotRequest{m_backupName, m_uuid},
        [this](const proto::GetSnapshotResponse&) {
            m_context.output() << "No snapshot details available.\n";
        }
    );
}

Result<void> SnapshotCommands::remove() {
    return m_context.acknowledge(
        proto::RemoveSnapshotRequest{m_backupName, m_uuid},
        fmt::format("Snapshot '{}' removed.", m_uuid)
    );
}

}  // namespace anchor::cli
