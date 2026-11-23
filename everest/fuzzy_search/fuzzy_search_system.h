/**
 * @file fuzzy_search_system.h
 * @brief 基于 UTF-8 的内存子串检索引擎。
 */

#ifndef FUZZY_SEARCH_SYSTEM_H_
#define FUZZY_SEARCH_SYSTEM_H_

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/define.h"
#include "fuzzy_search/detail/fuzzy_search_base_index.h"
#include "fuzzy_search/detail/fuzzy_search_delta_index.h"
#include "fuzzy_search/detail/fuzzy_search_gram_key.h"
#include "fuzzy_search/detail/fuzzy_search_lifecycle.h"
#include "fuzzy_search/detail/fuzzy_search_posting.h"
#include "fuzzy_search/detail/fuzzy_search_utf8.h"
#include "fuzzy_search/fuzzy_search_types.h"
#include "spdlog_wrapper/everest_spdlog.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief 基于 UTF-8 的内存子串检索引擎。
 *
 * 输入是 UTF-8 字符串和一个 64 位整数 id，输出是命中的 id；任意位置子串匹配，支持 1 到
 * max_string_length 个码点的查询，默认只折叠 ASCII 大小写。
 *
 * 实例通过 Create() 获得并由 std::unique_ptr 独占，不可拷贝、不可移动。
 *
 * 线程安全：库内无锁，全部接口必须在同一线程调用；Create() 之外没有可变的全局状态。
 *
 * 单实例：进程级守卫被所有模板实例化共享，已有存活实例时 Create() 返回 nullptr 与 kAlreadyCreated。
 *
 * 日志：通过 EVEREST_LOG_INFO/DEBUG/ERROR 输出，首次调用会惰性创建名为 "everest" 的 spdlog logger。
 *
 * @tparam IdT 外部 id 类型，必须是 64 位整数。
 * @tparam BmpOnly 为 true 时只接受 BMP 码点（<= 0xFFFF），并使用 32 位 bigram key；false 时支持完整 Unicode。
 */
template <typename IdT = std::uint64_t, bool BmpOnly = false>
class FuzzySearchSystem final {
public:
    // 构造配置类型。
    using Options = FuzzySearchOptions;
    // 状态码类型。
    using Status = FuzzySearchStatus;
    // 统计信息类型。
    using Stats = FuzzySearchStats;

    static_assert(std::is_integral_v<IdT> && sizeof(IdT) == 8, "FuzzySearchSystem requires a 64-bit integral id type");

    // 单次查询返回的硬上限。
    static constexpr std::size_t kMaxResults = 500;
    // 名字允许的最小码点数。
    static constexpr std::uint32_t kMinStringLength = 2;
    // 名字允许的最大码点数，同时决定热路径栈缓冲的尺寸。
    static constexpr std::uint32_t kMaxStringLength = 256;

    /**
     * @brief 唯一创建入口。
     *
     * @param options 构造配置，非法时创建失败。
     * @param status 可选输出，返回创建状态；可为 nullptr。
     * @return 创建成功返回实例；参数非法或进程内已有存活实例时返回 nullptr。
     *
     * @note 实例由返回的 std::unique_ptr 独占；析构时释放进程级单实例名额。
     */
    [[nodiscard]] static std::unique_ptr<FuzzySearchSystem> Create(const Options& options, Status* status = nullptr) {
        const Status result = ValidateOptions(options);
        if (status != nullptr) {
            *status = result;
        }

        if (result != Status::kOk) {
            EVEREST_LOG_ERROR("FuzzySearchSystem.Create failed: invalid options maxStringLength={}, minSearchLength={}", options.max_string_length, options.min_search_length);
            return nullptr;
        }

        if (!FuzzySearchCreationGuard::TryAcquire()) {
            if (status != nullptr) {
                *status = Status::kAlreadyCreated;
            }
            EVEREST_LOG_ERROR("FuzzySearchSystem.Create failed: an instance already exists in this process");
            return nullptr;
        }

        return std::unique_ptr<FuzzySearchSystem>(new FuzzySearchSystem(options));
    }

