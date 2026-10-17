#include "TestSupport.hh"
#include "core/services/SourceTreeBuilder.hh"

namespace anchor::test {
using namespace testing;
using SourceTreeBuilderTest = TemporaryDirectory;

TEST_F(SourceTreeBuilderTest, MergesNestedAndDuplicateGlobMatches) {
    ASSERT_TRUE(fs.createDirectory(path("docs/nested")));
    for (const auto* file :
         {"docs/one.txt", "docs/two.txt", "docs/nested/deep.txt",
          "docs/ignored.bin"}) {
        ASSERT_TRUE(fs.touch(path(file)));
    }
    const auto tree = SourceTreeBuilder::build(
        fs, Source{root, {"docs/*.txt", "docs/one.txt", "docs/nested/*.txt"}}
    );
    EXPECT_EQ(tree.root.str(), root.str());
    ASSERT_EQ(tree.nodes.size(), 1);
    ASSERT_TRUE(tree.nodes.contains("docs"));
    const auto& docs = tree.nodes.at("docs");
    EXPECT_EQ(docs.name, "docs");
    EXPECT_EQ(docs.relativePath, "docs");
    EXPECT_EQ(docs.type, SourceTree::NodeType::directory);
    ASSERT_EQ(docs.children.size(), 3);
    for (const auto* name : {"one.txt", "two.txt"}) {
        ASSERT_TRUE(docs.children.contains(name));
        const auto& file = docs.children.at(name);
        EXPECT_EQ(file.name, name);
        EXPECT_EQ(file.relativePath, Str{"docs/"} + name);
        EXPECT_EQ(file.type, SourceTree::NodeType::file);
        EXPECT_TRUE(file.children.empty());
    }
    ASSERT_TRUE(docs.children.contains("nested"));
    const auto& nested = docs.children.at("nested");
    EXPECT_EQ(nested.name, "nested");
    EXPECT_EQ(nested.relativePath, "docs/nested");
    EXPECT_EQ(nested.type, SourceTree::NodeType::directory);
    ASSERT_EQ(nested.children.size(), 1);
    ASSERT_TRUE(nested.children.contains("deep.txt"));
    EXPECT_EQ(nested.children.at("deep.txt").type, SourceTree::NodeType::file);
    EXPECT_EQ(
        nested.children.at("deep.txt").relativePath, "docs/nested/deep.txt"
    );
    EXPECT_TRUE(nested.children.at("deep.txt").children.empty());
}

TEST_F(SourceTreeBuilderTest, SelectedDirectoryRemainsALeaf) {
    ASSERT_TRUE(fs.createDirectory(path("assets/subdirectory")));
    ASSERT_TRUE(fs.touch(path("assets/subdirectory/file")));
    const auto tree = SourceTreeBuilder::build(fs, Source{root, {"assets"}});
    ASSERT_EQ(tree.nodes.size(), 1);
    ASSERT_TRUE(tree.nodes.contains("assets"));
    const auto& assets = tree.nodes.at("assets");
    EXPECT_EQ(assets.name, "assets");
    EXPECT_EQ(assets.relativePath, "assets");
    EXPECT_EQ(assets.type, SourceTree::NodeType::directory);
    EXPECT_TRUE(assets.children.empty());
}

TEST_F(
    SourceTreeBuilderTest,
    WholeDirectorySelectionIncludesDescendantsInEitherOrder
) {
    ASSERT_TRUE(fs.createDirectory(path("assets")));
    ASSERT_TRUE(fs.touch(path("assets/one.txt")));
    ASSERT_TRUE(fs.touch(path("assets/two.bin")));
    ASSERT_TRUE(fs.touch(path("assets-old.txt")));
    for (const auto& includes : std::vector<std::vector<Str>>{
             {"assets", "assets-old.txt", "assets/one.txt"},
             {"assets/one.txt", "assets-old.txt", "assets"}
         }) {
        SCOPED_TRACE(includes.front());
        const auto tree = SourceTreeBuilder::build(fs, Source{root, includes});
        ASSERT_EQ(tree.nodes.size(), 2);
        ASSERT_TRUE(tree.nodes.contains("assets"));
        ASSERT_TRUE(tree.nodes.contains("assets-old.txt"));
        EXPECT_EQ(
            tree.nodes.at("assets").type, SourceTree::NodeType::directory
        );
        EXPECT_TRUE(tree.nodes.at("assets").children.empty());
        EXPECT_EQ(tree.nodes.at("assets").relativePath, "assets");
        EXPECT_EQ(
            tree.nodes.at("assets-old.txt").relativePath, "assets-old.txt"
        );
    }
}

TEST_F(
    SourceTreeBuilderTest, EmptyIncludesAndUnmatchedPatternsProduceEmptyTrees
) {
    for (const auto& includes :
         std::vector<std::vector<Str>>{{}, {"*.missing"}}) {
        const auto tree = SourceTreeBuilder::build(fs, Source{root, includes});
        EXPECT_EQ(tree.root.str(), root.str());
        EXPECT_TRUE(tree.nodes.empty());
    }
    EXPECT_TRUE(
        SourceTreeBuilder::build(fs, std::span<const Source>{}).empty()
    );
}

TEST_F(SourceTreeBuilderTest, BuildsMultipleSourcesInInputOrder) {
    ASSERT_TRUE(fs.createDirectory(path("first")));
    ASSERT_TRUE(fs.createDirectory(path("second")));
    ASSERT_TRUE(fs.touch(path("first/one.txt")));
    ASSERT_TRUE(fs.touch(path("second/two.txt")));
    const std::vector<Source> sources{
        {path("first"), {"*.txt"}},
        {path("second"), {"*.txt"}},
        {path("missing"), {}},
    };
    const auto trees = SourceTreeBuilder::build(fs, sources);
    ASSERT_EQ(trees.size(), 3);
    EXPECT_EQ(trees[0].root.str(), path("first").str());
    ASSERT_TRUE(trees[0].nodes.contains("one.txt"));
    EXPECT_EQ(trees[0].nodes.at("one.txt").relativePath, "one.txt");
    EXPECT_EQ(trees[1].root.str(), path("second").str());
    ASSERT_TRUE(trees[1].nodes.contains("two.txt"));
    EXPECT_EQ(trees[1].nodes.at("two.txt").relativePath, "two.txt");
    EXPECT_EQ(trees[2].root.str(), path("missing").str());
    EXPECT_TRUE(trees[2].nodes.empty());
}

TEST_F(SourceTreeBuilderTest, RootTrailingSeparatorDoesNotTruncateNames) {
    ASSERT_TRUE(fs.touch(path("file.txt")));
    const auto tree =
        SourceTreeBuilder::build(fs, Source{root + "/", {"*.txt"}});
    ASSERT_EQ(tree.nodes.size(), 1);
    ASSERT_TRUE(tree.nodes.contains("file.txt"));
    EXPECT_EQ(tree.nodes.at("file.txt").relativePath, "file.txt");
}

TEST_F(SourceTreeBuilderTest, WholeRootSelectionHasDotRelativePath) {
    ASSERT_TRUE(fs.touch(path("file.txt")));
    const auto tree = SourceTreeBuilder::build(fs, Source{root, {""}});
    ASSERT_EQ(tree.nodes.size(), 1);
    const auto& node = tree.nodes.begin()->second;
    EXPECT_EQ(node.name, ".");
    EXPECT_EQ(node.relativePath, ".");
    EXPECT_EQ(node.type, SourceTree::NodeType::directory);
    EXPECT_TRUE(node.children.empty());
}

}  // namespace anchor::test
