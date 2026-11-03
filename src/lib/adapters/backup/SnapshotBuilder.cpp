#include "SnapshotBuilder.hh"

#include <algorithm>
#include <array>
#include <unordered_set>

#include "SnapshotManifestJson.hh"
#include "adapters/crypto/Sha256.hh"

namespace anchor {
namespace {

constexpr size_t bufferSize = 64 * 1024;

SnapshotEntry::Type snapshotEntryType(kc::FileInfo::Type type) {
    switch (type) {
        case kc::FileInfo::Type::regular:
            return SnapshotEntry::Type::file;
        case kc::FileInfo::Type::directory:
            return SnapshotEntry::Type::directory;
        case kc::FileInfo::Type::symlink:
            return SnapshotEntry::Type::symlink;
        case kc::FileInfo::Type::other:
            std::unreachable();
    }
    std::unreachable();
}

SnapshotEntry makeSnapshotEntry(const Str& relative, const kc::FileInfo& info) {
    return {
        .path = relative,
        .type = snapshotEntryType(info.type),
        .permissions = info.permissions,
        .owner = info.owner,
        .group = info.group,
        .modified = info.modified,
        .size = info.size,
        .linkTarget = info.linkTarget,
    };
}

class SourceWriter {
   public:
    SourceWriter(
        const kc::FileSystem& fs, const kc::Path& output,
        SnapshotWriter& writer, SnapshotSource& source
    )
        : m_fs(fs), m_output(output), m_writer(writer), m_source(source) {}

    Result<void> writeNodes(const SourceTree::Nodes& nodes) {
        std::vector<const SourceTree::Node*> ordered;
        for (const auto& [_, node] : nodes) {
            ordered.push_back(&node);
        }
        std::ranges::sort(ordered, {}, &SourceTree::Node::relativePath);
        for (const auto* node : ordered) {
            const auto path =
                kc::Path::join(m_source.root, node->relativePath).normalized();
            if (auto r = writePath(path, node->children.empty()); not r) {
                return r;
            }
            if (auto r = writeNodes(node->children); not r) {
                return r;
            }
        }
        return {};
    }

   private:
    Result<void> writePath(const kc::Path& path, bool recursive) {
        const auto relative =
            path.lexicallyRelative(m_source.root).normalized();
        const auto escapesSource =
            relative.str().empty() || relative.isAbsolute() ||
            relative.str() == ".." || relative.startsWith("../");
        if (escapesSource) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument,
                "Snapshot path is outside its source root: '{}'", path.str()
            );
        }
        if (not m_seen.insert(relative.str()).second) {
            return {};
        }
        auto info = m_fs.info(path);
        if (not info) {
            return Error::unexpected(info.error());
        }
        const auto invalidParent =
            not recursive && info->type != kc::FileInfo::Type::directory;
        if (invalidParent) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument,
                "Selected parent is not a directory: '{}'", path.str()
            );
        }
        if (info->type == kc::FileInfo::Type::other) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument,
                "Unsupported source file type: '{}'", path.str()
            );
        }
        if (info->type != kc::FileInfo::Type::symlink) {
            auto same = m_fs.equivalent(path, m_output);
            if (not same) {
                return Error::unexpected(same.error());
            }
            if (*same) {
                return Error::unexpected(
                    kc::ErrorCode::invalidArgument,
                    "Can't add snapshot to itself: '{}'", path.str()
                );
            }
        }
        if (relative.str() != ".") {
            if (auto r = writeEntry(path, relative.str(), *info); not r) {
                return r;
            }
        } else if (info->type != kc::FileInfo::Type::directory) {
            return Error::unexpected(
                kc::ErrorCode::invalidArgument,
                "Source root is not a directory: '{}'", path.str()
            );
        }
        if (recursive && info->type == kc::FileInfo::Type::directory) {
            auto children = m_fs.listEntries(path);
            if (not children) {
                return Error::unexpected(children.error());
            }
            std::ranges::sort(*children, {}, &kc::Path::str);
            for (const auto& child : *children) {
                if (auto r = writePath(child.normalized(), true); not r) {
                    return r;
                }
            }
        }
        return {};
    }

    Result<void> writeEntry(
        const kc::Path& path, const Str& relative, const kc::FileInfo& info
    ) {
        auto entry = makeSnapshotEntry(relative, info);
        const auto name = fmt::format("{}/{}", m_source.id, relative);
        if (auto r = m_writer.beginEntry(name, info); not r) {
            return r;
        }
        if (info.type == kc::FileInfo::Type::regular) {
            auto hash = copyData(path, info.size);
            if (not hash) {
                return Error::unexpected(hash.error());
            }
            entry.sha256 = std::move(*hash);
        }
        if (auto r = m_writer.endEntry(); not r) {
            return r;
        }
        m_source.entries.push_back(std::move(entry));
        return {};
    }

    Result<Str> copyData(const kc::Path& path, u64 expectedSize) {
        auto hash = Sha256::create();
        if (not hash) {
            return Error::unexpected(hash.error());
        }
        auto file = m_fs.openFile(path, kc::FileMode::read);
        if (not file) {
            return Error::unexpected(file.error());
        }
        std::array<Byte, bufferSize> buffer;
        u64 total = 0;
        for (;;) {
            auto count = (*file)->read(buffer);
            if (not count) {
                return Error::unexpected(count.error());
            }
            if (*count == 0) {
                break;
            }
            if (*count > expectedSize - total) {
                return Error::unexpected(
                    kc::ErrorCode::fileSystemError,
                    "File size changed while snapshotting '{}'", path.str()
                );
            }
            const auto bytes = std::span{buffer}.first(*count);
            if (auto r = m_writer.writeData(bytes); not r) {
                return Error::unexpected(r.error());
            }
            const auto updated = hash->update(bytes);
            if (not updated) {
                return Error::unexpected(updated.error());
            }
            total += *count;
        }
        if (total != expectedSize) {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError,
                "File size changed while snapshotting '{}'", path.str()
            );
        }
        if (auto r = (*file)->close(); not r) {
            return Error::unexpected(r.error());
        }
        return hash->finish();
    }

    const kc::FileSystem& m_fs;
    const kc::Path& m_output;
    SnapshotWriter& m_writer;
    SnapshotSource& m_source;
    std::unordered_set<Str> m_seen;
};

}  // namespace

