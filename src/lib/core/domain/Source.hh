#pragma once

#include <kcore/Concepts.hh>
#include <kcore/containers/FlatMap.hh>
#include <kcore/io/FileSystem.hh>
#include <unordered_map>
#include <vector>

#include "core/Core.hh"

namespace anchor {

struct Source {
    kc::Path root;
    std::vector<Str> includes;
};

struct SourceTree {
    enum class NodeType { file, directory };

    struct Node;
    using Nodes = std::unordered_map<Str, Node>;

    struct Node {
        NodeType type;
        Str relativePath;
        Str name;
        Nodes children;
    };

    kc::Path root;
    Nodes nodes;
};

}  // namespace anchor
