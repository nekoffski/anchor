#include "AsioSession.hh"

#include "base/Functional.hh"
#include "base/Log.hh"
#include "base/String.hh"

namespace anchor {

namespace {

std::string extractIdent(asio::ip::tcp::socket& socket) {
    return fmt::format(
        "{}:{}", socket.remote_endpoint().address().to_string(),
        socket.remote_endpoint().port()
    );
}

Error convertError(const std::error_code& ec) {
    if (ec == asio::error::eof) {
        return Error{ErrorCode::eof, "Socket closed by peer"};
    } else if (ec == asio::error::not_found) {
        return Error{ErrorCode::bufferOverflow, "Request too long"};
    }
    return Error{ErrorCode::connectionFailure, ec.message()};
}

}  // namespace

AsioSession::AsioSession(
    asio::ip::tcp::socket socket, AsioController& controller
)
    : m_socket(std::move(socket)),
      m_controller(controller),
      m_ident(extractIdent(m_socket)) {}

asio::awaitable<void> AsioSession::start() {
    log::info("{} - session started", m_ident);
    DEFER { log::info("{} - session ended", m_ident); };

    for (;;) {
        auto req = co_await readRequest();

        if (not req) {
            if (req.error().code() == ErrorCode::eof) {
                log::info("{} - client disconnected", m_ident);
                co_return;
            }

            log::error("{} - error reading request - {}", m_ident, req.error());
            proto::ErrorResponse response{req.error()};
            co_return co_await writeResponse(response);
        }

        auto res = m_controller.handle(*req.value());
        co_await writeResponse(*res);
    }
}

asio::awaitable<Result<std::unique_ptr<proto::Message>>>
AsioSession::readRequest() {
    auto message = co_await read();

    if (not message) {
        if (message.error().code() != ErrorCode::eof) {
            log::error(
                "{} - error reading from socket: {}", m_ident,
                message.error().message()
            );
        }
        co_return Error::unexpected(message.error());
    }

    auto json = parseJson(message.value());

    if (not json) {
        log::error("{} - error parsing JSON", m_ident);
        co_return Error::unexpected(json.error());
    }

    co_return proto::Message::create(json.value());
}

asio::awaitable<void> AsioSession::writeResponse(
    const proto::Message& message
) {
    auto json = message.serialize().dump() + '\n';

    if (auto response = co_await write(json); not response) {
        log::error(
            "{} - error writing response: {}", m_ident,
            response.error().message()
        );
    }
}

asio::awaitable<Result<Str>> AsioSession::read() {
    static constexpr u64 maxMessageSize = 1024 * 32;
    static constexpr char delimiter = '\n';

    Str message;

    auto [ec, bytesRead] = co_await asio::async_read_until(
        m_socket, asio::dynamic_buffer(message, maxMessageSize), delimiter,
        asio::as_tuple(asio::use_awaitable)
    );

    if (ec) [[unlikely]] {
        co_return Error::unexpected(convertError(ec));
    }

    message.pop_back();  // Remove delimiter

    log::debug(
        "{} - received message len={}, '{}'", m_ident, bytesRead, message
    );
    co_return message;
}

asio::awaitable<Result<void>> AsioSession::write(const Str& message) {
    auto [ec, bytesWritten] = co_await asio::async_write(
        m_socket, asio::buffer(message), asio::as_tuple(asio::use_awaitable)
    );

    if (ec) [[unlikely]] {
        co_return Error::unexpected(convertError(ec));
    }

    log::debug(
        "{} - sent message len={}, '{}'", m_ident, bytesWritten, message
    );
    co_return Result<void>{};
}

}  // namespace anchor
