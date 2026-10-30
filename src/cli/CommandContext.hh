#pragma once

#include <CLI/CLI.hpp>
#include <functional>
#include <ostream>

#include "Client.hh"
#include "core/Core.hh"
#include "core/Error.hh"
#include "proto/ErrorResponse.hh"

namespace anchor::cli {

class CommandContext : public kc::NonCopyable, public kc::NonMovable {
   public:
    CommandContext(Client& client, const u16& port, std::ostream& output);

    CLI::App& addCommand(
        CLI::App& parent, const Str& name, const Str& description,
        std::function<Result<void>()> action
    );
    Result<void> run();
    void reset();
    std::ostream& output() { return m_output; }

    template <typename Response, typename Renderer>
    Result<void> request(const proto::Message& message, Renderer render) {
        auto response = m_client.request(m_port, message);
        if (not response) {
            return Error::unexpected(response.error());
        }
        if (not *response) {
            return Error::unexpected(
                ErrorCode::invalidRequest, "Empty response"
            );
        }
        if (const auto* error = (*response)->as<proto::ErrorResponse>()) {
            return Error::unexpected(error->error());
        }
        const auto* typed = (*response)->as<Response>();
        if (not typed) {
            return Error::unexpected(
                ErrorCode::invalidRequest, "Unexpected response '{}' to '{}'",
                (*response)->name(), message.name()
            );
        }
        render(*typed);
        return {};
    }

    Result<void> acknowledge(const proto::Message& message, const Str& text);

   private:
    Client& m_client;
    const u16& m_port;
    std::ostream& m_output;
    std::function<Result<void>()> m_action;
};

}  // namespace anchor::cli
