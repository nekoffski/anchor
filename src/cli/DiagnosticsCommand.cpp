#include "DiagnosticsCommand.hh"

#include "Client.hh"
#include "proto/AckResponse.hh"
#include "proto/DiagnosticRequest.hh"

namespace anchor::cli {

DiagnosticsCommand::DiagnosticsCommand(CLI::App& app)
    : m_command(
          *app.add_subcommand("diagnostics", "Send diagnostics to the daemon")
      ) {
    m_command.fallthrough();
    m_command.add_option("--details", m_details, "Diagnostic details");
}

Result<void> DiagnosticsCommand::run(u16 port) const {
    Client client;
    if (auto connected = client.connect(port); not connected) {
        return Error::unexpected(connected.error());
    }

    auto response = client.request(proto::DiagnosticRequest{m_details});
    if (not response) {
        return Error::unexpected(response.error());
    }
    if (not(*response)->as<proto::AckResponse>()) {
        return Error::unexpected(
            ErrorCode::invalidRequest, "Unexpected diagnostic response"
        );
    }

    fmt::print("Diagnostics accepted by daemon.\n");
    return {};
}

}  // namespace anchor::cli
