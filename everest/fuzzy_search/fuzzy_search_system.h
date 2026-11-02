#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "detail/fuzzy_search_base_index.h"
#include "detail/fuzzy_search_delta_index.h"
#include "detail/fuzzy_search_gram_key.h"
#include "detail/fuzzy_search_lifecycle.h"
#include "detail/fuzzy_search_log.h"
#include "detail/fuzzy_search_posting.h"
#include "detail/fuzzy_search_utf8.h"
#include "fuzzy_search_types.h"

namespace metaverse {

// 基于 UTF-8 的内存子串检索引擎：一个字符串关联一个 64 位 id。
// 单线程、不可拷贝、不可移动；每个服务进程通过 Create() 创建一个实例。
template <typename IdT = std::uint64_t, bool BmpOnly = false>
class FuzzySearchSystem final {
public:
    // ==================== 公开类型 ====================
    using Options = FuzzySearchOptions;
    using Status = FuzzySearchStatus;
    using Stats = FuzzySearchStats;

    // ==================== 编译期约束与常量 ====================
    // 只支持 64 位整数 id；字符串和 id 是一对一关系。
    static_assert(std::is_integral_v<IdT> && sizeof(IdT) == 8,"FuzzySearchSystem requires a 64-bit integral id type");

    // 单次查询返回上限，硬编码 500。
    static constexpr std::size_t kMaxResults = 500;
    // 字符串最小长度，固定 2 个码点。
    static constexpr std::uint32_t kMinStringLength = 2;

    // ==================== 静态函数 ====================
    // 唯一创建入口；参数非法时返回 nullptr，并通过 status 返回原因。
    static std::unique_ptr<FuzzySearchSystem> Create(const Options& options,Status* status = nullptr) {
        const Status result = ValidateOptions(options);
        if (status != nullptr) {
            *status = result;
        }
        if (result != Status::kOk) {
            FUZZY_SEARCH_LOG_ERROR("FuzzySearchSystem.Create failed: invalid options maxStringLength={}, minSearchLength={}",options.maxStringLength, options.minSearchLength);
            return nullptr;
        }
        if (!fuzzy_search_detail::CreationGuard::TryAcquire()) {
            if (status != nullptr) {
                *status = Status::kAlreadyCreated;
            }
            FUZZY_SEARCH_LOG_ERROR("FuzzySearchSystem.Create failed: an instance already exists in this process");
            return nullptr;
        }
        return std::unique_ptr<FuzzySearchSystem>(new FuzzySearchSystem(options));
    }

    // 只校验构造参数，不分配内存、不抛异常。
    static Status ValidateOptions(const Options& options) noexcept {
        if (options.maxStringLength < kMinStringLength) {
            return Status::kInvalidOptions;
        }
        if (options.minSearchLength == 0) {
            return Status::kInvalidOptions;
        }
        if (options.minSearchLength > options.maxStringLength) {
            return Status::kInvalidOptions;
        }
        return Status::kOk;
    }

public:
    // ==================== 构造/析构/拷贝控制 ====================
    ~FuzzySearchSystem() {
        fuzzy_search_detail::CreationGuard::Release();
    }

