#pragma once

#include <nlohmann/json.hpp>

#include "Error.hh"

namespace anchor {

using namespace nlohmann;

using Json = nlohmann::json;

Result<Json> parseJson(const Str& str);

}  // namespace anchor
