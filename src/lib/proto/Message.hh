#pragma once

#include <kcore/Concepts.hh>
#include <kcore/serialization/Json.hh>
#include <typeindex>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::proto {

class Message : public kc::NonCopyable, public kc::NonMovable {
   public:
    virtual ~Message() = default;

    static Result<std::unique_ptr<Message>> create(const kc::Json& json);
    kc::Json serialize() const;

    template <typename T>
        requires std::is_base_of_v<Message, T>
    const T* as() const {
        if (type() == typeid(T)) {
            return static_cast<const T*>(this);
        }
        return nullptr;
    }

    virtual Str name() const = 0;

   private:
    virtual kc::Json serializeBody() const = 0;
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
