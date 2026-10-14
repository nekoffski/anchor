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
    explicit MessageImpl(const Str& name) : m_name(name) {}

   private:
    std::type_index type() const override { return typeid(T); }
    Str name() const override { return m_name; }

    Str m_name;
};

}  // namespace anchor::proto
