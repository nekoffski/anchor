#pragma once

#include <typeindex>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Error.hh"
#include "base/Json.hh"

namespace anchor {

class Message : public NonCopyable, public NonMovable {
   public:
    virtual ~Message() = default;

    static Result<std::unique_ptr<Message>> create(const Json& json);
    Json serialize() const;

    template <typename T>
    T* as() {
        if (type() == typeid(T)) {
            return static_cast<T*>(this);
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

class ErrorMessage : public MessageImpl<ErrorMessage> {
   public:
    ErrorMessage(const Error& error)
        : MessageImpl<ErrorMessage>("ErrorMessage"), m_error(error) {}

    virtual Json serializeBody() const override {
        Json json;
        json["message"] = m_error.message();
        json["code"] = std::to_underlying(m_error.code());
        return json;
    }

   private:
    Error m_error;
};

}  // namespace anchor
