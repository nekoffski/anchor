#pragma once

#include "Asio.hh"
#include "AsioController.hh"
#include "base/Config.hh"
#include "base/runtime/Thread.hh"

namespace anchor {

class AsioServer : public Thread {
   public:
    explicit AsioServer(const Config& cfg, AsioController& controller);
    void stop();

   private:
    void run() override;

    asio::awaitable<void> acceptConnection();

    Config m_cfg;
    AsioController& m_controller;

    asio::io_context m_io;
    asio::ip::tcp::acceptor m_acceptor;
};

}  // namespace anchor
