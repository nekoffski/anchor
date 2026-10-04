#include "AsioController.hh"

namespace anchor {

std::unique_ptr<Message> anchor::AsioController::handle(
    const Message& message
) {
    return std::unique_ptr<Message>();
}

}  // namespace anchor