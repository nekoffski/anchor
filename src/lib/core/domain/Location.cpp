#include "Location.hh"

#include "base/String.hh"

namespace anchor {

Result<LocationType> locationTypeFromStr(const Str& str) {
    if (str == "local") {
        return LocationType::local;
    } else if (str == "s3") {
        return LocationType::s3;
    }
    return Error::unexpected(
        ErrorCode::invalidArgument, "Invalid location type: {}", str
    );
}

Str locationTypeToStr(LocationType type) {
    switch (type) {
        case LocationType::local:
            return "local";
        case LocationType::s3:
            return "s3";
    }
    std::unreachable();
}

Result<Location> Location::fromStr(const Str& str) {
    static const Str separator = "://";
    auto parts = split(str, separator);

    if (parts.size() != 2) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid location format"
        );
    }

    auto type = locationTypeFromStr(parts[0]);

    if (not type) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid location type: {}", parts[0]
        );
    }

    return Location{.type = *type, .path = parts[1]};
}

Location Location::s3(const Str& path) {
    return Location{.type = LocationType::s3, .path = path};
}

Location Location::local(const Path& path) {
    return Location{.type = LocationType::local, .path = path.str()};
}

Str Location::str() const {
    return fmt::format("{}://{}", locationTypeToStr(type), path);
}

}  // namespace anchor
