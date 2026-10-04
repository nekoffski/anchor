#include "Message.hh"

namespace anchor {

Result<std::unique_ptr<Message>> Message::create(const Json& json) {
    return nullptr;
}

Json Message::serialize() const {
    Json json;
    json["name"] = name();
    json["body"] = serializeBody();
    return json;
}

}  // namespace anchor
