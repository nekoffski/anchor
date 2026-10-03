#include "Handle.hh"

namespace anchor {

void HandleKey::invalidate() {
    id = InvalidHandle;
    generation = 0u;
}

bool HandleKey::valid() const { return id != InvalidHandle; }

}  // namespace anchor
