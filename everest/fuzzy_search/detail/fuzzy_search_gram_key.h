#pragma once

#include <cstdint>
#include <type_traits>

namespace metaverse::fuzzy_search_detail {

// BmpOnly 为 true 时用 32 位 key，否则用 64 位 key。
template <bool BmpOnly>
using BigramKeyT = std::conditional_t<BmpOnly, std::uint32_t, std::uint64_t>;

// BmpOnly 为 true 时两个码点都必须 <= 0xFFFF（调用方已校验），key 为 (cp1 << 16) | cp2。
// BmpOnly 为 false 时 key 为 64 位，支持完整 Unicode 范围。
template <bool BmpOnly>
inline BigramKeyT<BmpOnly> MakeBigramKey(char32_t first, char32_t second) {
    if constexpr (BmpOnly) {
        return (static_cast<std::uint32_t>(first) << 16) | static_cast<std::uint32_t>(second);
    } else {
        return (static_cast<std::uint64_t>(first) << 32) | static_cast<std::uint64_t>(second);
    }
}

// 单字 key 直接使用 Unicode 码点，32 位足够表示 U+0000..U+10FFFF。
inline std::uint32_t MakeUnigramKey(char32_t codepoint) {
    return static_cast<std::uint32_t>(codepoint);
}

}  // namespace metaverse::fuzzy_search_detail
