/**
 * @file fuzzy_search_base_index.h
 * @brief 紧凑只读 base 索引。
 */

#ifndef FUZZY_SEARCH_BASE_INDEX_H_
#define FUZZY_SEARCH_BASE_INDEX_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "common/define.h"
#include "fuzzy_search/detail/fuzzy_search_gram_key.h"
#include "fuzzy_search/detail/fuzzy_search_posting.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief base 索引的 gram 表项。
 * @tparam KeyT gram key 类型。
 */
template <typename KeyT>
struct FuzzySearchBaseGramEntry {
    // gram key，表内按升序排列。
    KeyT key = 0;
    // posting 在字节缓冲区中的起始偏移。
    std::uint32_t offset = 0;
    // posting 中的文档号个数。
    std::uint32_t count = 0;
};

/**
 * @brief 紧凑只读 base 索引。
 *
 * 结构为按 key 排序的 gram 表加扁平的 delta + varint posting 缓冲区；
 * Build 时会合并重复的 (key, 文档号) 对。
 *
 * @tparam BmpOnly 是否只支持 BMP 码点，决定 bigram key 的位宽。
 */
template <bool BmpOnly>
class FuzzySearchBaseIndex final {
public:
    // bigram key 类型。
    using BigramKey = FuzzySearchBigramKeyT<BmpOnly>;

    /**
     * @brief 清空 gram 表与 posting 缓冲区。
     */
    void Clear() {
        bigram_table_.clear();
        bigram_postings_.clear();
        unigram_table_.clear();
        unigram_postings_.clear();
    }

    /**
     * @brief 用 (gram, 文档号) 对构建 base 索引。
     *
     * @param bigrams bigram 对，构建后被消费。
     * @param unigrams unigram 对，仅在 build_unigram 为 true 时使用，构建后被消费。
     * @param build_unigram 是否构建 unigram 表。
     */
    void Build(std::vector<FuzzySearchGramPair<BigramKey>> bigrams, std::vector<FuzzySearchGramPair<std::uint32_t>> unigrams, bool build_unigram) {
        bigram_table_.clear();
        bigram_postings_.clear();
        unigram_table_.clear();
        unigram_postings_.clear();

        BuildTable(bigrams, bigram_table_, bigram_postings_);
        if (build_unigram) {
            BuildTable(unigrams, unigram_table_, unigram_postings_);
        }
    }

    /**
     * @brief 查找 bigram posting。
     * @param key bigram key。
     * @return 找到时返回压缩视图；否则返回空。
     */
    [[nodiscard]] std::optional<FuzzySearchPostingView> FindBigram(BigramKey key) const {
        return FindPosting(bigram_table_, key, bigram_postings_);
    }

    /**
     * @brief 查找 unigram posting。
     * @param key unigram key。
     * @return 找到时返回压缩视图；否则返回空。
     */
    [[nodiscard]] std::optional<FuzzySearchPostingView> FindUnigram(std::uint32_t key) const {
        return FindPosting(unigram_table_, key, unigram_postings_);
    }

    /**
     * @brief 返回 gram 表项数（bigram 与 unigram 之和）。
     * @return 表项数。
     */
    [[nodiscard]] std::size_t EntryCount() const noexcept {
        return bigram_table_.size() + unigram_table_.size();
    }

