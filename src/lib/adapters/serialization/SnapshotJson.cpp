#include "SnapshotJson.hh"

#include <kcore/Random.hh>

namespace anchor {

namespace {

using Timestamp = std::chrono::sys_time<std::chrono::nanoseconds>;

Result<Timestamp> parseCreatedAt(const kc::Json& value) {
    const auto overflow =
        value.is_number_unsigned() &&
        value.get<u64>() > static_cast<u64>(std::numeric_limits<i64>::max());
    if (not value.is_number_integer() || overflow) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid snapshot creation time"
        );
    }
    return Timestamp{std::chrono::nanoseconds{value.get<i64>()}};
}

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
        {"uuid", manifest.uuid},
        {"backup_name", manifest.backupName},
        {"format", archiveTypeToStr(manifest.format)},
        {"created_at_ns", manifest.created.time_since_epoch().count()},
        {"hash_algorithm", SnapshotManifest::hashAlgorithm},
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

Result<SnapshotManifestHeader> parseSnapshotManifestHeader(
    const Str& contents
) {
    try {
        const auto json = kc::Json::parse(contents);
        const auto& version = json.at("version");
        const auto uuid = kc::parseUuid(json.at("uuid").get<Str>());
        const auto backupName = json.at("backup_name").get<Str>();
        if (not version.is_number_integer() ||
            version != SnapshotManifest::currentVersion || not uuid ||
            backupName.empty() ||
            json.at("hash_algorithm") != SnapshotManifest::hashAlgorithm ||
            not json.at("sources").is_array()) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument,
                "Invalid snapshot manifest header"
            );
        }
        return archiveTypeFromStr(json.at("format").get<Str>())
            .and_then([&](ArchiveType format) {
                return parseCreatedAt(json.at("created_at_ns"))
                    .transform([&](Timestamp created) {
                        return SnapshotManifestHeader{
                            *uuid, backupName, format, created
                        };
                    });
            });
    } catch (const kc::Json::exception& error) {
        return Error::unexpected(
            ErrorCode::serializationFailure, "Invalid snapshot manifest: {}",
            error.what()
        );
    }
}

kc::Json serializeSnapshotSummary(const SnapshotSummary& snapshot) {
    return {
        {"uuid", snapshot.uuid},
        {"created_at_ns", snapshot.created.time_since_epoch().count()},
        {"location", snapshot.location.str()}
    };
}

Result<SnapshotSummary> parseSnapshotSummary(const kc::Json& json) {
    try {
        const auto uuid = kc::parseUuid(json.at("uuid").get<Uuid>());
        if (not uuid) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument, "Invalid snapshot UUID"
            );
        }
        auto created = parseCreatedAt(json.at("created_at_ns"));
        if (not created) {
            return Error::unexpected(created.error());
        }
        auto location = Location::fromStr(json.at("location").get<Str>());
        if (not location) {
            return Error::unexpected(location.error());
        }
        const kc::Path path{location->path};
        if (location->path.empty() ||
            (location->type == LocationType::local &&
             (not path.isAbsolute() || path.normalized().str() == "/"))) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument,
                "Invalid snapshot artifact location"
            );
        }
        return SnapshotSummary{*uuid, *created, *location};
    } catch (const kc::Json::exception& error) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid snapshot summary: {}",
            error.what()
        );
    }
}

}  // namespace anchor
