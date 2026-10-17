#pragma once

#include <unordered_map>
#include <vector>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/FileSystem.hh"
#include "base/FlatMap.hh"

namespace anchor {

struct Source {
    Path root;
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

    Path root;
    Nodes nodes;
};

}  // namespace anchor
