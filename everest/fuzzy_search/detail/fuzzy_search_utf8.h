#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../fuzzy_search_types.h"

namespace metaverse::fuzzy_search_detail {

// 最多解码 maxCodepoints + 1 个码点；调用方通过 out.size() 判断是否超长。
// 严格校验 UTF-8：拒绝 overlong、代理区和大于 U+10FFFF 的码点。
inline FuzzySearchStatus DecodeUtf8(std::string_view input, std::uint32_t maxCodepoints, std::vector<char32_t>& out) {
    out.clear();
    out.reserve(std::min<std::size_t>(input.size(), static_cast<std::size_t>(maxCodepoints) + 1));

    std::size_t index = 0;
    while (index < input.size()) {
        if (out.size() > static_cast<std::size_t>(maxCodepoints)) {
            return FuzzySearchStatus::kOk;
        }

        const auto first = static_cast<unsigned char>(input[index]);
        char32_t codepoint = 0;
        std::size_t length = 0;
        if (first < 0x80) {
            codepoint = first;
            length = 1;
        } else if ((first & 0xE0) == 0xC0) {
            codepoint = first & 0x1F;
            length = 2;
        } else if ((first & 0xF0) == 0xE0) {
            codepoint = first & 0x0F;
            length = 3;
        } else if ((first & 0xF8) == 0xF0) {
            codepoint = first & 0x07;
            length = 4;
        } else {
            return FuzzySearchStatus::kInvalidUtf8;
        }

        if (index + length > input.size()) {
            return FuzzySearchStatus::kInvalidUtf8;
        }
        for (std::size_t i = 1; i < length; ++i) {
            const auto continuation = static_cast<unsigned char>(input[index + i]);
            if ((continuation & 0xC0) != 0x80) {
                return FuzzySearchStatus::kInvalidUtf8;
            }
            codepoint = (codepoint << 6) | (continuation & 0x3F);
        }

        if (length == 2 && codepoint < 0x80) {
            return FuzzySearchStatus::kInvalidUtf8;
        }
        if (length == 3 && codepoint < 0x800) {
            return FuzzySearchStatus::kInvalidUtf8;
        }
        if (length == 4 && codepoint < 0x10000) {
            return FuzzySearchStatus::kInvalidUtf8;
        }
        if (codepoint > 0x10FFFF) {
            return FuzzySearchStatus::kInvalidUtf8;
        }
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return FuzzySearchStatus::kInvalidUtf8;
        }

        out.push_back(codepoint);
        index += length;
    }
    return FuzzySearchStatus::kOk;
}

// 只折叠 ASCII A-Z 到 a-z，CJK、假名及其他码点原样保留。
inline void FoldAsciiCodepoints(std::vector<char32_t>& codepoints) {
    for (char32_t& codepoint : codepoints) {
        if (codepoint >= U'A' && codepoint <= U'Z') {
            codepoint = codepoint - U'A' + U'a';
        }
    }
}

// 按字节折叠 ASCII 大小写；UTF-8 的 ASCII 字节不会出现在多字节序列内部，因此安全。
inline void FoldAsciiBytes(std::string& bytes) {
    for (char& byte : bytes) {
        if (byte >= 'A' && byte <= 'Z') {
            byte = static_cast<char>(byte - 'A' + 'a');
        }
    }
}

}  // namespace metaverse::fuzzy_search_detail
