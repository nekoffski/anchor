#pragma once

#include <kcore/Concepts.hh>
#include <kcore/io/FileSystem.hh>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

enum class LocationType { local, s3 };

Result<LocationType> locationTypeFromStr(const Str& str);
Str locationTypeToStr(LocationType type);

struct Location {
    LocationType type;
    Str path;

    static Location s3(const Str& path);
    static Location local(const kc::Path& path);
    static Result<Location> fromStr(const Str& str);

    Str str() const;
};

}  // namespace anchor
