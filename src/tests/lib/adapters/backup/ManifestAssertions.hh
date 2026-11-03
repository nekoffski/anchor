#pragma once

#include <gtest/gtest.h>
#include <openssl/sha.h>

#include <array>
#include <kcore/serialization/Json.hh>

#include "core/domain/Snapshot.hh"

namespace anchor::test {

inline Str referenceSha256(const Str& bytes) {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    EXPECT_NE(
        SHA256(
            reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(),
            digest.data()
        ),
        nullptr
    );
    Str result;
    for (const auto byte : digest) {
        result += fmt::format("{:02x}", byte);
    }
    return result;
}

inline void expectManifestResult(
    const SnapshotMetadata& snapshot, const Str& bytes
) {
    ASSERT_TRUE(bytes.ends_with('\n'));
    EXPECT_EQ(snapshot.manifestSha256, referenceSha256(bytes));
    EXPECT_NE(
        snapshot.manifestSha256,
        referenceSha256(bytes.substr(0, bytes.size() - 1))
    );
    const auto json = kc::Json::parse(bytes);
    const auto& manifest = snapshot.manifest;
    EXPECT_EQ(manifest.version, json["version"].get<u32>());
    EXPECT_EQ(manifest.name, json["name"].get<Str>());
    EXPECT_EQ(
        manifest.created.time_since_epoch().count(),
        json["created_at_ns"].get<i64>()
    );
    ASSERT_EQ(manifest.sources.size(), json["sources"].size());
    for (size_t i = 0; i < manifest.sources.size(); ++i) {
        const auto& source = manifest.sources[i];
        const auto& encoded = json["sources"][i];
        EXPECT_EQ(source.id, encoded["id"].get<u64>());
        EXPECT_EQ(source.root, encoded["root"].get<Str>());
        ASSERT_EQ(source.entries.size(), encoded["entries"].size());
        for (size_t j = 0; j < source.entries.size(); ++j) {
            const auto& entry = source.entries[j];
            const auto& value = encoded["entries"][j];
            EXPECT_EQ(entry.path, value["path"].get<Str>());
            EXPECT_EQ(entry.permissions, value["permissions"].get<u32>());
            EXPECT_EQ(entry.owner, value["owner"].get<u64>());
            EXPECT_EQ(entry.group, value["group"].get<u64>());
            EXPECT_EQ(
                entry.modified.time_since_epoch().count(),
                value["modified_ns"].get<i64>()
            );
            switch (entry.type) {
                case SnapshotEntry::Type::file:
                    EXPECT_EQ(value["type"].get<Str>(), "file");
                    EXPECT_EQ(entry.size, value["size"].get<u64>());
                    EXPECT_EQ(entry.sha256, value["sha256"].get<Str>());
                    break;
                case SnapshotEntry::Type::directory:
                    EXPECT_EQ(value["type"].get<Str>(), "directory");
                    break;
                case SnapshotEntry::Type::symlink:
                    EXPECT_EQ(value["type"].get<Str>(), "symlink");
                    EXPECT_EQ(entry.linkTarget, value["target"].get<Str>());
                    break;
            }
        }
    }
}

}  // namespace anchor::test
