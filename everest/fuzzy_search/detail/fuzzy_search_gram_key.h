/**
 * @file fuzzy_search_gram_key.h
 * @brief gram key 的类型别名与打包工具。
 */

#ifndef FUZZY_SEARCH_GRAM_KEY_H_
#define FUZZY_SEARCH_GRAM_KEY_H_

#include <cstdint>
#include <type_traits>

#include "common/define.h"

NAMESPACE_EVEREST_BEGIN

// bigram key 类型：BmpOnly 为 true 时 32 位，否则 64 位。
template <bool BmpOnly>
using FuzzySearchBigramKeyT = std::conditional_t<BmpOnly, std::uint32_t, std::uint64_t>;

/**
 * @brief gram key 打包工具。
 *
 * 不可实例化的静态工具类。bigram key 由相邻两个码点拼接而成，unigram key 直接使用码点。
 */
class FuzzySearchGramKey final {
public:
    /**
     * @brief 禁止实例化。
     */
    FuzzySearchGramKey() = delete;

    /**
     * @brief 由相邻两个码点生成 bigram key。
     *
     * @tparam BmpOnly 为 true 时只使用每个码点的低 16 位。
     * @param first 前一个码点；BmpOnly 为 true 时调用方保证其 <= 0xFFFF。
     * @param second 后一个码点；BmpOnly 为 true 时调用方保证其 <= 0xFFFF。
     * @return BmpOnly 为 true 时返回 (first << 16) | second，否则返回 (first << 32) | second。
     */
    template <bool BmpOnly>
    [[nodiscard]] static FuzzySearchBigramKeyT<BmpOnly> MakeBigram(char32_t first, char32_t second) noexcept {
        if constexpr (BmpOnly) {
            return (static_cast<std::uint32_t>(first) << 16) | static_cast<std::uint32_t>(second);
        } else {
            return (static_cast<std::uint64_t>(first) << 32) | static_cast<std::uint64_t>(second);
        }
    }

    /**
     * @brief 由单个码点生成 unigram key。
     *
     * @param codepoint Unicode 码点，32 位足够表示 U+0000..U+10FFFF。
     * @return 码点本身。
     */
    [[nodiscard]] static std::uint32_t MakeUnigram(char32_t codepoint) noexcept {
        return static_cast<std::uint32_t>(codepoint);
    }
};

NAMESPACE_EVEREST_END

#endif
