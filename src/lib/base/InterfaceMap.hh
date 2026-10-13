#pragma once

#include <memory>

#include "FlatMap.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"

namespace anchor {

template <typename Key, typename Interface>
class InterfaceMap : public NonCopyable, public NonMovable {
   public:
    template <typename T, typename... Args>
        requires std::is_base_of_v<Interface, T> &&
                 std::constructible_from<T, Args...>
    void add(const Key& key, Args&&... args) {
        add(key, std::make_unique<T>(std::forward<Args>(args)...));
    }

    void add(const Key& key, std::unique_ptr<Interface> interface) {
        m_interfaces.insert(key, std::move(interface));
    }

    Interface* get(const Key& key) {
        if (auto it = m_interfaces.find(key); it != m_interfaces.end()) {
            return it->second.get();
        }
        return nullptr;
    }

   private:
    FlatMap<Key, std::unique_ptr<Interface>> m_interfaces;
};

}  // namespace anchor
