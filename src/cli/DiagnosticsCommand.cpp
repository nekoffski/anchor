#include "DiagnosticsCommand.hh"

#include "core/Error.hh"
#include "proto/DiagnosticRequest.hh"

namespace anchor::cli {

DiagnosticsCommand::DiagnosticsCommand(CLI::App& app, CommandContext& context)
    : m_context(context) {
    auto& command = context.addCommand(
        app, "diagnostics", "Send diagnostics to the daemon",
        [this] { return run(); }
    );
    command.add_option("--details", m_details, "Diagnostic details");
}

Result<void> DiagnosticsCommand::run() {
    return m_context.acknowledge(
        proto::DiagnosticRequest{m_details}, "Diagnostics accepted by daemon."
    );
}

}  // namespace anchor::cli
