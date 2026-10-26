#include "Application.hh"

#include <sstream>

namespace anchor::cli {

Application::Application(
    Client& client, std::ostream& output, std::ostream& errors
)
    : m_output(output), m_errors(errors), m_context(client, m_port, output) {
    m_app.require_subcommand(1, 1);
    m_app.add_option("--port", m_port, "Local daemon port")
        ->check(CLI::Range(1, 65535))
        ->capture_default_str();
}

int Application::run(int argc, const char* const* argv) {
    m_context.reset();
    try {
        m_app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        std::ostringstream output;
        std::ostringstream errors;
        const auto code = m_app.exit(error, output, errors);
        m_output << output.str();
        m_errors << errors.str();
        return code;
    }

    if (auto result = m_context.run(); not result) {
        m_errors << "anchor: " << result.error().message() << '\n';
        return 1;
    }
    return 0;
}

}  // namespace anchor::cli
