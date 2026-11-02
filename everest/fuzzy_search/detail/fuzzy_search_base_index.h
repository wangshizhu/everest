#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

#include "fuzzy_search_gram_key.h"
#include "fuzzy_search_posting.h"

namespace metaverse::fuzzy_search_detail {

// 紧凑只读 base 索引：按 key 排序的 gram 表 + 扁平的 delta + varint posting。
// Build 时会合并重复的 (key, 文档号) 对。
template <bool BmpOnly>
class BaseIndex {
public:
    // ==================== 公开类型 ====================
    using BigramKey = BigramKeyT<BmpOnly>;

    // bigram 表项：key、posting 在字节缓冲区中的偏移、文档号个数。
    struct BigramEntry {
        BigramKey key;
        std::uint32_t offset;
        std::uint32_t count;
    };

    // unigram 表项：key、posting 在字节缓冲区中的偏移、文档号个数。
    struct UnigramEntry {
        std::uint32_t key;
        std::uint32_t offset;
        std::uint32_t count;
    };

    // ==================== 公开接口 ====================
    // 清空 base 表和 posting 缓冲区。
    void Clear() {
        bigramTable_.clear();
        bigramPostings_.clear();
        unigramTable_.clear();
        unigramPostings_.clear();
    }

    // 用 (gram, 文档号) 对构建 base；buildUnigram 为 false 时不构建 unigram。
    void Build(std::vector<std::pair<BigramKey, DocId>> bigrams, std::vector<std::pair<std::uint32_t, DocId>> unigrams, bool buildUnigram) {
        bigramTable_.clear();
        bigramPostings_.clear();
        unigramTable_.clear();
        unigramPostings_.clear();

        BuildTable(bigrams, bigramTable_, bigramPostings_);
        if (buildUnigram) {
            BuildTable(unigrams, unigramTable_, unigramPostings_);
        }
    }

    // 查找 bigram posting；找到时返回压缩字节和文档号个数。
    bool FindBigram(BigramKey key, std::span<const std::uint8_t>* data, std::uint32_t* count) const {
        const auto it = std::lower_bound(bigramTable_.begin(), bigramTable_.end(), key, [](const BigramEntry& entry, BigramKey value) { return entry.key < value; });
        if (it == bigramTable_.end() || it->key != key) {
            return false;
        }

        const auto next = std::next(it);
        const std::size_t beginOffset = it->offset;
        const std::size_t endOffset = next == bigramTable_.end() ? bigramPostings_.size() : next->offset;
        if (beginOffset > endOffset || endOffset > bigramPostings_.size()) {
            return false;
        }
        *data = std::span<const std::uint8_t>(bigramPostings_.data() + beginOffset, endOffset - beginOffset);
        *count = it->count;
        return true;
    }

    // 查找 unigram posting；找到时返回压缩字节和文档号个数。
    bool FindUnigram(std::uint32_t key, std::span<const std::uint8_t>* data, std::uint32_t* count) const {
        const auto it = std::lower_bound(unigramTable_.begin(), unigramTable_.end(), key, [](const UnigramEntry& entry, std::uint32_t value) { return entry.key < value; });
        if (it == unigramTable_.end() || it->key != key) {
            return false;
        }

        const auto next = std::next(it);
        const std::size_t beginOffset = it->offset;
        const std::size_t endOffset = next == unigramTable_.end() ? unigramPostings_.size() : next->offset;
        if (beginOffset > endOffset || endOffset > unigramPostings_.size()) {
            return false;
        }
        *data = std::span<const std::uint8_t>(unigramPostings_.data() + beginOffset, endOffset - beginOffset);
        *count = it->count;
        return true;
    }

    // 返回 gram 表项数。
    std::size_t EntryCount() const noexcept {
        return bigramTable_.size() + unigramTable_.size();
    }

    // 返回 posting 压缩缓冲区的字节数。
    std::size_t PostingBytes() const noexcept {
        return bigramPostings_.size() + unigramPostings_.size();
    }

    // 估算 base 占用的内存字节数。
    std::size_t MemoryBytes() const noexcept {
        const std::size_t bigramBytes = bigramTable_.capacity() * sizeof(BigramEntry) + bigramPostings_.capacity();
        const std::size_t unigramBytes = unigramTable_.capacity() * sizeof(UnigramEntry) + unigramPostings_.capacity();
        return bigramBytes + unigramBytes;
    }

private:
    // ==================== 私有静态辅助函数 ====================
    // 把 (gram, 文档号) 对排序、去重、分组，编码成 gram 表和扁平 posting。
    template <typename Entry, typename Pairs>
    static void BuildTable(Pairs& pairs, std::vector<Entry>& table, std::vector<std::uint8_t>& postings) {
        std::sort(pairs.begin(), pairs.end());
        postings.reserve(pairs.size() * 2 + 16);

        std::vector<DocId> uniqueIds;
        std::size_t begin = 0;
        while (begin < pairs.size()) {
            std::size_t end = begin + 1;
            while (end < pairs.size() && pairs[end].first == pairs[begin].first) {
                ++end;
            }

            uniqueIds.clear();
            uniqueIds.reserve(end - begin);
            DocId previous = 0;
            bool first = true;
            for (std::size_t i = begin; i < end; ++i) {
                const DocId docId = pairs[i].second;
                if (!first && docId == previous) {
                    continue;
                }
                uniqueIds.push_back(docId);
                previous = docId;
                first = false;
            }

            Entry entry;
            entry.key = pairs[begin].first;
            entry.offset = static_cast<std::uint32_t>(postings.size());
            entry.count = static_cast<std::uint32_t>(uniqueIds.size());
            EncodePostingList(uniqueIds, postings);
            table.push_back(entry);
            begin = end;
        }
    }

private:
    // ==================== 成员变量 ====================
    std::vector<BigramEntry> bigramTable_;
    std::vector<std::uint8_t> bigramPostings_;
    std::vector<UnigramEntry> unigramTable_;
    std::vector<std::uint8_t> unigramPostings_;
};

}  // namespace metaverse::fuzzy_search_detail
