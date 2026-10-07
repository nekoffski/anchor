#pragma once

#include <algorithm>
#include <bitset>
#include <concepts>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Core.hh"

namespace anchor {

std::vector<Str> split(const Str& input, const Str& separator);

template <u64 N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }
    char value[N];
};

template <std::unsigned_integral T>
Str toBinaryString(T value) {
    return std::bitset<sizeof(T) * 8>(value).to_string();
}

Str toBinaryString(CBytesView bytes);

template <std::unsigned_integral T>
Str toHexString(T value) {
    return fmt::format("{:0{}x}", value, sizeof(T) * 2);
}

Str toHexString(CBytesView bytes);

Str hexDump(CBytesView bytes);

Str join(const std::vector<Str>& parts, const Str& separator);

}  // namespace anchor
