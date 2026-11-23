/**
 * @file fuzzy_search_types.h
 * @brief fuzzy_search 库的公共配置、状态码与统计类型。
 */

#ifndef FUZZY_SEARCH_TYPES_H_
#define FUZZY_SEARCH_TYPES_H_

#include <cstddef>
#include <cstdint>

#include "common/define.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief 模糊搜索库的构造配置。
 *
 * 三个字段共同决定索引规模与查询下限；非法配置由 FuzzySearchSystem::ValidateOptions 拒绝。
 */
struct FuzzySearchOptions {
    // 允许的最大字符串长度，按 Unicode 码点计，取值范围 [2, kMaxStringLength]。
    std::uint32_t max_string_length = 64;
    // 允许的最小查询长度，按 Unicode 码点计，取值范围 [1, max_string_length]。
    std::uint32_t min_search_length = 1;
    // 是否只折叠 ASCII 大小写：A-Z 折叠为 a-z，CJK、假名及其他码点原样比较。
    bool case_insensitive = true;
};

/**
 * @brief 库对外返回的状态码。
 */
enum class FuzzySearchStatus : std::uint8_t {
    // 成功。
    kOk = 0,
    // 构造参数非法。
    kInvalidOptions,
    // 字符串少于 kMinStringLength 个码点。
    kStringTooShort,
    // 字符串超过 max_string_length 个码点。
    kStringTooLong,
    // 输入不是合法 UTF-8。
    kInvalidUtf8,
    // BmpOnly 模式下出现 BMP 之外的码点。
    kUnsupportedCharacter,
    // 查询短于 min_search_length。
    kQueryTooShort,
    // 查询长于 max_string_length。
    kQueryTooLong,
    // 查询为空。
    kInvalidQuery,
    // maxResults 不在 [1, kMaxResults] 区间内。
    kInvalidMaxResults,
    // 文档号超出 uint32 可表示范围。
    kTooManyDocuments,
    // 名字 arena 超出 uint32 可表示范围。
    kNameArenaFull,
    // 同一进程已存在一个存活实例。
    kAlreadyCreated,
};

/**
 * @brief 文档数、gram 数和内存占用的近似统计。
 *
 * 供监控与调优参考；内存字段是按容量估算的近似值，不是精确值。
 */
struct FuzzySearchStats {
    // 存活文档数。
    std::size_t live_documents = 0;
    // 文档总数，含墓碑。
    std::size_t total_documents = 0;
    // 墓碑文档数。
    std::size_t tombstone_documents = 0;
    // 名字 arena 的字节数。
    std::size_t name_bytes = 0;
    // base 索引的 gram 表项数。
    std::size_t base_gram_entries = 0;
    // base posting 的压缩字节数。
    std::size_t base_posting_bytes = 0;
    // delta 索引的 gram 数。
    std::size_t delta_gram_entries = 0;
    // delta posting 的文档号总数。
    std::size_t delta_postings = 0;
    // 估算内存占用，供监控参考。
    std::size_t estimated_bytes = 0;
    // 是否构建了 unigram 倒排。
    bool unigram_enabled = false;
};

NAMESPACE_EVEREST_END

#endif
