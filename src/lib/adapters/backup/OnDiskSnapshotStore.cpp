#include "OnDiskSnapshotStore.hh"

#include <algorithm>
#include <kcore/Functional.hh>
#include <kcore/Random.hh>

#include "adapters/crypto/Sha256.hh"
#include "adapters/serialization/SnapshotJson.hh"

namespace anchor {
namespace {

Result<void> validateBackupName(const Str& name) {
    if (name.empty() || name == "." || name == ".." ||
        name.find_first_of("/\\") != Str::npos ||
        name.find('\0') != Str::npos) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid backup name"
        );
    }
    return {};
}

Result<kc::Path> validateDestination(
    const kc::FileSystem& fs, const Location& destination
) {
    if (destination.type != LocationType::local || destination.path.empty() ||
        destination.path.find('\0') != Str::npos) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid local snapshot destination"
        );
    }
    const auto path = kc::Path{destination.path}.resolveEnv();
    if (not fs.isDirectory(path)) {
        return Error::unexpected(
            kc::ErrorCode::fileSystemError,
            "Snapshot destination is unavailable: '{}'", path.str()
        );
    }
    return path.absolute().transform([](const kc::Path& path) {
        return path.normalized();
    });
}

Result<kc::Path> backupDirectory(
    const kc::FileSystem& fs, const Location& destination, const Str& backupName
) {
    return validateBackupName(backupName)
        .and_then([&] { return validateDestination(fs, destination); })
        .transform([&](const kc::Path& root) {
            return kc::Path::join(root, backupName);
        });
}

Result<void> validateFileType(
    const kc::FileSystem& fs, const kc::Path& path, kc::FileInfo::Type type
) {
    return fs.info(path).and_then(
        [&](const kc::FileInfo& info) -> Result<void> {
            if (info.type != type) {
                return Error::unexpected(
                    kc::ErrorCode::invalidArgument,
                    "Unexpected snapshot file type: '{}'", path.str()
                );
            }
            return {};
        }
    );
}

Result<void> ignoreMissingFile(const Error& error) {
    return error.code() == kc::ErrorCode::fileNotFound
               ? Result<void>{}
               : Error::unexpected(error);
}

Result<void> validatePathAbsent(
    const kc::FileSystem& fs, const kc::Path& path
) {
    return fs.info(path)
        .and_then([&](const kc::FileInfo&) -> Result<void> {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "Snapshot already exists: '{}'",
                path.str()
            );
        })
        .or_else(ignoreMissingFile);
}

kc::Path manifestPath(const kc::Path& root, const Uuid& uuid) {
    return kc::Path::join(root, uuid + ".json");
}

kc::Path payloadPath(
    const kc::Path& root, const Uuid& uuid, ArchiveType format
) {
    return kc::Path::join(
        root, uuid + (format == ArchiveType::directory
                          ? ""
                          : "." + archiveTypeToStr(format))
    );
}

kc::FileInfo::Type payloadType(ArchiveType format) {
    if (format == ArchiveType::directory) {
        return kc::FileInfo::Type::directory;
    }
    return kc::FileInfo::Type::regular;
}

struct StoredSnapshot {
    SnapshotSummary summary;
    ArchiveType format;
};

Result<StoredSnapshot> parseStoredSnapshot(
    const Str& contents, const kc::Path& root, const Str& backupName,
    const Uuid& uuid
) {
    return parseSnapshotManifestHeader(contents).and_then(
        [&](const SnapshotManifestHeader& header) -> Result<StoredSnapshot> {
            if (header.uuid != uuid || header.backupName != backupName) {
                return Error::unexpected(
                    kc::ErrorCode::invalidArgument,
                    "Snapshot manifest identity does not match its location"
                );
            }
            const auto location =
                Location::local(payloadPath(root, uuid, header.format));
            const SnapshotSummary summary{
                header.uuid, header.created, location
            };
            return StoredSnapshot{summary, header.format};
        }
    );
}

Result<StoredSnapshot> readStoredSnapshot(
    const kc::FileSystem& fs, const kc::Path& root, const Str& backupName,
    const Uuid& uuid
) {
    const auto path = manifestPath(root, uuid);
    return validateFileType(fs, path, kc::FileInfo::Type::regular)
        .and_then([&] { return fs.readFile(path); })
        .and_then([&](const Str& contents) {
            return parseStoredSnapshot(contents, root, backupName, uuid);
        })
        .transform_error([&](const Error& error) {
            return Error{
                error.code(), "Could not read manifest '{}': {}", path.str(),
                error.message()
            };
        });
}