    /**
     * @brief 返回 posting 压缩缓冲区的字节数。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t PostingBytes() const noexcept {
        return bigram_postings_.size() + unigram_postings_.size();
    }

    /**
     * @brief 估算 base 占用的内存字节数。
     * @return 按容量估算的字节数，含表项与 posting 缓冲区。
     */
    [[nodiscard]] std::size_t MemoryBytes() const noexcept {
        const std::size_t bigram_bytes = bigram_table_.capacity() * sizeof(FuzzySearchBaseGramEntry<BigramKey>) + bigram_postings_.capacity();
        const std::size_t unigram_bytes = unigram_table_.capacity() * sizeof(FuzzySearchBaseGramEntry<std::uint32_t>) + unigram_postings_.capacity();
        return bigram_bytes + unigram_bytes;
    }

private:
    /**
     * @brief 在已排序的 gram 表中二分查找 posting。
     *
     * @tparam KeyT gram key 类型。
     * @param table 按 key 升序排列的 gram 表。
     * @param key 待查找的 key。
     * @param postings 所有 posting 共享的字节缓冲区。
     * @return 找到时返回压缩视图；否则返回空。
     */
    template <typename KeyT>
    [[nodiscard]] static std::optional<FuzzySearchPostingView> FindPosting(const std::vector<FuzzySearchBaseGramEntry<KeyT>>& table, KeyT key,
                                                                           const std::vector<std::uint8_t>& postings) {
        const auto it = std::lower_bound(table.begin(), table.end(), key, [](const FuzzySearchBaseGramEntry<KeyT>& entry, KeyT value) { return entry.key < value; });
        if (it == table.end() || it->key != key) {
            return std::nullopt;
        }

        const auto next = std::next(it);
        const std::size_t begin_offset = it->offset;
        const std::size_t end_offset = next == table.end() ? postings.size() : next->offset;
        if (begin_offset > end_offset || end_offset > postings.size()) {
            return std::nullopt;
        }

        FuzzySearchPostingView view;
        view.data = std::span<const std::uint8_t>(postings.data() + begin_offset, end_offset - begin_offset);
        view.count = it->count;
        return view;
    }

    /**
     * @brief 把 (gram, 文档号) 对排序、分组、去重并编码成 gram 表与扁平 posting。
     *
     * @tparam KeyT gram key 类型。
     * @param pairs 待构建的 gram 对，构建后被消费。
     * @param table 输出的 gram 表。
     * @param postings 输出的 posting 字节缓冲区。
     */
    template <typename KeyT>
    static void BuildTable(std::vector<FuzzySearchGramPair<KeyT>> pairs, std::vector<FuzzySearchBaseGramEntry<KeyT>>& table, std::vector<std::uint8_t>& postings) {
        std::sort(pairs.begin(), pairs.end(), [](const FuzzySearchGramPair<KeyT>& lhs, const FuzzySearchGramPair<KeyT>& rhs) {
            if (lhs.key != rhs.key) {
                return lhs.key < rhs.key;
            }
            return lhs.doc_id < rhs.doc_id;
        });

        // 每个文档号最多编码 5 字节，这里按 2 字节近似；同时防止 size_t 乘法溢出。
        constexpr std::size_t kReserveFactor = 2;
        constexpr std::size_t kReservePadding = 16;
        constexpr std::size_t kMaxSafeSize = (std::numeric_limits<std::size_t>::max() - kReservePadding) / kReserveFactor;
        postings.reserve(pairs.size() > kMaxSafeSize ? kMaxSafeSize : pairs.size() * kReserveFactor + kReservePadding);

        std::vector<FuzzySearchDocId> unique_ids;
        std::size_t begin = 0;
        while (begin < pairs.size()) {
            std::size_t end = begin + 1;
            while (end < pairs.size() && pairs[end].key == pairs[begin].key) {
                ++end;
            }

            unique_ids.clear();
            unique_ids.reserve(end - begin);
            FuzzySearchDocId previous = 0;
            bool first = true;
            for (std::size_t i = begin; i < end; ++i) {
                const FuzzySearchDocId doc_id = pairs[i].doc_id;
                if (!first && doc_id == previous) {
                    continue;
                }
                unique_ids.push_back(doc_id);
                previous = doc_id;
                first = false;
            }

            FuzzySearchBaseGramEntry<KeyT> entry;
            entry.key = pairs[begin].key;
            entry.offset = static_cast<std::uint32_t>(postings.size());
            entry.count = static_cast<std::uint32_t>(unique_ids.size());
            FuzzySearchPostingCodec::EncodePostingList(unique_ids, postings);
            table.push_back(entry);
            begin = end;
        }
    }

private:
    // bigram gram 表，按 key 升序。
    std::vector<FuzzySearchBaseGramEntry<BigramKey>> bigram_table_;
    // bigram posting 字节缓冲区。
    std::vector<std::uint8_t> bigram_postings_;
    // unigram gram 表，按 key 升序。
    std::vector<FuzzySearchBaseGramEntry<std::uint32_t>> unigram_table_;
    // unigram posting 字节缓冲区。
    std::vector<std::uint8_t> unigram_postings_;
};

NAMESPACE_EVEREST_END

#endif