    // 禁止拷贝和移动，避免索引状态被复制或转移。
    FuzzySearchSystem(const FuzzySearchSystem&) = delete;
    FuzzySearchSystem& operator=(const FuzzySearchSystem&) = delete;
    FuzzySearchSystem(FuzzySearchSystem&&) = delete;
    FuzzySearchSystem& operator=(FuzzySearchSystem&&) = delete;

private:
    // 私有构造，只能通过 Create() 创建。
    explicit FuzzySearchSystem(const Options& options): options_(options), unigramEnabled_(options.minSearchLength == 1) {}

public:
    // ==================== 公开接口 ====================
    // 插入或覆盖：同一个 id 再次 Add 会替换旧名字，旧文档只打墓碑。
    Status Add(IdT id, std::string_view utf8Name) {
        std::vector<char32_t> codepoints;
        std::string foldedName;

        const Status status = DecodeName(utf8Name, codepoints, foldedName);
        if (status != Status::kOk) {
            return status;
        }

        if (documents_.size() >= std::numeric_limits<fuzzy_search_detail::DocId>::max()) {
            return Status::kTooManyDocuments;
        }

        if (nameArena_.size() + foldedName.size() > std::numeric_limits<std::uint32_t>::max()) {
            return Status::kTooManyDocuments;
        }

        // 同名 id 覆盖：旧文档保留在 posting 中但标记为墓碑。
        const auto existing = idToDoc_.find(id);
        if (existing != idToDoc_.end()) {
            documents_[existing->second].alive = false;
            idToDoc_.erase(existing);
        }

        const std::uint32_t nameOffset = static_cast<std::uint32_t>(nameArena_.size());
        nameArena_.append(foldedName);
        const fuzzy_search_detail::DocId docId = static_cast<fuzzy_search_detail::DocId>(documents_.size());
        documents_.push_back(DocumentRecord{id, nameOffset, static_cast<std::uint32_t>(foldedName.size()), true});
        idToDoc_.emplace(id, docId);

        // 新文档只写入 delta；Compact() 时再合并进 base。
        if (unigramEnabled_) {
            for (char32_t codepoint : codepoints) {
                deltaIndex_.AddUnigram(fuzzy_search_detail::MakeUnigramKey(codepoint), docId);
            }
        }

        for (std::size_t i = 0; i + 1 < codepoints.size(); ++i) {
            deltaIndex_.AddBigram(fuzzy_search_detail::MakeBigramKey<BmpOnly>(codepoints[i], codepoints[i + 1]), docId);
        }

        return Status::kOk;
    }

    // 幂等删除：id 不存在时直接返回 kOk，不报错。
    Status Remove(IdT id) {
        const auto existing = idToDoc_.find(id);
        if (existing == idToDoc_.end()) {
            return Status::kOk;
        }
        documents_[existing->second].alive = false;
        idToDoc_.erase(existing);
        return Status::kOk;
    }

    // 判断 id 当前是否存在。
    bool Contains(IdT id) const noexcept {
        return idToDoc_.find(id) != idToDoc_.end();
    }

    // 查询入口：visitor 返回 false 时提前停止；maxResults 必须在 [1, kMaxResults] 之间。
    template <typename Visitor>
    Status Search(std::string_view utf8Query, std::size_t maxResults, Visitor&& visitor) const {
        if (maxResults == 0 || maxResults > kMaxResults) {
            return Status::kInvalidMaxResults;
        }

        std::vector<char32_t> codepoints;
        std::string foldedQuery;
        const Status status = DecodeQuery(utf8Query, codepoints, foldedQuery);
        if (status != Status::kOk) {
            return status;
        }

        std::size_t matched = 0;
        auto emit = [this, &visitor, &matched, maxResults](fuzzy_search_detail::DocId docId) -> bool {
            const DocumentRecord& document = documents_[docId];
            if (!document.alive) {
                return true;
            }
            if (!visitor(document.id)) {
                return false;
            }
            ++matched;
            return matched < maxResults;
        };

        // 1 字查询走 unigram；只有 minSearchLength == 1 时才构建 unigram。
        if (codepoints.size() == 1) {
            if (!unigramEnabled_) {
                return Status::kQueryTooShort;
            }
            ForEachUnigramPosting(fuzzy_search_detail::MakeUnigramKey(codepoints[0]), emit);
            return Status::kOk;
        }
        // 2 字查询走单个 bigram，posting 就是精确结果，无需原文校验。
        if (codepoints.size() == 2) {
            ForEachBigramPosting(fuzzy_search_detail::MakeBigramKey<BmpOnly>(codepoints[0], codepoints[1]), emit);
            return Status::kOk;
        }

        // 3 字以上：取相邻 bigram 列表求交，再用名字原文校验连续性。
        const std::size_t gramCount = codepoints.size() - 1;
        std::vector<std::vector<fuzzy_search_detail::DocId>> postingLists(gramCount);
        for (std::size_t i = 0; i < gramCount; ++i) {
            MaterializeBigram(fuzzy_search_detail::MakeBigramKey<BmpOnly>(codepoints[i], codepoints[i + 1]), postingLists[i]);
            if (postingLists[i].empty()) {
                return Status::kOk;
            }
        }

        std::size_t shortest = 0;
        for (std::size_t i = 1; i < gramCount; ++i) {
            if (postingLists[i].size() < postingLists[shortest].size()) {
                shortest = i;
            }
        }

        // 从最短的 posting 列表开始遍历，减少候选量。
        const std::string_view foldedView(foldedQuery);
        for (fuzzy_search_detail::DocId docId : postingLists[shortest]) {
            if (!documents_[docId].alive) {
                continue;
            }
            if (!PresentInAllPostings(docId, postingLists, shortest)) {
                continue;
            }
            if (!NameContains(docId, foldedView)) {
                continue;
            }
            if (!emit(docId)) {
                break;
            }
        }
        return Status::kOk;
    }

