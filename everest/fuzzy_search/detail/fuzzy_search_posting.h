/**
 * @file fuzzy_search_posting.h
 * @brief posting 的类型、delta + varint 编码工具与只读游标。
 */

#ifndef FUZZY_SEARCH_POSTING_H_
#define FUZZY_SEARCH_POSTING_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "common/define.h"

NAMESPACE_EVEREST_BEGIN

// 内部文档号，uint32 足够覆盖单实例的数据规模。
using FuzzySearchDocId = std::uint32_t;

/**
 * @brief 编码含义为「gram key + 文档号」的排序单元。
 * @tparam KeyT gram key 类型。
 */
template <typename KeyT>
struct FuzzySearchGramPair {
    // gram key。
    KeyT key = 0;
    // 内部文档号。
    FuzzySearchDocId doc_id = 0;
};

/**
 * @brief base 索引中一个 gram 的压缩 posting 视图。
 */
struct FuzzySearchPostingView {
    // delta + varint 编码后的字节序列，不拥有所有权。
    std::span<const std::uint8_t> data;
    // 解码出的文档号个数。
    std::uint32_t count = 0;
};

/**
 * @brief posting 的编码工具。
 *
 * 不可实例化的静态工具类；编码格式为 delta + LEB128 varint。
 */
class FuzzySearchPostingCodec final {
public:
    /**
     * @brief 禁止实例化。
     */
    FuzzySearchPostingCodec() = delete;

    /**
     * @brief 把 value 按 LEB128 变长编码追加到 out。
     *
     * @param value 待编码的无符号整数。
     * @param out 输出字节缓冲，由调用方持有。
     */
    static void AppendVarint(std::uint32_t value, std::vector<std::uint8_t>& out) {
        while (value >= 0x80) {
            out.push_back(static_cast<std::uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        out.push_back(static_cast<std::uint8_t>(value));
    }

    /**
     * @brief 把严格递增的文档号列表编码成 delta + varint。
     *
     * @param ids 严格递增的文档号序列。
     * @param out 输出字节缓冲，由调用方持有，编码结果追加在末尾。
     */
    static void EncodePostingList(std::span<const FuzzySearchDocId> ids, std::vector<std::uint8_t>& out) {
        FuzzySearchDocId previous = 0;
        bool first = true;
        for (FuzzySearchDocId id : ids) {
            const std::uint32_t delta = first ? id : id - previous;
            AppendVarint(delta, out);
            previous = id;
            first = false;
        }
    }
};

/**
 * @brief 只读游标：逐个解码 base 中 delta + varint 编码的文档号。
 */
class FuzzySearchPackedPostingCursor {
public:
    /**
     * @brief 在压缩字节上构造游标。
     *
     * @param data 压缩字节序列，不拥有所有权，生命周期需覆盖游标。
     * @param count 预期的文档号个数。
     */
    FuzzySearchPackedPostingCursor(std::span<const std::uint8_t> data, std::uint32_t count) noexcept : data_(data), remaining_(count) {}

    /**
     * @brief 是否还有未读取的文档号。
     *
     * @return 还有剩余时返回 true。
     */
    [[nodiscard]] bool HasNext() const noexcept {
        return remaining_ > 0;
    }

    /**
     * @brief 读取下一个文档号。
     *
     * @param out 输出文档号，不能为 nullptr。
     * @return 读取成功返回 true；读完后返回 false。
     *
     * @note Debug 下遇到内部不变量损坏（数据截断或 varint 过长）会触发断言；Release 下返回 false。
     */
    bool Next(FuzzySearchDocId* out) {
        if (remaining_ == 0 || out == nullptr) {
            return false;
        }

        std::uint32_t delta = 0;
        int shift = 0;
        while (true) {
            if (offset_ >= data_.size() || shift >= 32) {
                assert(false && "packed posting cursor hit corrupted data");
                return false;
            }
            const std::uint8_t byte = data_[offset_++];
            delta |= static_cast<std::uint32_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) {
                break;
            }
            shift += 7;
        }

        previous_ = first_ ? delta : previous_ + delta;
        first_ = false;
        *out = previous_;
        --remaining_;
        return true;
    }

private:
    // 压缩字节序列。
    std::span<const std::uint8_t> data_;
    // 剩余待读取的文档号个数。
    std::uint32_t remaining_ = 0;
    // 当前读取位置。
    std::size_t offset_ = 0;
    // 上一个解码出的文档号，用于 delta 累加。
    FuzzySearchDocId previous_ = 0;
    // 是否尚未读取第一个文档号。
    bool first_ = true;
};

NAMESPACE_EVEREST_END

#endif