    /**
     * @brief 只校验构造参数，不分配内存、不抛异常。
     * @param options 待校验的构造配置。
     * @return 合法返回 kOk，否则返回 kInvalidOptions。
     */
    [[nodiscard]] static Status ValidateOptions(const Options& options) noexcept {
        if (options.max_string_length < kMinStringLength || options.max_string_length > kMaxStringLength) {
            return Status::kInvalidOptions;
        }
        if (options.min_search_length == 0 || options.min_search_length > options.max_string_length) {
            return Status::kInvalidOptions;
        }
        return Status::kOk;
    }

    /**
     * @brief 禁止拷贝构造。
     */
    FuzzySearchSystem(const FuzzySearchSystem&) = delete;

    /**
     * @brief 禁止拷贝赋值。
     */
    FuzzySearchSystem& operator=(const FuzzySearchSystem&) = delete;

    /**
     * @brief 禁止移动构造。
     */
    FuzzySearchSystem(FuzzySearchSystem&&) = delete;

    /**
     * @brief 禁止移动赋值。
     */
    FuzzySearchSystem& operator=(FuzzySearchSystem&&) = delete;

    /**
     * @brief 析构实例并释放进程级单实例名额。
     */
    ~FuzzySearchSystem() {
        FuzzySearchCreationGuard::Release();
    }

    /**
     * @brief 插入或覆盖一条文档。
     *
     * @param id 外部 id；重复 Add 同一个 id 会以新名字覆盖旧名字。
     * @param utf8_name UTF-8 名字，长度为 [kMinStringLength, max_string_length] 个码点。
     * @return 成功返回 kOk，否则返回参数或容量相关的状态码。
     *
     * @note 失败时不修改任何已建立的索引，调用方可以安全地重试。
     */
    [[nodiscard]] Status Add(IdT id, std::string_view utf8_name) {
        TextScratch scratch;
        const Status decode_status = DecodeText(utf8_name, DecodeTarget::kName, scratch);
        if (decode_status != Status::kOk) {
            return decode_status;
        }

        if (documents_.size() >= std::numeric_limits<FuzzySearchDocId>::max()) {
            return Status::kTooManyDocuments;
        }

        if (name_arena_.size() + scratch.byte_count > std::numeric_limits<std::uint32_t>::max()) {
            return Status::kNameArenaFull;
        }

        // 同名 id 覆盖：旧文档保留在 posting 中但标记为墓碑。
        const auto existing = id_to_doc_.find(id);
        if (existing != id_to_doc_.end()) {
            documents_[existing->second].alive = false;
            id_to_doc_.erase(existing);
        }

        const std::uint32_t name_offset = static_cast<std::uint32_t>(name_arena_.size());
        name_arena_.append(scratch.folded_bytes.data(), scratch.byte_count);
        const FuzzySearchDocId doc_id = static_cast<FuzzySearchDocId>(documents_.size());
        documents_.push_back(DocumentRecord{id, name_offset, static_cast<std::uint32_t>(scratch.byte_count), true});
        id_to_doc_.emplace(id, doc_id);

        // 新文档只写入 delta；Compact() 时再合并进 base。
        const std::span<const char32_t> codepoints(scratch.codepoints.data(), scratch.codepoint_count);
        if (unigram_enabled_) {
            for (char32_t codepoint : codepoints) {
                delta_index_.AddUnigram(FuzzySearchGramKey::MakeUnigram(codepoint), doc_id);
            }
        }

        for (std::size_t i = 0; i + 1 < codepoints.size(); ++i) {
            const BigramKey key = FuzzySearchGramKey::MakeBigram<BmpOnly>(codepoints[i], codepoints[i + 1]);
            delta_index_.AddBigram(key, doc_id);
        }

        return Status::kOk;
    }