    // 查询并把结果写入 vector；结果顺序不承诺，最多 maxResults 条。
    Status Search(std::string_view utf8Query, std::size_t maxResults,std::vector<IdT>& out) const {
        out.clear();
        if (maxResults == 0 || maxResults > kMaxResults) {
            return Status::kInvalidMaxResults;
        }
        out.reserve(maxResults);
        return Search(utf8Query, maxResults, [&out, maxResults](IdT id) {
            out.push_back(id);
            return out.size() < maxResults;
        });
    }

    // 容量提示，只影响分配次数，不影响索引正确性。
    void Reserve(std::size_t documentCount, std::size_t nameBytes = 0) {
        documents_.reserve(documentCount);
        idToDoc_.reserve(documentCount);
        nameArena_.reserve(nameBytes);
        deltaIndex_.Reserve(documentCount);
    }

    // 合并 base + delta，清理墓碑并重建紧凑 base；同步执行，建议在低峰调用。
    void Compact() {
        const std::size_t liveDocuments = idToDoc_.size();
        const std::size_t totalDocuments = documents_.size();
        if (totalDocuments == 0 && deltaIndex_.EntryCount() == 0) {
            FUZZY_SEARCH_LOG_DEBUG("FuzzySearchSystem.Compact skipped: empty");
            return;
        }

        const auto start = std::chrono::steady_clock::now();
        FUZZY_SEARCH_LOG_INFO("FuzzySearchSystem.Compact start: live={}, tombstones={}, nameBytes={}", liveDocuments, totalDocuments - liveDocuments, nameArena_.size());

        // 只保留存活文档，并重新分配连续的内部文档号。
        std::vector<DocumentRecord> newDocuments;
        std::unordered_map<IdT, fuzzy_search_detail::DocId> newIdToDoc;
        std::string newNameArena;
        newDocuments.reserve(liveDocuments);
        newIdToDoc.reserve(liveDocuments);
        newNameArena.reserve(nameArena_.size());

        const std::size_t maxBigrams = options_.maxStringLength > 0 ? options_.maxStringLength - 1 : 0;
        GramPairs gramPairs;
        gramPairs.bigrams.reserve(liveDocuments * maxBigrams);
        if (unigramEnabled_) {
            gramPairs.unigrams.reserve(liveDocuments * options_.maxStringLength);
        }

        for (fuzzy_search_detail::DocId oldDocId = 0; oldDocId < documents_.size(); ++oldDocId) {
            const DocumentRecord& document = documents_[oldDocId];
            if (!document.alive) {
                continue;
            }
            if (newNameArena.size() + document.nameLength > std::numeric_limits<std::uint32_t>::max()) {
                FUZZY_SEARCH_LOG_ERROR("FuzzySearchSystem.Compact failed: name arena too large");
                return;
            }

            const fuzzy_search_detail::DocId newDocId = static_cast<fuzzy_search_detail::DocId>(newDocuments.size());
            const std::uint32_t newOffset = static_cast<std::uint32_t>(newNameArena.size());
            newNameArena.append(nameArena_.data() + document.nameOffset, document.nameLength);
            newDocuments.push_back(DocumentRecord{document.id, newOffset, document.nameLength, true});
            newIdToDoc.emplace(document.id, newDocId);
            AppendDocumentGrams(newDocId, std::string_view(newNameArena.data() + newOffset, document.nameLength), gramPairs);
        }

        // 用新的 gram 对重建 base，然后清空 delta 和墓碑。
        baseIndex_.Build(std::move(gramPairs.bigrams), std::move(gramPairs.unigrams), unigramEnabled_);
        documents_ = std::move(newDocuments);
        idToDoc_ = std::move(newIdToDoc);
        nameArena_ = std::move(newNameArena);
        deltaIndex_.Clear();

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        FUZZY_SEARCH_LOG_INFO("FuzzySearchSystem.Compact done: live={}, nameBytes={}, costMs={}", idToDoc_.size(), nameArena_.size(), static_cast<long long>(elapsed.count()));
    }

