#pragma once

#include <kcore/Singleton.hh>
#include <kcore/io/FileSystem.hh>

#include "core/Error.hh"
#include "core/domain/Source.hh"

namespace anchor {

class SourceTreeBuilder : public kc::StaticClass {
   public:
    static SourceTree build(const kc::FileSystem& fs, const Source& source);

    static std::vector<SourceTree> build(
        const kc::FileSystem& fs, std::span<const Source> sources
    );
};

}  // namespace anchor
