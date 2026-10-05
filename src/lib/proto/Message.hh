#pragma once

#include <typeindex>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"
#include "base/Json.hh"

namespace anchor::proto {

class Message : public NonCopyable, public NonMovable {
   public:
    virtual ~Message() = default;

    static Result<std::unique_ptr<Message>> create(const Json& json);
    Json serialize() const;

    template <typename T>
        requires std::is_base_of_v<Message, T>
    const T* as() const {
        if (type() == typeid(T)) {
            return static_cast<const T*>(this);
        }
        return nullptr;
    }

   private:
    virtual Json serializeBody() const = 0;
    virtual Str name() const = 0;
    virtual std::type_index type() const = 0;
};

template <typename T>
class MessageImpl : public Message {
   public:
    MessageImpl(const Str& name) : m_name(std::move(name)) {}

   private:
    std::type_index type() const override { return typeid(T); }
    Str name() const override { return m_name; }

    Str m_name;
};

class ErrorResponse : public MessageImpl<ErrorResponse> {
   public:
    explicit ErrorResponse(ErrorCode code, const Str& message)
        : ErrorResponse(Error{code, message}) {}

    explicit ErrorResponse(const Error& error)
        : MessageImpl<ErrorResponse>("ErrorResponse"), m_error(error) {}

    virtual Json serializeBody() const override {
        Json json;
        json["message"] = m_error.message();
        json["code"] = std::to_underlying(m_error.code());
        return json;
    }

   private:
    Error m_error;
};

class DiagnosticRequest : public MessageImpl<DiagnosticRequest> {
   public:
    DiagnosticRequest(const Str& details)
        : MessageImpl<DiagnosticRequest>("DiagnosticRequest"),
          m_details(details) {}

    virtual Json serializeBody() const override {
        Json json;
        json["details"] = m_details;
        return json;
    }

    static std::unique_ptr<DiagnosticRequest> create(const Json& json) {
        return std::make_unique<DiagnosticRequest>(json["details"].get<Str>());
    }

    const Str& details() const { return m_details; }

   private:
    Str m_details;
};

class AckResponse : public MessageImpl<AckResponse> {
   public:
    AckResponse() : MessageImpl<AckResponse>("AckResponse") {}

    virtual Json serializeBody() const override { return Json{}; }
};

}  // namespace anchor::proto