    // 清空全部数据，保留构造配置。
    void Clear() {
        FUZZY_SEARCH_LOG_INFO("FuzzySearchSystem.Clear: live={}, total={}", idToDoc_.size(), documents_.size());
        documents_.clear();
        idToDoc_.clear();
        nameArena_.clear();
        baseIndex_.Clear();
        deltaIndex_.Clear();
    }

    // 返回文档数、gram 数和估算内存等统计。
    Stats stats() const noexcept {
        Stats result;
        result.liveDocuments = idToDoc_.size();
        result.totalDocuments = documents_.size();
        result.tombstoneDocuments = result.totalDocuments > result.liveDocuments ? result.totalDocuments - result.liveDocuments : 0;
        result.nameBytes = nameArena_.size();
        result.baseGramEntries = baseIndex_.EntryCount();
        result.basePostingBytes = baseIndex_.PostingBytes();
        result.deltaGramEntries = deltaIndex_.EntryCount();
        result.deltaPostings = deltaIndex_.PostingCount();
        result.unigramEnabled = unigramEnabled_;
        const std::size_t documentBytes = sizeof(DocumentRecord) * documents_.capacity();
        const std::size_t idMapBytes = idToDoc_.size() * (sizeof(IdT) + sizeof(fuzzy_search_detail::DocId) + 32);
        result.estimatedBytes = documentBytes + nameArena_.capacity() + idMapBytes + baseIndex_.MemoryBytes() + deltaIndex_.MemoryBytes();
        return result;
    }

private:
    // ==================== 私有类型 ====================
    // 根据 BmpOnly 选择 32 位或 64 位 bigram key。
    using BigramKey = fuzzy_search_detail::BigramKeyT<BmpOnly>;

    // 文档记录：外部 id、名字在 arena 中的位置、是否存活。
    struct DocumentRecord {
        IdT id;
        std::uint32_t nameOffset;
        std::uint32_t nameLength;
        bool alive;
    };

    // Compact 构建 base 时使用的 (gram, 文档号) 临时集合。
    struct GramPairs {
        std::vector<std::pair<BigramKey, fuzzy_search_detail::DocId>> bigrams;
        std::vector<std::pair<std::uint32_t, fuzzy_search_detail::DocId>> unigrams;
    };

