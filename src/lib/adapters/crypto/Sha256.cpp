#include "Sha256.hh"

#include <array>
#include <kcore/String.hh>

namespace anchor {

Sha256::Sha256(Context context) : m_context(std::move(context)) {}

Result<Sha256> Sha256::create() {
    Context context{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    if (not context) {
        return Error::unexpected(
            ErrorCode::internalError, "Could not allocate SHA-256 context"
        );
    }
    const auto initialized =
        EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr);
    if (initialized != 1) {
        return Error::unexpected(
            ErrorCode::internalError, "Could not initialize SHA-256"
        );
    }
    return Sha256{std::move(context)};
}

Result<void> Sha256::update(std::span<const Byte> bytes) {
    if (not m_context) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "SHA-256 context is closed"
        );
    }
    const auto updated =
        EVP_DigestUpdate(m_context.get(), bytes.data(), bytes.size());
    if (updated != 1) {
        m_context.reset();
        return Error::unexpected(
            ErrorCode::internalError, "Could not update SHA-256"
        );
    }
    return {};
}

Result<Str> Sha256::finish() {
    if (not m_context) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "SHA-256 context is closed"
        );
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest;
    unsigned int size = 0;
    const auto finished =
        EVP_DigestFinal_ex(m_context.get(), digest.data(), &size);
    m_context.reset();
    if (finished != 1) {
        return Error::unexpected(
            ErrorCode::internalError, "Could not finish SHA-256"
        );
    }
    return kc::toHexString(std::span{digest}.first(size));
}

Result<Str> sha256(std::span<const Byte> bytes) {
    auto hash = Sha256::create();
    if (not hash) {
        return Error::unexpected(hash.error());
    }
    const auto updated = hash->update(bytes);
    if (not updated) {
        return Error::unexpected(updated.error());
    }
    return hash->finish();
}

}  // namespace anchor
