#include "Message.hh"

namespace anchor::proto {

Result<std::unique_ptr<Message>> Message::create(const Json& json) {
    if (json.find("name") == json.end()) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Missing message name"
        );
    }

    if (json.find("body") == json.end()) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Missing message body"
        );
    }

    const auto& name = json["name"];
    const auto& body = json["body"];

    if (name == "DiagnosticRequest") {
        return DiagnosticRequest::create(body);
    }
}

Json Message::serialize() const {
    Json json;
    json["name"] = name();
    json["body"] = serializeBody();
    return json;
}

}  // namespace anchor::proto
