#include "Core.hh"

#include "Random.hh"

namespace anchor {

Tag<Str> Tag<Str>::fromUuid() { return Tag<Str>{RandomEngine::get().uuid()}; }

}  // namespace anchor