    /**
     * @brief 幂等删除一条文档。
     *
     * @param id 外部 id。
     * @return 总是返回 kOk；id 不存在时不做任何修改。
     */
    [[nodiscard]] Status Remove(IdT id) {
        const auto existing = id_to_doc_.find(id);
        if (existing == id_to_doc_.end()) {
            return Status::kOk;
        }
        documents_[existing->second].alive = false;
        id_to_doc_.erase(existing);
        return Status::kOk;
    }

    /**
     * @brief 判断 id 当前是否存在。
     * @param id 外部 id。
     * @return 存在且存活时返回 true。
     */
    [[nodiscard]] bool Contains(IdT id) const noexcept {
        return id_to_doc_.find(id) != id_to_doc_.end();
    }

    /**
     * @brief 查询并逐条回调结果。
     *
     * @param utf8_query UTF-8 查询串，长度为 [min_search_length, max_string_length] 个码点。
     * @param max_results 期望的最大结果数，取值 [1, kMaxResults]。
     * @param visitor 结果回调，返回 false 时立即停止；同步调用，不做生命周期转移。
     * @return 成功返回 kOk，否则返回参数相关的状态码。
     *
     * @note 结果顺序不承诺；跳过墓碑后才会调用 visitor。
     */
    template <typename Visitor>
        requires std::invocable<Visitor&, IdT>
    [[nodiscard]] Status Search(std::string_view utf8_query, std::size_t max_results, Visitor&& visitor) const {
        if (max_results == 0 || max_results > kMaxResults) {
            return Status::kInvalidMaxResults;
        }

        TextScratch scratch;
        const Status decode_status = DecodeText(utf8_query, DecodeTarget::kQuery, scratch);
        if (decode_status != Status::kOk) {
            return decode_status;
        }

        std::size_t matched = 0;
        auto emit = [this, &visitor, &matched, max_results](FuzzySearchDocId doc_id) -> bool {
            const DocumentRecord& document = documents_[doc_id];
            if (!document.alive) {
                return true;
            }
            if (!visitor(document.id)) {
                return false;
            }
            ++matched;
            return matched < max_results;
        };

        const std::span<const char32_t> codepoints(scratch.codepoints.data(), scratch.codepoint_count);

        // 1 字查询走 unigram；只有 min_search_length == 1 时才构建 unigram。
        if (codepoints.size() == 1) {
            if (!unigram_enabled_) {
                return Status::kQueryTooShort;
            }
            const std::uint32_t key = FuzzySearchGramKey::MakeUnigram(codepoints[0]);
            EmitCombinedPosting(base_index_.FindUnigram(key), delta_index_.FindUnigram(key), emit);
            return Status::kOk;
        }

        // 2 字查询走单个 bigram，posting 就是精确结果，无需原文校验。
        if (codepoints.size() == 2) {
            const BigramKey key = FuzzySearchGramKey::MakeBigram<BmpOnly>(codepoints[0], codepoints[1]);
            EmitCombinedPosting(base_index_.FindBigram(key), delta_index_.FindBigram(key), emit);
            return Status::kOk;
        }

        // 3 字以上：取相邻 bigram 列表求交，再用名字原文校验连续性。
        SearchMultiCodepoint(codepoints, std::string_view(scratch.folded_bytes.data(), scratch.byte_count), emit);
        return Status::kOk;
    }

    /**
     * @brief 查询并把结果写入 vector。
     *
     * @param utf8_query UTF-8 查询串，长度为 [min_search_length, max_string_length] 个码点。
     * @param max_results 期望的最大结果数，取值 [1, kMaxResults]。
     * @param out 结果容器，由调用方持有；参数校验通过后先清空再写入。
     * @return 成功返回 kOk，否则返回参数相关的状态码。
     *
     * @note 结果顺序不承诺；参数非法时 out 的内容保持不变。
     */
    [[nodiscard]] Status Search(std::string_view utf8_query, std::size_t max_results, std::vector<IdT>& out) const {
        if (max_results == 0 || max_results > kMaxResults) {
            return Status::kInvalidMaxResults;
        }

        out.clear();
        out.reserve(max_results);
        return Search(utf8_query, max_results, [&out, max_results](IdT id) {
            out.push_back(id);
            return out.size() < max_results;
        });
    }

