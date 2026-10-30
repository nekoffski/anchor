#include "DirectorySnapshotter.hh"

#include <kcore/Log.hh>

#include "core/Error.hh"

namespace anchor {

DirectorySnapshotter::DirectorySnapshotter(const kc::FileSystem& fs)
    : m_fs(fs) {}

Result<void> DirectorySnapshotter::traverse(
    const kc::Path& source, const kc::Path& destination,
    const SourceTree::Nodes& nodes
) {
    // TODO: refactor to use the relative path stored in node

    if (nodes.empty()) {  // leaf, copy and return
        if (auto r = m_fs.copyRecursive(source, destination); not r) {
            kc::log::error(
                "Failed to copy node from '{}' to '{}' - {}", source.str(),
                destination.str(), r.error()
            );
            return Error::unexpected(r.error());
        }
    }

    for (const auto& node : nodes | std::views::values) {
        if (auto r = m_fs.createDirectory(destination); not r) {
            kc::log::error(
                "Failed to create directory '{}'", destination.str()
            );
            return Error::unexpected(r.error());
        }

        const auto d = kc::Path::join(destination, node.name);
        const auto s = kc::Path::join(source, node.name);

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
    const auto path = kc::Path::join(workspace, name);

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
        .name = name, .location = Location::local(path.str())
    };

    return metadata;
}

}  // namespace anchor
