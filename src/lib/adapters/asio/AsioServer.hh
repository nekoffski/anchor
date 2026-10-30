#pragma once

#include <kcore/runtime/Thread.hh>

#include "Asio.hh"
#include "AsioController.hh"
#include "core/Config.hh"

namespace anchor {

class AsioServer : public kc::Thread {
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