    // ==================== 日志通道 ====================
    // 迁移时只需替换 detail/fuzzy_search_log.h。
    FUZZY_SEARCH_LOG_CHANNEL(FuzzySearch)

private:
    // ==================== 私有静态辅助函数 ====================
    // 遍历 base 的压缩 posting；emit 返回 false 时停止。
    template <typename Emit>
    static bool EmitPackedPosting(std::span<const std::uint8_t> data, std::uint32_t count, Emit&& emit) {
        fuzzy_search_detail::PackedPostingCursor cursor(data, count);
        fuzzy_search_detail::DocId docId = 0;
        while (cursor.Next(&docId)) {
            if (!emit(docId)) {
                return false;
            }
        }
        return true;
    }

    // 遍历 delta posting；emit 返回 false 时停止。
    template <typename Emit>
    static bool EmitDeltaPosting(const std::vector<fuzzy_search_detail::DocId>* delta, Emit&& emit) {
        if (delta == nullptr) {
            return true;
        }
        for (fuzzy_search_detail::DocId docId : *delta) {
            if (!emit(docId)) {
                return false;
            }
        }
        return true;
    }

    // 把 base 的压缩 posting 解码到 out。
    static void DecodePackedPosting(std::span<const std::uint8_t> data, std::uint32_t count, std::vector<fuzzy_search_detail::DocId>& out) {
        fuzzy_search_detail::PackedPostingCursor cursor(data, count);
        fuzzy_search_detail::DocId docId = 0;
        while (cursor.Next(&docId)) {
            out.push_back(docId);
        }
    }

private:

    // ==================== 私有成员函数 ====================
    // 解码并校验名字，输出折叠后的码点和字节。
    Status DecodeName(std::string_view utf8Name, std::vector<char32_t>& codepoints, std::string& foldedName) const {
        const Status status = fuzzy_search_detail::DecodeUtf8(utf8Name, options_.maxStringLength, codepoints);
        if (status != Status::kOk) {
            return status;
        }
        if (codepoints.size() > static_cast<std::size_t>(options_.maxStringLength)) {
            return Status::kStringTooLong;
        }
        if (codepoints.size() < kMinStringLength) {
            return Status::kStringTooShort;
        }
        if constexpr (BmpOnly) {
            for (char32_t codepoint : codepoints) {
                if (codepoint > 0xFFFF) {
                    return Status::kUnsupportedCharacter;
                }
            }
        }
        if (options_.caseInsensitive) {
            fuzzy_search_detail::FoldAsciiCodepoints(codepoints);
        }
        foldedName.assign(utf8Name);
        if (options_.caseInsensitive) {
            fuzzy_search_detail::FoldAsciiBytes(foldedName);
        }
        return Status::kOk;
    }

    // 解码并校验查询，输出折叠后的码点和字节。
    Status DecodeQuery(std::string_view utf8Query, std::vector<char32_t>& codepoints, std::string& foldedQuery) const {
        const Status status = fuzzy_search_detail::DecodeUtf8(utf8Query, options_.maxStringLength, codepoints);
        if (status != Status::kOk) {
            return status;
        }
        if (codepoints.empty()) {
            return Status::kInvalidQuery;
        }
        if (codepoints.size() > static_cast<std::size_t>(options_.maxStringLength)) {
            return Status::kQueryTooLong;
        }
        if (codepoints.size() < static_cast<std::size_t>(options_.minSearchLength)) {
            return Status::kQueryTooShort;
        }
        if constexpr (BmpOnly) {
            for (char32_t codepoint : codepoints) {
                if (codepoint > 0xFFFF) {
                    return Status::kUnsupportedCharacter;
                }
            }
        }
        if (options_.caseInsensitive) {
            fuzzy_search_detail::FoldAsciiCodepoints(codepoints);
        }
        foldedQuery.assign(utf8Query);
        if (options_.caseInsensitive) {
            fuzzy_search_detail::FoldAsciiBytes(foldedQuery);
        }
        return Status::kOk;
    }

