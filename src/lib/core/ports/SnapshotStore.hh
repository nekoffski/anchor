#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "core/domain/Backup.hh"

namespace anchor {

class SnapshotStore : public NonCopyable, public NonMovable {
   public:
    ~SnapshotStore() = default;
};

class SnapshotStores : public NonCopyable, public NonMovable {
   public:
    template <typename T, typename... Args>
        requires std::is_base_of_v<SnapshotStore, T> &&
                 std::constructible_from<T, Args...>
    void add(DestinationType type, Args&&... args) {
        add(type, std::make_unique<T>(std::forward<Args>(args)...));
    }

    void add(
        DestinationType type, std::unique_ptr<SnapshotStore> snapshotStore
    ) {
        m_snapshotStores.insert({type, std::move(snapshotStore)});
    }

    SnapshotStore* get(DestinationType type) {
        if (auto it = m_snapshotStores.find(type);
            it != m_snapshotStores.end()) {
            return it->second.get();
        }
        return nullptr;
    }

   private:
    std::unordered_map<DestinationType, std::unique_ptr<SnapshotStore>>
        m_snapshotStores;
};

}  // namespace anchor