    /**
     * @brief 容量提示，只影响分配次数，不影响索引正确性。
     * @param document_count 预估的文档数。
     * @param name_bytes 预估的名字字节数。
     */
    void Reserve(std::size_t document_count, std::size_t name_bytes = 0) {
        documents_.reserve(document_count);
        id_to_doc_.reserve(document_count);
        name_arena_.reserve(name_bytes);
        delta_index_.Reserve(document_count);
    }

    /**
     * @brief 合并 base 与 delta，清理墓碑并重建紧凑 base。
     *
     * @return 成功或无需重建时返回 kOk；名字 arena、文档号超限或名字解码失败时返回对应状态码。
     *
     * @note 同步执行，建议在低峰调用。失败时 documents_、id_to_doc_、name_arena_、base_index_、
     *       delta_index_ 均保持调用前的状态，调用方可以重试或回退。
     */
    [[nodiscard]] Status Compact() {
        const std::size_t live_documents = id_to_doc_.size();
        const std::size_t total_documents = documents_.size();

        // 没有墓碑、delta 也为空时没有可合并的内容，直接跳过。
        if (total_documents == live_documents && delta_index_.EntryCount() == 0) {
            EVEREST_LOG_DEBUG("FuzzySearchSystem.Compact skipped: no tombstones and empty delta");
            return Status::kOk;
        }

        const auto start = std::chrono::steady_clock::now();
        EVEREST_LOG_INFO("FuzzySearchSystem.Compact start: live={}, tombstones={}, nameBytes={}", live_documents, total_documents - live_documents, name_arena_.size());

        if (live_documents > std::numeric_limits<FuzzySearchDocId>::max()) {
            EVEREST_LOG_ERROR("FuzzySearchSystem.Compact failed: too many live documents={}", live_documents);
            return Status::kTooManyDocuments;
        }

        // 只保留存活文档，并重新分配连续的内部文档号。
        std::vector<DocumentRecord> new_documents;
        std::unordered_map<IdT, FuzzySearchDocId> new_id_to_doc;
        std::string new_name_arena;
        new_documents.reserve(live_documents);
        new_id_to_doc.reserve(live_documents);
        new_name_arena.reserve(name_arena_.size());

        // 预留量按名字字节中实际出现的码点数估算：UTF-8 续字节（10xxxxxx）不是码点起点。
        std::size_t codepoint_total = 0;
        for (char byte : name_arena_) {
            if ((static_cast<unsigned char>(byte) & 0xC0) != 0x80) {
                ++codepoint_total;
            }
        }
        GramPairs gram_pairs;
        gram_pairs.bigrams.reserve(codepoint_total);
        if (unigram_enabled_) {
            gram_pairs.unigrams.reserve(codepoint_total);
        }

        for (std::size_t old_doc_id = 0; old_doc_id < documents_.size(); ++old_doc_id) {
            const DocumentRecord& document = documents_[old_doc_id];
            if (!document.alive) {
                continue;
            }

            if (new_name_arena.size() + document.name_length > std::numeric_limits<std::uint32_t>::max()) {
                EVEREST_LOG_ERROR("FuzzySearchSystem.Compact failed: name arena too large");
                return Status::kNameArenaFull;
            }

            const FuzzySearchDocId new_doc_id = static_cast<FuzzySearchDocId>(new_documents.size());
            const std::uint32_t new_offset = static_cast<std::uint32_t>(new_name_arena.size());
            new_name_arena.append(name_arena_.data() + document.name_offset, document.name_length);
            new_documents.push_back(DocumentRecord{document.id, new_offset, document.name_length, true});
            new_id_to_doc.emplace(document.id, new_doc_id);

            const std::string_view name(new_name_arena.data() + new_offset, document.name_length);
            const Status gram_status = AppendDocumentGrams(new_doc_id, name, gram_pairs);
            if (gram_status != Status::kOk) {
                EVEREST_LOG_ERROR("FuzzySearchSystem.Compact failed: decode name failed docId={} status={}", new_doc_id, static_cast<int>(gram_status));
                return gram_status;
            }
        }

        // 新的 gram 表构建完成后才整体替换，保证失败路径不改动现有索引。
        base_index_.Build(std::move(gram_pairs.bigrams), std::move(gram_pairs.unigrams), unigram_enabled_);
        documents_ = std::move(new_documents);
        id_to_doc_ = std::move(new_id_to_doc);
        name_arena_ = std::move(new_name_arena);
        delta_index_.Clear();

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        EVEREST_LOG_INFO("FuzzySearchSystem.Compact done: live={}, nameBytes={}, costMs={}", id_to_doc_.size(), name_arena_.size(), static_cast<long long>(elapsed.count()));
        return Status::kOk;
    }

