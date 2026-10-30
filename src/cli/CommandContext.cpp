#include "CommandContext.hh"

#include "core/Core.hh"
#include "core/Error.hh"
#include "proto/AckResponse.hh"

namespace anchor::cli {

CommandContext::CommandContext(
    Client& client, const u16& port, std::ostream& output
)
    : m_client(client), m_port(port), m_output(output) {}

CLI::App& CommandContext::addCommand(
    CLI::App& parent, const Str& name, const Str& description,
    std::function<Result<void>()> action
) {
    auto& command = *parent.add_subcommand(name, description);
    command.fallthrough();
    command.callback([this, action = std::move(action)] { m_action = action; });
    return command;
}

Result<void> CommandContext::run() {
    if (not m_action) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Missing command"
        );
    }
    return m_action();
}

void CommandContext::reset() { m_action = {}; }

Result<void> CommandContext::acknowledge(
    const proto::Message& message, const Str& text
) {
    const auto printAcknowledgement = [&](const proto::AckResponse&) {
        m_output << text << '\n';
    };
    return request<proto::AckResponse>(message, printAcknowledgement);
}

}  // namespace anchor::cli
