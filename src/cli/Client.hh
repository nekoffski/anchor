#pragma once

#include "adapters/asio/Asio.hh"
#include "proto/Message.hh"

namespace anchor::cli {

class Client : public NonCopyable, public NonMovable {
   public:
    Client() = default;

    Result<void> connect(u16 port);
    Result<void> send(const proto::Message& message);
    Result<std::unique_ptr<proto::Message>> receive();
    Result<std::unique_ptr<proto::Message>> request(
        const proto::Message& message
    );

   private:
    asio::io_context m_io;
    asio::ip::tcp::socket m_socket{m_io};
    Str m_buffer;
};

}  // namespace anchor::cli
