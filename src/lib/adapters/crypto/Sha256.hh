#pragma once

#include <openssl/evp.h>

#include <memory>

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

class Sha256 {
   public:
    static Result<Sha256> create();

    Sha256(Sha256&&) noexcept = default;
    Sha256& operator=(Sha256&&) noexcept = default;

    Result<void> update(std::span<const Byte> bytes);
    Result<Str> finish();

   private:
    using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

    explicit Sha256(Context context);

    Context m_context;
};

Result<Str> sha256(std::span<const Byte> bytes);

}  // namespace anchor
