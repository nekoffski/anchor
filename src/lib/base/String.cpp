#include "String.hh"

namespace anchor {

std::vector<Str> split(const Str& input, const Str& separator) {
    if (separator.empty()) {
        return {input};
    }
    std::vector<Str> result;
    u64 start = 0;
    u64 end;

    while ((end = input.find(separator, start)) != Str::npos) {
        result.push_back(input.substr(start, end - start));
        start = end + separator.length();
    }
    result.push_back(input.substr(start));
    return result;
}

Str toBinaryString(CBytesView bytes) {
    Str result;
    result.reserve(bytes.size() * 8);
    for (const auto byte : bytes) {
        result += std::bitset<8>(byte).to_string();
    }
    return result;
}

Str toHexString(CBytesView bytes) {
    Str result;
    result.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        result += fmt::format("{:02x}", byte);
    }
    return result;
}

Str hexDump(CBytesView bytes) {
    Str hex;
    Str ascii;
    hex.reserve(bytes.size() * 3);
    ascii.reserve(bytes.size());
    for (const auto byte : bytes) {
        hex += fmt::format("{:02x} ", byte);
        ascii += (byte >= 0x20 && byte < 0x7f) ? static_cast<char>(byte) : '.';
    }
    if (!hex.empty()) {
        hex.pop_back();
    }
    return fmt::format("{} | {}", hex, ascii);
}

Str join(const std::vector<Str>& parts, const Str& separator) {
    auto n = parts.size();

    if (n == 0) {
        return Str{};
    } else if (n == 1) {
        return parts[0];
    }

    Str out = parts[0];

    for (u64 i = 1u; i < n; ++i) {
        out += separator + parts[i];
    }

    return out;
}

}  // namespace anchor
