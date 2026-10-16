#pragma once

#include "base/Error.hh"
#include "base/FileSystem.hh"
#include "base/Singleton.hh"
#include "core/domain/Source.hh"

namespace anchor {

class SourceTreeBuilder : public StaticClass {
   public:
    static SourceTree build(const FileSystem& fs, const Source& source);

    static std::vector<SourceTree> build(
        const FileSystem& fs, std::span<const Source> sources
    );
};

}  // namespace anchor
