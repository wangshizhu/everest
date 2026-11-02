#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace metaverse::fuzzy_search_detail {

// 内部文档号，uint32 足够覆盖单实例的数据规模。
using DocId = std::uint32_t;

// 把 value 按 LEB128 变长编码追加到 out。
inline void AppendVarint(std::uint32_t value, std::vector<std::uint8_t>& out) {
    while (value >= 0x80) {
        out.push_back(static_cast<std::uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(value));
}

// 把严格递增的文档号列表编码成 delta + varint。
inline void EncodePostingList(std::span<const DocId> ids, std::vector<std::uint8_t>& out) {
    DocId previous = 0;
    bool first = true;
    for (DocId id : ids) {
        const std::uint32_t delta = first ? id : id - previous;
        AppendVarint(delta, out);
        previous = id;
        first = false;
    }
}

// 只读游标：逐个解码 base 中 delta + varint 编码的文档号。
class PackedPostingCursor {
public:
    // ==================== 构造 ====================
    PackedPostingCursor(std::span<const std::uint8_t> data, std::uint32_t count) : data_(data), remaining_(count) {}

public:
    // ==================== 公开接口 ====================
    // 是否还有未读取的文档号。
    bool HasNext() const noexcept { return remaining_ > 0; }

    // 读取下一个文档号；数据损坏或读完后返回 false。
    bool Next(DocId* out) {
        if (remaining_ == 0 || out == nullptr) {
            return false;
        }

        std::uint32_t delta = 0;
        int shift = 0;
        while (true) {
            if (offset_ >= data_.size() || shift >= 32) {
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
    // ==================== 成员变量 ====================
    std::span<const std::uint8_t> data_;
    std::uint32_t remaining_ = 0;
    std::size_t offset_ = 0;
    DocId previous_ = 0;
    bool first_ = true;
};

}  // namespace metaverse::fuzzy_search_detail