    /**
     * @brief 清空全部数据，保留构造配置。
     */
    void Clear() {
        EVEREST_LOG_INFO("FuzzySearchSystem.Clear: live={}, total={}", id_to_doc_.size(), documents_.size());
        documents_.clear();
        id_to_doc_.clear();
        name_arena_.clear();
        base_index_.Clear();
        delta_index_.Clear();
    }

    /**
     * @brief 返回文档数、gram 数和内存占用的近似统计。
     * @return 统计快照；内存字段为按容量估算的近似值。
     */
    [[nodiscard]] Stats stats() const noexcept {
        Stats result;
        result.live_documents = id_to_doc_.size();
        result.total_documents = documents_.size();
        result.tombstone_documents = result.total_documents > result.live_documents ? result.total_documents - result.live_documents : 0;
        result.name_bytes = name_arena_.size();
        result.base_gram_entries = base_index_.EntryCount();
        result.base_posting_bytes = base_index_.PostingBytes();
        result.delta_gram_entries = delta_index_.EntryCount();
        result.delta_postings = delta_index_.PostingCount();
        result.unigram_enabled = unigram_enabled_;
        // 32 是哈希节点与桶开销的粗略近似值，只用于监控估算。
        constexpr std::size_t kNodeOverhead = 32;
        const std::size_t document_bytes = sizeof(DocumentRecord) * documents_.capacity();
        const std::size_t id_map_bytes = id_to_doc_.size() * (sizeof(IdT) + sizeof(FuzzySearchDocId) + kNodeOverhead);
        result.estimated_bytes = document_bytes + name_arena_.capacity() + id_map_bytes + base_index_.MemoryBytes() + delta_index_.MemoryBytes();
        return result;
    }

private:
    // bigram key 类型。
    using BigramKey = FuzzySearchBigramKeyT<BmpOnly>;

    /**
     * @brief 区分名字与查询，决定长度校验返回的错误码。
     */
    enum class DecodeTarget {
        // 名字：过短返回 kStringTooShort，过长返回 kStringTooLong。
        kName,
        // 查询：为空返回 kInvalidQuery，过短返回 kQueryTooShort，过长返回 kQueryTooLong。
        kQuery,
    };

    /**
     * @brief Add / Search 热路径上的固定缓冲，避免每次调用做堆分配。
     */
    struct TextScratch {
        // 码点缓冲，最多保存 kMaxStringLength + 1 个码点。
        std::array<char32_t, kMaxStringLength + 1> codepoints{};
        // 折叠后的 UTF-8 字节缓冲，最长 4 * kMaxStringLength 字节。
        std::array<char, 4 * kMaxStringLength + 1> folded_bytes{};
        // 解码得到的码点数；超过 max_string_length 时最多为 max_string_length + 1。
        std::size_t codepoint_count = 0;
        // 折叠后的字节数。
        std::size_t byte_count = 0;
    };

    /**
     * @brief 文档记录：外部 id、名字在 arena 中的位置、是否存活。
     */
    struct DocumentRecord {
        // 外部 id。
        IdT id = 0;
        // 名字在 name_arena_ 中的字节偏移。
        std::uint32_t name_offset = 0;
        // 名字的字节长度。
        std::uint32_t name_length = 0;
        // false 表示墓碑。
        bool alive = false;
    };

