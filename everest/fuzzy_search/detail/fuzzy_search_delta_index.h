#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "fuzzy_search_gram_key.h"
#include "fuzzy_search_posting.h"

namespace metaverse::fuzzy_search_detail {

// 可变增量索引：gram -> 升序文档号列表。文档号按 Add 顺序追加，天然有序；
// 同一个名字内重复的 gram 通过 back() 判断去重。
template <bool BmpOnly>
class DeltaIndex {
public:
    // ==================== 公开类型 ====================
    using BigramKey = BigramKeyT<BmpOnly>;

    // ==================== 公开接口 ====================
    // 清空所有增量 posting。
    void Clear() {
        bigram_.clear();
        unigram_.clear();
    }

    // 预留哈希表容量，只是容量提示。
    void Reserve(std::size_t documentCount) {
        bigram_.reserve(documentCount / 4 + 16);
        unigram_.reserve(documentCount / 16 + 16);
    }

    // 追加一个 bigram posting；同一个文档重复出现时只保留一次。
    void AddBigram(BigramKey key, DocId docId) {
        std::vector<DocId>& postings = bigram_[key];
        if (postings.empty() || postings.back() != docId) {
            postings.push_back(docId);
        }
    }

    // 追加一个 unigram posting；同一个文档重复出现时只保留一次。
    void AddUnigram(std::uint32_t key, DocId docId) {
        std::vector<DocId>& postings = unigram_[key];
        if (postings.empty() || postings.back() != docId) {
            postings.push_back(docId);
        }
    }

    // 查找 bigram posting；不存在时返回 nullptr。
    const std::vector<DocId>* FindBigram(BigramKey key) const {
        const auto it = bigram_.find(key);
        return it == bigram_.end() ? nullptr : &it->second;
    }

    // 查找 unigram posting；不存在时返回 nullptr。
    const std::vector<DocId>* FindUnigram(std::uint32_t key) const {
        const auto it = unigram_.find(key);
        return it == unigram_.end() ? nullptr : &it->second;
    }

    // 返回 gram 表项数。
    std::size_t EntryCount() const noexcept {
        return bigram_.size() + unigram_.size();
    }

    // 返回 posting 中的文档号总数。
    std::size_t PostingCount() const noexcept {
        std::size_t count = 0;
        for (const auto& entry : bigram_) {
            count += entry.second.size();
        }
        for (const auto& entry : unigram_) {
            count += entry.second.size();
        }
        return count;
    }

    // 估算 delta 占用的内存字节数。
    std::size_t MemoryBytes() const noexcept {
        std::size_t bytes = 0;
        bytes += bigram_.size() * (sizeof(BigramKey) + sizeof(std::vector<DocId>) + 32);
        for (const auto& entry : bigram_) {
            bytes += entry.second.capacity() * sizeof(DocId);
        }
        bytes += unigram_.size() * (sizeof(std::uint32_t) + sizeof(std::vector<DocId>) + 32);
        for (const auto& entry : unigram_) {
            bytes += entry.second.capacity() * sizeof(DocId);
        }
        return bytes;
    }

private:
    // ==================== 成员变量 ====================
    std::unordered_map<BigramKey, std::vector<DocId>> bigram_;
    std::unordered_map<std::uint32_t, std::vector<DocId>> unigram_;
};

}  // namespace metaverse::fuzzy_search_detail
