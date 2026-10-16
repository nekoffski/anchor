#include "SourceTreeBuilder.hh"

#include <algorithm>
#include <filesystem>
#include <ranges>

#include "base/String.hh"

namespace anchor {

namespace {

void logRecursive(const SourceTree::Nodes& node, u64 level = 1) {
    for (const auto& [k, v] : node) {
        log::debug("{}{}", std::string(level * 2, ' '), k);
        logRecursive(v.children, level + 1);
    }
}

}  // namespace

SourceTree SourceTreeBuilder::build(
    const FileSystem& fs, const Source& source
) {
    SourceTree tree;
    tree.root = source.root;

    std::vector<Path> paths;
    for (const auto& include : source.includes) {
        auto p = fs.glob(Path::join(source.root, include).str());
        std::move(p.begin(), p.end(), std::back_inserter(paths));
    }

    if (paths.empty()) {
        return tree;
    }

    std::ranges::sort(paths, {}, [](const Path& path) {
        return std::filesystem::path{path.str()};
    });

    Str selectedPrefix;
    for (const auto& path : paths) {
        // A selected directory already includes all of its descendants.
        if (not selectedPrefix.empty() and path.startsWith(selectedPrefix)) {
            continue;
        }
        selectedPrefix = path.str();
        if (not selectedPrefix.ends_with("/")) {
            selectedPrefix += "/";
        }

        auto suffix = path.lexicallyRelative(source.root);
        auto root = tree.root;
        auto* level = &tree.nodes;

        for (const auto& component : split(suffix, "/")) {
            root = Path::join(root, component);
            auto& node = (*level)[component];

            node.name = component;
            node.type = fs.isDirectory(root) ? SourceTree::NodeType::directory
                                             : SourceTree::NodeType::file;
            level = &node.children;
        }
    }

    if (log::level() >= log::Level::debug) {
        log::debug("Source tree '{}':", tree.root.str());
        logRecursive(tree.nodes);
    }

    return tree;
}

std::vector<SourceTree> SourceTreeBuilder::build(
    const FileSystem& fs, std::span<const Source> sources
) {
    auto convert = [&](const Source& source) -> SourceTree {
        return build(fs, source);
    };

    return sources | std::views::transform(convert) |
           std::ranges::to<std::vector<SourceTree>>();
}

}  // namespace anchor