    /**
     * @brief Compact 构建 base 时使用的 (gram, 文档号) 临时集合。
     */
    struct GramPairs {
        // bigram 对。
        std::vector<FuzzySearchGramPair<BigramKey>> bigrams;
        // unigram 对。
        std::vector<FuzzySearchGramPair<std::uint32_t>> unigrams;
    };

    /**
     * @brief 一个 posting 列表在单块缓冲中的位置。
     */
    struct PostingRange {
        // 在 PostingBlock::postings 中的起始下标。
        std::size_t offset = 0;
        // 文档号个数。
        std::size_t size = 0;
    };

    /**
     * @brief 多码点查询时 Q-1 个 posting 列表的单块缓冲。
     */
    struct PostingBlock {
        // 依次写入的各列表文档号。
        std::vector<FuzzySearchDocId> postings;
        // 每个列表在 postings 中的区间，最多 max_string_length - 1 个。
        std::array<PostingRange, kMaxStringLength - 1> ranges{};
        // 实际使用的区间数。
        std::size_t range_count = 0;
    };

    /**
     * @brief 私有构造，只能通过 Create() 创建。
     * @param options 已通过 ValidateOptions 的构造配置。
     */
    explicit FuzzySearchSystem(const Options& options) : options_(options), unigram_enabled_(options.min_search_length == 1) {}

    /**
     * @brief 解码名字或查询，完成长度、字符集校验与大小写折叠。
     *
     * @param input UTF-8 输入。
     * @param target 决定长度错误码取名字还是查询的语义。
     * @param scratch 调用方持有的固定缓冲，输出码点、折叠字节与两者长度。
     * @return 成功返回 kOk，否则返回对应错误码；失败时 scratch 的内容不可用。
     */
    [[nodiscard]] Status DecodeText(std::string_view input, DecodeTarget target, TextScratch& scratch) const {
        const std::size_t max_codepoints = options_.max_string_length;

        // 每个码点最多 4 字节，超出字节预算直接判超长，不扫描内容。
        if (input.size() > max_codepoints * 4) {
            return TooLongStatus(target);
        }

        const FuzzySearchDecodeResult decoded = FuzzySearchUtf8::Decode(input, options_.max_string_length, std::span<char32_t>(scratch.codepoints));
        if (decoded.status != Status::kOk) {
            return decoded.status;
        }

        scratch.codepoint_count = decoded.codepoint_count;
        if (scratch.codepoint_count > max_codepoints) {
            return TooLongStatus(target);
        }

        if (target == DecodeTarget::kName) {
            if (scratch.codepoint_count < kMinStringLength) {
                return Status::kStringTooShort;
            }
        } else {
            if (scratch.codepoint_count == 0) {
                return Status::kInvalidQuery;
            }
            if (scratch.codepoint_count < options_.min_search_length) {
                return Status::kQueryTooShort;
            }
        }

        const std::span<char32_t> codepoints(scratch.codepoints.data(), scratch.codepoint_count);
        if constexpr (BmpOnly) {
            for (char32_t codepoint : codepoints) {
                if (codepoint > 0xFFFF) {
                    return Status::kUnsupportedCharacter;
                }
            }
        }

        if (options_.case_insensitive) {
            FuzzySearchUtf8::FoldCodepoints(codepoints);
        }

        scratch.byte_count = input.size();
        std::copy(input.begin(), input.end(), scratch.folded_bytes.begin());
        if (options_.case_insensitive) {
            FuzzySearchUtf8::FoldBytes(std::span<char>(scratch.folded_bytes.data(), scratch.byte_count));
        }
        return Status::kOk;
    }

