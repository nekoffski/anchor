#include "Random.hh"

namespace anchor {

RandomEngine::RandomEngine()
    : m_engine(std::random_device{}()), m_uuidGen(m_engine) {}

Str RandomEngine::uuid() { return uuids::to_string(m_uuidGen()); }

WithUuid::WithUuid() : m_uuid(RandomEngine::get().uuid()) {}

const Uuid& WithUuid::uuid() const { return m_uuid; }

}  // namespace anchor
