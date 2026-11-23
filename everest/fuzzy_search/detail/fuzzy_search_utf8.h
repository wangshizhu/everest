/**
 * @file fuzzy_search_utf8.h
 * @brief UTF-8 严格解码与 ASCII 大小写折叠工具。
 */

#ifndef FUZZY_SEARCH_UTF8_H_
#define FUZZY_SEARCH_UTF8_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "common/define.h"
#include "fuzzy_search/fuzzy_search_types.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief UTF-8 解码结果。
 */
struct FuzzySearchDecodeResult {
    // 解码状态；遇到非法 UTF-8 时为 kInvalidUtf8。
    FuzzySearchStatus status = FuzzySearchStatus::kOk;
    // 实际保存的码点数，上限为存储缓冲大小与 max_codepoints + 1 中的较小值。
    std::size_t codepoint_count = 0;
};

/**
 * @brief UTF-8 严格解码与 ASCII 大小写折叠工具。
 *
 * 不可实例化的静态工具类。解码拒绝 overlong、代理区与大于 U+10FFFF 的码点。
 */
class FuzzySearchUtf8 final {
public:
    /**
     * @brief 禁止实例化。
     */
    FuzzySearchUtf8() = delete;

    /**
     * @brief 严格解码 UTF-8，把码点写入调用方提供的缓冲。
     *
     * 最多保存 max_codepoints + 1 个码点（受 out 大小限制）；超出后仍完整校验剩余字节，
     * 因此非法字节优先以 kInvalidUtf8 返回，而不是被长度上限掩盖。
     *
     * @param input 输入字节序列，不要求以 '\0' 结尾。
     * @param max_codepoints 调用方允许的码点数上限。
     * @param out 码点输出缓冲，长度应不小于 max_codepoints + 1；由调用方持有。
     * @return 解码状态与实际码点数；超出上限时 codepoint_count 大于 max_codepoints。
     *
     * @note 不分配内存、不抛异常。
     */
    [[nodiscard]] static FuzzySearchDecodeResult Decode(std::string_view input, std::uint32_t max_codepoints, std::span<char32_t> out) noexcept {
        FuzzySearchDecodeResult result;
        const std::size_t store_limit = std::min<std::size_t>(out.size(), static_cast<std::size_t>(max_codepoints) + 1);

        std::size_t index = 0;
        while (index < input.size()) {
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
                result.status = FuzzySearchStatus::kInvalidUtf8;
                return result;
            }

            if (index + length > input.size()) {
                result.status = FuzzySearchStatus::kInvalidUtf8;
                return result;
            }
            for (std::size_t i = 1; i < length; ++i) {
                const auto continuation = static_cast<unsigned char>(input[index + i]);
                if ((continuation & 0xC0) != 0x80) {
                    result.status = FuzzySearchStatus::kInvalidUtf8;
                    return result;
                }
                codepoint = (codepoint << 6) | (continuation & 0x3F);
            }

            if (length == 2 && codepoint < 0x80) {
                result.status = FuzzySearchStatus::kInvalidUtf8;
                return result;
            }
            if (length == 3 && codepoint < 0x800) {
                result.status = FuzzySearchStatus::kInvalidUtf8;
                return result;
            }
            if (length == 4 && codepoint < 0x10000) {
                result.status = FuzzySearchStatus::kInvalidUtf8;
                return result;
            }
            if (codepoint > 0x10FFFF) {
                result.status = FuzzySearchStatus::kInvalidUtf8;
                return result;
            }
            if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                result.status = FuzzySearchStatus::kInvalidUtf8;
                return result;
            }

            if (result.codepoint_count < store_limit) {
                out[result.codepoint_count] = codepoint;
                ++result.codepoint_count;
            }
            index += length;
        }
        return result;
    }

    /**
     * @brief 原地折叠码点中的 ASCII A-Z 到 a-z。
     * @param codepoints 待折叠的码点序列，由调用方持有。
     */
    static void FoldCodepoints(std::span<char32_t> codepoints) noexcept {
        for (char32_t& codepoint : codepoints) {
            if (codepoint >= U'A' && codepoint <= U'Z') {
                codepoint = codepoint - U'A' + U'a';
            }
        }
    }

    /**
     * @brief 原地折叠字节中的 ASCII A-Z 到 a-z。
     *
     * UTF-8 自同步，ASCII 字节不会出现在多字节序列内部，因此字节级折叠是安全的。
     *
     * @param bytes 待折叠的 UTF-8 字节序列，由调用方持有。
     */
    static void FoldBytes(std::span<char> bytes) noexcept {
        for (char& byte : bytes) {
            if (byte >= 'A' && byte <= 'Z') {
                byte = static_cast<char>(byte - 'A' + 'a');
            }
        }
    }
};

NAMESPACE_EVEREST_END

#endif
