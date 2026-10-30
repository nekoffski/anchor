#pragma once

#include <array>
#include <kcore/Concepts.hh>

#include "Asio.hh"
#include "AsioController.hh"
#include "core/Core.hh"
#include "core/Error.hh"
#include "proto/Message.hh"

namespace anchor {

class AsioSession : public kc::NonCopyable, public kc::NonMovable {
   public:
    explicit AsioSession(
        asio::ip::tcp::socket socket, AsioController& controller
    );

    asio::awaitable<void> start();

   private:
    asio::awaitable<Result<std::unique_ptr<proto::Message>>> readRequest();
    asio::awaitable<void> writeResponse(const proto::Message& message);

    asio::awaitable<Result<Str>> read();
    asio::awaitable<Result<void>> write(const Str& message);

    asio::ip::tcp::socket m_socket;
    AsioController& m_controller;
    Str m_ident;
};

}  // namespace anchor