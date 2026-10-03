#pragma once

#include "Log.hh"

namespace anchor {

template <typename T>
T unwrap(Result<T>&& result) {
    log::expect(result);
    return std::move(*result);
}

}  // namespace anchor