SnapshotBuilder::SnapshotBuilder(const kc::FileSystem& fs) : m_fs(fs) {}

Result<SnapshotMetadata> SnapshotBuilder::write(
    const SnapshotArgs& args, const kc::Path& output, SnapshotWriter& writer
) const {
    SnapshotManifest manifest{
        .name = args.name,
        .created = std::chrono::time_point_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now()
        ),
    };
    for (const auto& tree : args.trees) {
        auto root = tree.root.absolute();
        if (not root) {
            return Error::unexpected(root.error());
        }
        auto& source = manifest.sources.emplace_back(
            SnapshotSource{
                .id = manifest.sources.size(),
                .root = root->normalized().str(),
            }
        );
        kc::FileInfo info{
            .type = kc::FileInfo::Type::directory,
            .permissions = 0755,
            .modified = manifest.created,
        };
        const auto started =
            writer.beginEntry(fmt::format("{}", source.id), info);
        if (not started) {
            return Error::unexpected(started.error());
        }
        if (auto r = writer.endEntry(); not r) {
            return Error::unexpected(r.error());
        }
        SourceWriter sourceWriter{m_fs, output, writer, source};
        if (auto r = sourceWriter.writeNodes(tree.nodes); not r) {
            return Error::unexpected(r.error());
        }
    }
    auto json = serializeSnapshotManifest(manifest);
    if (not json) {
        return Error::unexpected(json.error());
    }
    const auto bytes =
        std::span{reinterpret_cast<const Byte*>(json->data()), json->size()};
    auto digest = sha256(bytes);
    if (not digest) {
        return Error::unexpected(digest.error());
    }
    kc::FileInfo info{
        .type = kc::FileInfo::Type::regular,
        .size = json->size(),
        .permissions = 0600,
        .modified = manifest.created,
    };
    if (auto r = writer.beginEntry("manifest.json", info); not r) {
        return Error::unexpected(r.error());
    }
    if (auto r = writer.writeData(bytes); not r) {
        return Error::unexpected(r.error());
    }
    if (auto r = writer.endEntry(); not r) {
        return Error::unexpected(r.error());
    }
    return SnapshotMetadata{
        .name = output.filenameWithExtension(),
        .location = Location::local(output.str()),
        .manifest = std::move(manifest),
        .manifestSha256 = std::move(*digest),
    };
}

}  // namespace anchor