Result<void> validatePayload(
    const kc::FileSystem& fs, const StoredSnapshot& snapshot
) {
    return validateFileType(
        fs, snapshot.summary.location.path, payloadType(snapshot.format)
    );
}

Result<void> validateSource(
    const kc::FileSystem& fs, const SnapshotMetadata& metadata,
    const StoredSnapshot& snapshot, const Str& contents
) {
    const std::span<const Byte> bytes{
        reinterpret_cast<const Byte*>(contents.data()), contents.size()
    };
    return sha256(bytes).and_then([&](const Str& digest) -> Result<void> {
        if (digest != metadata.manifestSha256) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument,
                "Snapshot manifest hash mismatch"
            );
        }
        if (snapshot.format != metadata.manifest.format ||
            snapshot.summary.created != metadata.manifest.created) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument,
                "Snapshot metadata does not match its manifest"
            );
        }
        return validateFileType(
            fs, kc::Path{metadata.location.path}.resolveEnv(),
            payloadType(snapshot.format)
        );
    });
}

struct SnapshotUpload {
    StoredSnapshot snapshot;
    kc::Path source;
    Str manifest;
};

Result<SnapshotUpload> prepareUpload(
    const kc::FileSystem& fs, const SnapshotMetadata& metadata,
    const kc::Path& root
) {
    const auto uuid = kc::parseUuid(metadata.manifest.uuid);
    if (not uuid || metadata.location.type != LocationType::local) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument,
            "Invalid snapshot UUID or source location"
        );
    }
    const auto source = kc::Path{metadata.location.path}.resolveEnv();
    return fs.readFile(manifestPath(source.parent(), *uuid))
        .and_then([&](Str contents) {
            auto parsed = parseStoredSnapshot(
                contents, root, metadata.manifest.backupName, *uuid
            );
            return std::move(parsed).and_then([&](StoredSnapshot snapshot) {
                return validateSource(fs, metadata, snapshot, contents)
                    .transform([&] {
                        return SnapshotUpload{
                            std::move(snapshot), source, std::move(contents)
                        };
                    });
            });
        });
}

Result<std::vector<kc::Path>> snapshotEntries(
    const kc::FileSystem& fs, const kc::Path& root
) {
    return validateFileType(fs, root, kc::FileInfo::Type::directory)
        .and_then([&] { return fs.listEntries(root); })
        .or_else([](const Error& error) -> Result<std::vector<kc::Path>> {
            if (error.code() == kc::ErrorCode::fileNotFound) {
                return std::vector<kc::Path>{};
            }
            return Error::unexpected(error);
        });
}

Result<void> appendSnapshotSummary(
    const kc::FileSystem& fs, const kc::Path& root, const Str& backupName,
    const kc::Path& path, std::vector<SnapshotSummary>& summaries
) {
    if (not path.endsWith(".json")) {
        return {};
    }
    const auto uuid = kc::parseUuid(path.filename());
    if (not uuid || *uuid != path.filename()) {
        return {};
    }
    return readStoredSnapshot(fs, root, backupName, *uuid)
        .and_then([&](const StoredSnapshot& snapshot) {
            return validatePayload(fs, snapshot)
                .transform([&] { summaries.push_back(snapshot.summary); })
                .or_else(ignoreMissingFile);
        });
}

bool newestSnapshotFirst(const SnapshotSummary& a, const SnapshotSummary& b) {
    return a.created != b.created ? a.created > b.created : a.uuid < b.uuid;
}

Result<std::vector<SnapshotSummary>> listStoredSnapshots(
    const kc::FileSystem& fs, const kc::Path& root, const Str& backupName
) {
    const auto collectSummaries =
        [&](const auto& entries) -> Result<std::vector<SnapshotSummary>> {
        std::vector<SnapshotSummary> summaries;
        for (const auto& path : entries) {
            const auto r =
                appendSnapshotSummary(fs, root, backupName, path, summaries);
            if (not r) {
                return Error::unexpected(r.error());
            }
        }
        std::ranges::sort(summaries, newestSnapshotFirst);
        return summaries;
    };
    return snapshotEntries(fs, root).and_then(collectSummaries);
}