    /**
     * @brief 把「长度超限」映射为名字或查询对应的错误码。
     * @param target 解码目标。
     * @return 名字返回 kStringTooLong，查询返回 kQueryTooLong。
     */
    [[nodiscard]] static Status TooLongStatus(DecodeTarget target) noexcept {
        return target == DecodeTarget::kName ? Status::kStringTooLong : Status::kQueryTooLong;
    }

    /**
     * @brief 依次遍历 base 与 delta 中同一个 gram 的 posting。
     *
     * @param base_posting base 中的压缩 posting，可为空。
     * @param delta_posting delta 中的文档号视图，可为空。
     * @param emit 文档号回调，返回 false 时立即停止。
     * @return 全部遍历完返回 true；emit 要求停止时返回 false。
     */
    template <typename Emit>
    static bool EmitCombinedPosting(const std::optional<FuzzySearchPostingView>& base_posting, std::span<const FuzzySearchDocId> delta_posting, Emit&& emit) {
        if (base_posting.has_value()) {
            FuzzySearchPackedPostingCursor cursor(base_posting->data, base_posting->count);
            FuzzySearchDocId doc_id = 0;
            while (cursor.Next(&doc_id)) {
                if (!emit(doc_id)) {
                    return false;
                }
            }
        }

        for (FuzzySearchDocId doc_id : delta_posting) {
            if (!emit(doc_id)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 统计 base 与 delta 中同一个 gram 的 posting 文档号总数。
     * @param base_posting base 中的压缩 posting，可为空。
     * @param delta_posting delta 中的文档号视图，可为空。
     * @return 文档号总数。
     */
    [[nodiscard]] static std::size_t CountPosting(const std::optional<FuzzySearchPostingView>& base_posting, std::span<const FuzzySearchDocId> delta_posting) noexcept {
        const std::size_t base_count = base_posting.has_value() ? base_posting->count : 0;
        return base_count + delta_posting.size();
    }

    /**
     * @brief 判断文档号是否出现在除 skip 外的所有 posting 列表中。
     * @param block 单块 posting 缓冲。
     * @param doc_id 候选文档号。
     * @param skip 已作为遍历来源的列表下标，跳过它的二分查找。
     * @return 出现在其余全部列表中返回 true。
     */
    [[nodiscard]] bool PresentInAllPostings(const PostingBlock& block, FuzzySearchDocId doc_id, std::size_t skip) const {
        for (std::size_t i = 0; i < block.range_count; ++i) {
            if (i == skip) {
                continue;
            }
            const PostingRange& range = block.ranges[i];
            const FuzzySearchDocId* first = block.postings.data() + static_cast<std::ptrdiff_t>(range.offset);
            const FuzzySearchDocId* last = first + static_cast<std::ptrdiff_t>(range.size);
            if (!std::binary_search(first, last, doc_id)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 用名字原文做连续性校验，排除多个 gram 存在但不相邻的误报。
     * @param doc_id 候选文档号。
     * @param folded_query 折叠后的查询字节。
     * @return 名字包含查询子串时返回 true。
     */
    [[nodiscard]] bool NameContains(FuzzySearchDocId doc_id, std::string_view folded_query) const {
        const DocumentRecord& document = documents_[doc_id];
        const std::string_view name(name_arena_.data() + document.name_offset, document.name_length);
        return name.find(folded_query) != std::string_view::npos;
    }

    /**
     * @brief 为 Compact 生成单个文档的 bigram / unigram 对。
     *
     * @param doc_id 新索引中的内部文档号。
     * @param name arena 中的折叠后名字。
     * @param out 输出的 gram 对集合，由调用方持有。
     * @return 名字解码成功返回 kOk，否则返回解码错误码。
     */
    [[nodiscard]] Status AppendDocumentGrams(FuzzySearchDocId doc_id, std::string_view name, GramPairs& out) const {
        TextScratch scratch;
        const Status status = DecodeText(name, DecodeTarget::kName, scratch);
        if (status != Status::kOk) {
            return status;
        }

        const std::span<const char32_t> codepoints(scratch.codepoints.data(), scratch.codepoint_count);
        for (std::size_t i = 0; i + 1 < codepoints.size(); ++i) {
            out.bigrams.push_back(FuzzySearchGramPair<BigramKey>{FuzzySearchGramKey::MakeBigram<BmpOnly>(codepoints[i], codepoints[i + 1]), doc_id});
        }

        if (!unigram_enabled_) {
            return Status::kOk;
        }
        for (char32_t codepoint : codepoints) {
            out.unigrams.push_back(FuzzySearchGramPair<std::uint32_t>{FuzzySearchGramKey::MakeUnigram(codepoint), doc_id});
        }
        return Status::kOk;
    }

    /**
     * @brief 3 个码点以上查询的交集计算与原文校验。
     *
     * @param codepoints 折叠后的查询码点，至少 3 个。
     * @param folded_query 折叠后的查询字节，用于原文连续性校验。
     * @param emit 文档号回调，返回 false 时立即停止。
     */
    template <typename Emit>
    void SearchMultiCodepoint(std::span<const char32_t> codepoints, std::string_view folded_query, Emit&& emit) const {
        const std::size_t gram_count = codepoints.size() - 1;

        // 先求各列表长度之和，用一块连续缓冲承载 Q-1 个列表，避免每次查询多次堆分配。
        std::size_t posting_total = 0;
        for (std::size_t i = 0; i < gram_count; ++i) {
            const BigramKey key = FuzzySearchGramKey::MakeBigram<BmpOnly>(codepoints[i], codepoints[i + 1]);
            const std::size_t count = CountPosting(base_index_.FindBigram(key), delta_index_.FindBigram(key));
            const std::size_t remaining = std::numeric_limits<std::size_t>::max() - posting_total;
            posting_total += count > remaining ? remaining : count;
        }

        PostingBlock block;
        block.postings.reserve(posting_total);
        for (std::size_t i = 0; i < gram_count; ++i) {
            const BigramKey key = FuzzySearchGramKey::MakeBigram<BmpOnly>(codepoints[i], codepoints[i + 1]);
            block.ranges[i].offset = block.postings.size();
            EmitCombinedPosting(base_index_.FindBigram(key), delta_index_.FindBigram(key), [&block](FuzzySearchDocId doc_id) {
                block.postings.push_back(doc_id);
                return true;
            });
            block.ranges[i].size = block.postings.size() - block.ranges[i].offset;
            if (block.ranges[i].size == 0) {
                // 任一 gram 没有 posting，交集必然为空。
                return;
            }
        }
        block.range_count = gram_count;

        // 从最短的 posting 列表开始遍历，减少候选量。
        std::size_t shortest = 0;
        for (std::size_t i = 1; i < gram_count; ++i) {
            if (block.ranges[i].size < block.ranges[shortest].size) {
                shortest = i;
            }
        }

        const std::size_t begin = block.ranges[shortest].offset;
        const std::size_t end = begin + block.ranges[shortest].size;
        for (std::size_t index = begin; index < end; ++index) {
            const FuzzySearchDocId doc_id = block.postings[index];
            if (!documents_[doc_id].alive) {
                continue;
            }
            if (!PresentInAllPostings(block, doc_id, shortest)) {
                continue;
            }
            if (!NameContains(doc_id, folded_query)) {
                continue;
            }
            if (!emit(doc_id)) {
                break;
            }
        }
    }

private:
    // 构造配置。
    Options options_;
    // 是否构建 unigram 倒排，min_search_length == 1 时为 true。
    bool unigram_enabled_ = false;
    // 全量文档表，含墓碑。
    std::vector<DocumentRecord> documents_;
    // 外部 id -> 内部文档号。
    std::unordered_map<IdT, FuzzySearchDocId> id_to_doc_;
    // 折叠后的 UTF-8 名字。
    std::string name_arena_;
    // 紧凑只读索引。
    FuzzySearchBaseIndex<BmpOnly> base_index_;
    // 可变增量索引。
    FuzzySearchDeltaIndex<BmpOnly> delta_index_;
};

NAMESPACE_EVEREST_END

#endif
