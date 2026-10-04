#include "SignalHandler.hh"

#include <csignal>

#include "base/Log.hh"

namespace anchor {

SignalHandler::SignalHandler() {
    sigemptyset(&m_sigset);
    sigaddset(&m_sigset, SIGINT);
    sigaddset(&m_sigset, SIGTERM);

    pthread_sigmask(SIG_BLOCK, &m_sigset, nullptr);
}

void SignalHandler::waitImpl(MoveOnlyFunction<SignalAction(Signal)> callback) {
    for (;;) {
        i32 posixSignal = 0;
        sigwait(&m_sigset, &posixSignal);

        if (auto signal = fromPosix(posixSignal); signal) {
            if (callback(*signal) == SignalAction::stop) {
                break;
            }
        } else {
            log::panic(
                "Received posix signal that could not be converted to "
                "internal representation: {} - {}",
                posixSignal, signal.error()
            );
        }
    }
}

Result<Signal> fromPosix(i32 posixSignal) {
    switch (posixSignal) {
        case SIGINT:
            return Signal::sigint;
        case SIGTERM:
            return Signal::sigterm;
        default:
            return Error::unexpected(
                ErrorCode::invalidArgument, "Invalid signal"
            );
    }
}

Str toString(Signal signal) {
    switch (signal) {
        case Signal::sigint:
            return "SIGINT";
        case Signal::sigterm:
            return "SIGTERM";
    };
    std::unreachable();
}

}  // namespace anchor
