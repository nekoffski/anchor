#include "Application.hh"

#include <cstdio>
#include <sstream>

namespace anchor::cli {

Application::Application() {
    m_app.require_subcommand(1);
    m_app.add_option("--port", m_port, "Local daemon port")
        ->check(CLI::Range(1, 65535))
        ->capture_default_str();
}

int Application::run(int argc, const char** argv) {
    try {
        m_app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        std::ostringstream output;
        std::ostringstream errors;
        const auto code = m_app.exit(error, output, errors);
        fmt::print("{}", output.str());
        fmt::print(stderr, "{}", errors.str());
        return code;
    }

    if (auto result = runCommand(); not result) {
        fmt::print(stderr, "anchor: {}\n", result.error().message());
        return 1;
    }
    return 0;
}

Result<void> Application::runCommand() {
    if (m_diagnostics.selected()) {
        return m_diagnostics.run(m_port);
    }
    if (m_sync.selected()) {
        return m_sync.run();
    }
    if (m_backup.selected()) {
        return m_backup.run(m_port);
    }
    return Error::unexpected(ErrorCode::invalidArgument, "Missing command");
}

}  // namespace anchor::cli
