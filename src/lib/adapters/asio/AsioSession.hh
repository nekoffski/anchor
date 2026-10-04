#pragma once

#include <array>

#include "Asio.hh"
#include "AsioController.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"
#include "proto/Message.hh"

namespace anchor {

class AsioSession : public NonCopyable, public NonMovable {
   public:
    explicit AsioSession(
        asio::ip::tcp::socket socket, AsioController& controller
    );

    asio::awaitable<void> start();

   private:
    asio::awaitable<Result<std::unique_ptr<Message>>> readRequest();
    asio::awaitable<void> writeResponse(const Message& message);

    asio::awaitable<Result<Str>> read();
    asio::awaitable<Result<void>> write(const Str& message);

    asio::ip::tcp::socket m_socket;
    AsioController& m_controller;
    Str m_ident;
};

}  // namespace anchor