Result<void> removeStoredSnapshot(
    const kc::FileSystem& fs, const kc::Path& root, const Str& backupName,
    const Uuid& id
) {
    const auto uuid = kc::parseUuid(id);
    if (not uuid) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid snapshot UUID"
        );
    }
    return validateFileType(fs, root, kc::FileInfo::Type::directory)
        .and_then([&] {
            return readStoredSnapshot(fs, root, backupName, *uuid);
        })
        .transform_error(
            [](const Error& error) {
                return error.code() == kc::ErrorCode::fileNotFound
                ? Error{kc::ErrorCode::fileNotFound, "Snapshot not found"} : error;
            }
        )
        .and_then([&](const StoredSnapshot& snapshot) {
            return kc::pipeline(
                [&] {
                    return validatePayload(fs, snapshot)
                        .or_else(ignoreMissingFile);
                },
                [&] { return fs.removeAll(snapshot.summary.location.path); },
                [&] { return fs.remove(manifestPath(root, *uuid)); }
            );
        });
}

}  // namespace

OnDiskSnapshotStore::OnDiskSnapshotStore(const kc::FileSystem& fs) : m_fs(fs) {}

Result<void> OnDiskSnapshotStore::publishUpload(
    const kc::Path& root, const kc::Path& source,
    const SnapshotSummary& snapshot, const Str& manifestContents
) {
    const auto& uuid = snapshot.uuid;
    const kc::Path payload{snapshot.location.path};
    const auto manifest = manifestPath(root, uuid);
    const auto staging = kc::Path::join(
        root, "." + uuid + "." + kc::RandomEngine::get().uuid() + ".partial"
    );
    KCORE_DEFER {
        if (auto r = m_fs.removeAll(staging); not r) {
            log::error(
                "Could not clean snapshot staging path '{}': {}", staging.str(),
                r.error()
            );
        }
    };
    const auto r = kc::pipeline(
        [&] { return m_fs.createDirectory(root); },
        [&] {
            return validateFileType(m_fs, root, kc::FileInfo::Type::directory);
        },
        [&] { return m_fs.copyRecursive(source, staging); }
    );
    if (not r) {
        return r;
    }
    std::lock_guard lock{m_mutex};
    return kc::pipeline(
        [&] { return validatePathAbsent(m_fs, payload); },
        [&] { return validatePathAbsent(m_fs, manifest); },
        [&] { return m_fs.rename(staging, payload); },
        [&] { return m_fs.writeFileAtomic(manifest, manifestContents); }
    );
}

Result<SnapshotMetadata> OnDiskSnapshotStore::put(
    const SnapshotMetadata& metadata, const Location& destination
) {
    return backupDirectory(m_fs, destination, metadata.manifest.backupName)
        .and_then([&](const kc::Path& root) {
            return prepareUpload(m_fs, metadata, root)
                .and_then([&](const SnapshotUpload& upload) {
                    auto r = publishUpload(
                        root, upload.source, upload.snapshot.summary,
                        upload.manifest
                    );
                    return std::move(r).transform([&] {
                        auto stored = metadata;
                        stored.name =
                            kc::Path{upload.snapshot.summary.location.path}
                                .filenameWithExtension();
                        stored.location = upload.snapshot.summary.location;
                        return stored;
                    });
                });
        });
}

Result<std::vector<SnapshotSummary>> OnDiskSnapshotStore::list(
    const Location& destination, const Str& backupName
) const {
    std::lock_guard lock{m_mutex};
    return backupDirectory(m_fs, destination, backupName)
        .and_then([&](const kc::Path& root) {
            return listStoredSnapshots(m_fs, root, backupName);
        });
}

Result<void> OnDiskSnapshotStore::remove(
    const Location& destination, const Str& backupName, const Uuid& uuid
) {
    std::lock_guard lock{m_mutex};
    return backupDirectory(m_fs, destination, backupName)
        .and_then([&](const kc::Path& root) {
            return removeStoredSnapshot(m_fs, root, backupName, uuid);
        });
}

}  // namespace anchor
