#pragma once

#include <unordered_map>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/FileSystem.hh"
#include "core/domain/Backup.hh"
#include "core/domain/Snapshot.hh"

namespace anchor {

class Snapshotter : public NonCopyable, public NonMovable {
   public:
    virtual ~Snapshotter() = default;

    virtual Result<SnapshotMetadata> snapshot(
        const Str& name, std::span<const Filter> filters, const Path& workspace
    ) = 0;
};

class Snapshotters : public NonCopyable, public NonMovable {
   public:
    template <typename T, typename... Args>
        requires std::is_base_of_v<Snapshotter, T> &&
                 std::constructible_from<T, Args...>
    void add(ArchiveType type, Args&&... args) {
        add(type, std::make_unique<T>(std::forward<Args>(args)...));
    }

    void add(ArchiveType type, std::unique_ptr<Snapshotter> snapshotter) {
        m_snapshotters.insert({type, std::move(snapshotter)});
    }

    Snapshotter* get(ArchiveType type) {
        if (auto it = m_snapshotters.find(type); it != m_snapshotters.end()) {
            return it->second.get();
        }
        return nullptr;
    }

   private:
    std::unordered_map<ArchiveType, std::unique_ptr<Snapshotter>>
        m_snapshotters;
};

}  // namespace anchor
