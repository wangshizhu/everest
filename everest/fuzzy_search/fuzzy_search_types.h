#pragma once

#include <cstddef>
#include <cstdint>

namespace metaverse {

// 模糊搜索库的构造配置：最大字符串长度、最小搜索长度和大小写折叠开关。
struct FuzzySearchOptions {
    // 允许的最大字符串长度，按 Unicode 码点计，必填且 >= 2。
    std::uint32_t maxStringLength = 0;
    // 允许的最小查询长度，按 Unicode 码点计，必填且在 1..maxStringLength 之间。
    std::uint32_t minSearchLength = 1;
    // 是否只折叠 ASCII 大小写：A-Z 折叠为 a-z，CJK、假名及其他码点原样比较。
    bool caseInsensitive = true;
};

// 库对外返回的状态码。
enum class FuzzySearchStatus : std::uint8_t {
    kOk = 0,                  // 成功
    kInvalidOptions,          // 构造参数非法
    kStringTooShort,          // 字符串少于 2 个码点
    kStringTooLong,           // 字符串超过 maxStringLength
    kInvalidUtf8,             // 输入不是合法 UTF-8
    kUnsupportedCharacter,    // BmpOnly 模式下出现 BMP 之外的码点
    kQueryTooShort,           // 查询短于 minSearchLength
    kQueryTooLong,            // 查询长于 maxStringLength
    kInvalidQuery,            // 查询为空或非法
    kInvalidMaxResults,       // maxResults 不在 [1, 500]
    kTooManyDocuments,        // 文档号或名字 arena 超出 uint32 可表示范围
    kAlreadyCreated,          // 同一进程已存在一个存活实例
};

// 文档数、gram 数和内存占用的近似统计。
struct FuzzySearchStats {
    std::size_t liveDocuments = 0;       // 存活文档数
    std::size_t totalDocuments = 0;      // 文档总数，含墓碑
    std::size_t tombstoneDocuments = 0;  // 墓碑文档数
    std::size_t nameBytes = 0;           // 名字 arena 的字节数
    std::size_t baseGramEntries = 0;     // base 索引的 gram 表项数
    std::size_t basePostingBytes = 0;    // base posting 的压缩字节数
    std::size_t deltaGramEntries = 0;    // delta 索引的 gram 数
    std::size_t deltaPostings = 0;       // delta posting 的文档号总数
    std::size_t estimatedBytes = 0;      // 估算内存占用，供监控参考
    bool unigramEnabled = false;         // 是否构建了 unigram 倒排
};

}  // namespace metaverse