    // 判断文档号是否出现在除 skip 外的所有 posting 列表中。
    bool PresentInAllPostings(fuzzy_search_detail::DocId docId, const std::vector<std::vector<fuzzy_search_detail::DocId>>& lists, std::size_t skip) const {
        for (std::size_t i = 0; i < lists.size(); ++i) {
            if (i == skip) {
                continue;
            }
            if (!std::binary_search(lists[i].begin(), lists[i].end(), docId)) {
                return false;
            }
        }
        return true;
    }

    // 用名字原文做连续性校验，排除多个 gram 存在但不相邻的误报。
    bool NameContains(fuzzy_search_detail::DocId docId, std::string_view query) const {
        const DocumentRecord& document = documents_[docId];
        const std::string_view name(nameArena_.data() + document.nameOffset, document.nameLength);
        return name.find(query) != std::string_view::npos;
    }

    // 为 Compact 生成一个文档的 bigram/unigram 对。
    void AppendDocumentGrams(fuzzy_search_detail::DocId docId, std::string_view name, GramPairs& out) const {
        std::vector<char32_t> codepoints;
        fuzzy_search_detail::DecodeUtf8(name, options_.maxStringLength, codepoints);
        for (std::size_t i = 0; i + 1 < codepoints.size(); ++i) {
            out.bigrams.emplace_back(fuzzy_search_detail::MakeBigramKey<BmpOnly>(codepoints[i], codepoints[i + 1]), docId);
        }
        if (!unigramEnabled_) {
            return;
        }
        for (char32_t codepoint : codepoints) {
            out.unigrams.emplace_back(fuzzy_search_detail::MakeUnigramKey(codepoint), docId);
        }
    }

    // 依次遍历 base 和 delta 的 unigram posting。
    template <typename Emit>
    void ForEachUnigramPosting(std::uint32_t key, Emit&& emit) const {
        std::span<const std::uint8_t> data;
        std::uint32_t count = 0;
        if (baseIndex_.FindUnigram(key, &data, &count)) {
            if (!EmitPackedPosting(data, count, emit)) {
                return;
            }
        }
        EmitDeltaPosting(deltaIndex_.FindUnigram(key), emit);
    }

    // 依次遍历 base 和 delta 的 bigram posting。
    template <typename Emit>
    void ForEachBigramPosting(BigramKey key, Emit&& emit) const {
        std::span<const std::uint8_t> data;
        std::uint32_t count = 0;
        if (baseIndex_.FindBigram(key, &data, &count)) {
            if (!EmitPackedPosting(data, count, emit)) {
                return;
            }
        }
        EmitDeltaPosting(deltaIndex_.FindBigram(key), emit);
    }

    // 把 base + delta 的 bigram posting 合并成升序列表。
    void MaterializeBigram(BigramKey key, std::vector<fuzzy_search_detail::DocId>& out) const {
        out.clear();
        std::span<const std::uint8_t> data;
        std::uint32_t count = 0;
        if (baseIndex_.FindBigram(key, &data, &count)) {
            DecodePackedPosting(data, count, out);
        }
        if (const std::vector<fuzzy_search_detail::DocId>* delta = deltaIndex_.FindBigram(key)) {
            out.insert(out.end(), delta->begin(), delta->end());
        }
    }

private:
    // ==================== 成员变量 ====================
    // 构造配置。
    Options options_;
    // 是否构建 unigram 倒排，minSearchLength == 1 时为 true。
    bool unigramEnabled_ = false;
    // 全量文档表，含墓碑。
    std::vector<DocumentRecord> documents_;
    // 外部 id -> 内部文档号。
    std::unordered_map<IdT, fuzzy_search_detail::DocId> idToDoc_;
    // 折叠后的 UTF-8 名字。
    std::string nameArena_;
    // 紧凑只读索引。
    fuzzy_search_detail::BaseIndex<BmpOnly> baseIndex_;
    // 可变增量索引。
    fuzzy_search_detail::DeltaIndex<BmpOnly> deltaIndex_;
};

}  // namespace metaverse
