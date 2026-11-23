/**
 * @file fuzzy_search_delta_index.h
 * @brief 可变增量索引。
 */

#ifndef FUZZY_SEARCH_DELTA_INDEX_H_
#define FUZZY_SEARCH_DELTA_INDEX_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "common/define.h"
#include "fuzzy_search/detail/fuzzy_search_gram_key.h"
#include "fuzzy_search/detail/fuzzy_search_posting.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief 可变增量索引：gram -> 升序文档号列表。
 *
 * 文档号按 Add 顺序追加，天然有序。不变量：同一个文档号只会连续追加到单个 gram 的末尾，
 * 因此去重只需与最后一个元素比较。
 *
 * @tparam BmpOnly 是否只支持 BMP 码点，决定 bigram key 的位宽。
 */
template <bool BmpOnly>
class FuzzySearchDeltaIndex final {
public:
    // bigram key 类型。
    using BigramKey = FuzzySearchBigramKeyT<BmpOnly>;

    /**
     * @brief 清空所有增量 posting。
     */
    void Clear() {
        bigram_.clear();
        unigram_.clear();
    }

    /**
     * @brief 预留哈希表容量。
     * @param document_count 预估的文档数，只是容量提示，不影响正确性。
     */
    void Reserve(std::size_t document_count) {
        bigram_.reserve(document_count / 4 + 16);
        unigram_.reserve(document_count / 16 + 16);
    }

    /**
     * @brief 追加一个 bigram posting。
     * @param key bigram key。
     * @param doc_id 内部文档号；同一个文档重复出现时只保留一次。
     */
    void AddBigram(BigramKey key, FuzzySearchDocId doc_id) {
        std::vector<FuzzySearchDocId>& postings = bigram_[key];
        assert(postings.empty() || postings.back() <= doc_id);
        if (postings.empty() || postings.back() != doc_id) {
            postings.push_back(doc_id);
        }
    }

    /**
     * @brief 追加一个 unigram posting。
     * @param key unigram key。
     * @param doc_id 内部文档号；同一个文档重复出现时只保留一次。
     */
    void AddUnigram(std::uint32_t key, FuzzySearchDocId doc_id) {
        std::vector<FuzzySearchDocId>& postings = unigram_[key];
        assert(postings.empty() || postings.back() <= doc_id);
        if (postings.empty() || postings.back() != doc_id) {
            postings.push_back(doc_id);
        }
    }

    /**
     * @brief 查找 bigram posting。
     * @param key bigram key。
     * @return 文档号升序视图；不存在时返回空 span。
     */
    [[nodiscard]] std::span<const FuzzySearchDocId> FindBigram(BigramKey key) const {
        const auto it = bigram_.find(key);
        if (it == bigram_.end()) {
            return {};
        }
        return std::span<const FuzzySearchDocId>(it->second);
    }

    /**
     * @brief 查找 unigram posting。
     * @param key unigram key。
     * @return 文档号升序视图；不存在时返回空 span。
     */
    [[nodiscard]] std::span<const FuzzySearchDocId> FindUnigram(std::uint32_t key) const {
        const auto it = unigram_.find(key);
        if (it == unigram_.end()) {
            return {};
        }
        return std::span<const FuzzySearchDocId>(it->second);
    }

    /**
     * @brief 返回 gram 表项数（bigram 与 unigram 之和）。
     * @return 表项数。
     */
    [[nodiscard]] std::size_t EntryCount() const noexcept {
        return bigram_.size() + unigram_.size();
    }

    /**
     * @brief 返回所有 posting 中的文档号总数。
     * @return 文档号总数。
     */
    [[nodiscard]] std::size_t PostingCount() const noexcept {
        std::size_t count = 0;
        for (const auto& entry : bigram_) {
            count += entry.second.size();
        }
        for (const auto& entry : unigram_) {
            count += entry.second.size();
        }
        return count;
    }

    /**
     * @brief 估算 delta 占用的内存字节数。
     * @return 按容量估算的字节数。
     */
    [[nodiscard]] std::size_t MemoryBytes() const noexcept {
        // 32 是哈希节点与桶开销的粗略近似值，只用于监控估算。
        constexpr std::size_t kNodeOverhead = 32;
        std::size_t bytes = 0;
        bytes += bigram_.size() * (sizeof(BigramKey) + sizeof(std::vector<FuzzySearchDocId>) + kNodeOverhead);
        for (const auto& entry : bigram_) {
            bytes += entry.second.capacity() * sizeof(FuzzySearchDocId);
        }
        bytes += unigram_.size() * (sizeof(std::uint32_t) + sizeof(std::vector<FuzzySearchDocId>) + kNodeOverhead);
        for (const auto& entry : unigram_) {
            bytes += entry.second.capacity() * sizeof(FuzzySearchDocId);
        }
        return bytes;
    }

private:
    // bigram -> 升序文档号列表。
    std::unordered_map<BigramKey, std::vector<FuzzySearchDocId>> bigram_;
    // unigram -> 升序文档号列表。
    std::unordered_map<std::uint32_t, std::vector<FuzzySearchDocId>> unigram_;
};

NAMESPACE_EVEREST_END

#endif
