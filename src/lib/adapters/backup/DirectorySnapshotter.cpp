#include "DirectorySnapshotter.hh"

#include "base/Log.hh"

namespace anchor {

DirectorySnapshotter::DirectorySnapshotter(const FileSystem& fs) : m_fs(fs) {}

Result<void> DirectorySnapshotter::traverse(
    const Path& source, const Path& destination, const SourceTree::Nodes& nodes
) {
    if (nodes.empty()) {  // leaf, copy and return
        if (auto r = m_fs.copyRecursive(source, destination); not r) {
            log::error(
                "Failed to copy node from '{}' to '{}' - {}", source.str(),
                destination.str(), r.error()
            );
            return Error::unexpected(r.error());
        }
    }

    for (const auto& node : nodes | std::views::values) {
        if (auto r = m_fs.createDirectory(destination); not r) {
            log::error("Failed to create directory '{}'", destination.str());
            return Error::unexpected(r.error());
        }

        const auto d = Path::join(destination, node.name);
        const auto s = Path::join(source, node.name);

        if (auto r = traverse(s, d, node.children); not r) {
            return r;
        }
    }

    return {};
}

Result<SnapshotMetadata> DirectorySnapshotter::snapshot(
    const SnapshotArgs& args
) {
    const auto& [name, trees, workspace] = args;

    const auto snapshotName = newSnapshotName(name);
    const auto path = Path::join(workspace, snapshotName);

    if (auto result = m_fs.createDirectory(path); not result) {
        return Error::unexpected(result.error());
    }

    for (const auto& tree : trees) {
        if (tree.nodes.empty()) {
            continue;
        }
        if (auto r = traverse(tree.root, path, tree.nodes); not r) {
            return Error::unexpected(r.error());
        }
    }

    SnapshotMetadata metadata{
        .name = snapshotName, .location = Location::local(path.str())
    };

    return metadata;
}

Str DirectorySnapshotter::newSnapshotName(const Str& backupName) {
    const auto ts = std::chrono::system_clock::now();
    return fmt::format("{}_{}", backupName, ts.time_since_epoch().count());
}

}  // namespace anchor
