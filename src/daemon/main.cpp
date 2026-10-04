
#include "adapters/asio/AsioServer.hh"
#include "base/Functional.hh"
#include "base/Log.hh"
#include "base/Unwrap.hh"
#include "base/runtime/SignalHandler.hh"

using namespace anchor;

int main(int argc, const char** argv) {
    log::expect(argc == 2, "Usage: {} <config_file>", argv[0]);

    Path configPath{argv[1]};
    auto cfg = unwrap(Config::load(configPath));

    log::init(
        log::LoggerOptions{
            .formatPattern = "%^[%d-%m-%Y %T] [Th: %t] %-7l [anchord]: %v %$"
        }
    );
    log::info("daemon starting");
    DEFER { log::info("daemon shutting down, cya!"); };

    SignalHandler signals;

    AsioController controller;
    AsioServer server{cfg, controller};
    server.start();

    signals.wait([&](Signal s) {
        log::info("received signal: {}", toString(s));

        if (s == Signal::sigint || s == Signal::sigterm) {
            log::info("received shutdown request");

            server.stop();
            return SignalAction::stop;
        }
        return SignalAction::waitForNext;
    });

    Thread::join(server);

    return 0;
}