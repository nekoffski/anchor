#include "Client.hh"

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::cli {

namespace {

Error convertError(const std::error_code& ec) {
    if (ec == asio::error::eof) {
        return Error{kc::ErrorCode::eof, "Socket closed by peer"};
    }
    if (ec == asio::error::not_found) {
        return Error{kc::ErrorCode::bufferOverflow, "Response too long"};
    }
    return Error{kc::ErrorCode::connectionFailure, ec.message()};
}

}  // namespace

Result<void> Client::connect(u16 port) {
    std::error_code ec;
    m_socket.connect({asio::ip::address_v4::loopback(), port}, ec);
    if (ec) {
        return Error::unexpected(convertError(ec));
    }
    return {};
}

Result<void> Client::send(const proto::Message& message) {
    const auto frame = message.serialize().dump() + '\n';
    std::error_code ec;
    asio::write(m_socket, asio::buffer(frame), ec);
    if (ec) {
        return Error::unexpected(convertError(ec));
    }
    return {};
}

Result<std::unique_ptr<proto::Message>> Client::receive() {
    static constexpr u64 maxMessageSize = 1024 * 32;
    static constexpr char delimiter = '\n';

    std::error_code ec;
    const auto size = asio::read_until(
        m_socket, asio::dynamic_buffer(m_buffer, maxMessageSize), delimiter, ec
    );
    if (ec) {
        return Error::unexpected(convertError(ec));
    }

    auto json = kc::parseJson(m_buffer.substr(0, size));
    m_buffer.erase(0, size);  // Retain subsequent frames for the next receive.
    if (not json) {
        return Error::unexpected(json.error());
    }

    try {
        auto response = proto::Message::create(*json);
        if (not response) {
            return Error::unexpected(response.error());
        }
        return std::move(*response);
    } catch (const kc::Json::exception& error) {
        return Error::unexpected(
            kc::ErrorCode::serializationFailure, "Invalid response: {}",
            error.what()
        );
    }
}

Result<std::unique_ptr<proto::Message>> Client::request(
    u16 port, const proto::Message& message
) {
    std::error_code ignored;
    m_socket.close(ignored);
    m_buffer.clear();
    if (auto connected = connect(port); not connected) {
        return Error::unexpected(
            connected.error().code(), "Could not connect to daemon: {}",
            connected.error().message()
        );
    }
    if (auto sent = send(message); not sent) {
        return Error::unexpected(sent.error());
    }
    return receive();
}

}  // namespace anchor::cli
