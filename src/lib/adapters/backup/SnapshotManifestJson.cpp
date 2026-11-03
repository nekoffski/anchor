#include "SnapshotManifestJson.hh"

#include <kcore/serialization/Json.hh>

namespace anchor {
namespace {

kc::Json serialize(const SnapshotManifest& manifest) {
    auto sources = kc::Json::array();
    for (const auto& source : manifest.sources) {
        auto entries = kc::Json::array();
        for (const auto& entry : source.entries) {
            kc::Json value{
                {"path", entry.path},
                {"permissions", entry.permissions},
                {"owner", entry.owner},
                {"group", entry.group},
                {"modified_ns", entry.modified.time_since_epoch().count()},
            };
            switch (entry.type) {
                case SnapshotEntry::Type::file:
                    value["type"] = "file";
                    value["size"] = entry.size;
                    value["sha256"] = entry.sha256;
                    break;
                case SnapshotEntry::Type::directory:
                    value["type"] = "directory";
                    break;
                case SnapshotEntry::Type::symlink:
                    value["type"] = "symlink";
                    value["target"] = entry.linkTarget;
                    break;
            }
            entries.push_back(std::move(value));
        }
        sources.push_back(
            {{"id", source.id}, {"root", source.root}, {"entries", entries}}
        );
    }
    return {
        {"version", manifest.version},
        {"name", manifest.name},
        {"created_at_ns", manifest.created.time_since_epoch().count()},
        {"hash_algorithm", "sha256"},
        {"sources", sources},
    };
}

}  // namespace

Result<Str> serializeSnapshotManifest(const SnapshotManifest& manifest) {
    try {
        return serialize(manifest).dump(2) + "\n";
    } catch (const kc::Json::exception& e) {
        return Error::unexpected(
            ErrorCode::serializationFailure,
            "Could not encode snapshot manifest: {}", e.what()
        );
    }
}

}  // namespace anchor
