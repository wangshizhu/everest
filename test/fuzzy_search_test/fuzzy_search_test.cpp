/**
 * @file fuzzy_search_test.cpp
 * @brief fuzzy_search 组件的 GoogleTest 用例：覆盖公共 API 行为与 detail 层工具。
 */

#include "fuzzy_search/fuzzy_search_system.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "fuzzy_search/detail/fuzzy_search_gram_key.h"
#include "fuzzy_search/detail/fuzzy_search_posting.h"
#include "fuzzy_search/detail/fuzzy_search_utf8.h"

// 只接受 BMP 码点的测试实例类型。
using BmpSystem = everest::FuzzySearchSystem<std::uint64_t, true>;
// 支持完整 Unicode 的测试实例类型。
using FullSystem = everest::FuzzySearchSystem<std::uint64_t, false>;

/**
 * @brief 构造测试用的配置。
 * @param max_length 最大字符串长度。
 * @param min_search 最小查询长度。
 * @param case_insensitive 是否折叠 ASCII 大小写。
 * @return 填充后的配置。
 */
static BmpSystem::Options MakeOptions(std::uint32_t max_length, std::uint32_t min_search, bool case_insensitive) {
    BmpSystem::Options options;
    options.max_string_length = max_length;
    options.min_search_length = min_search;
    options.case_insensitive = case_insensitive;
    return options;
}

/**
 * @brief 查询并返回命中的 id 列表。
 * @param system 待查询的实例。
 * @param query UTF-8 查询串。
 * @param max_results 最大结果数。
 * @return 命中的 id 列表；查询状态不是 kOk 时为空。
 */
static std::vector<std::uint64_t> SearchIds(const BmpSystem& system, std::string_view query, std::size_t max_results = BmpSystem::kMaxResults) {
    std::vector<std::uint64_t> ids;
    const BmpSystem::Status status = system.Search(query, max_results, ids);
    EXPECT_EQ(status, BmpSystem::Status::kOk);
    return ids;
}

/**
 * @brief 判断 id 是否在结果列表中。
 * @param ids 结果列表。
 * @param id 待查找的 id。
 * @return 存在返回 true。
 */
static bool ContainsId(const std::vector<std::uint64_t>& ids, std::uint64_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

TEST(FuzzySearchSystemTest, CreateAcceptsValidOptions) {
    BmpSystem::Status status = BmpSystem::Status::kOk;
    auto system = BmpSystem::Create(MakeOptions(8, 1, true), &status);

    EXPECT_NE(system, nullptr);
    EXPECT_EQ(status, BmpSystem::Status::kOk);
}

TEST(FuzzySearchSystemTest, CreateRejectsInvalidOptions) {
    BmpSystem::Status status = BmpSystem::Status::kOk;

    const BmpSystem::Options invalid_options[] = {
        MakeOptions(1, 1, true),
        MakeOptions(8, 0, true),
        MakeOptions(8, 9, true),
        MakeOptions(BmpSystem::kMaxStringLength + 1, 1, true),
    };
    for (const BmpSystem::Options& options : invalid_options) {
        auto system = BmpSystem::Create(options, &status);
        EXPECT_EQ(system, nullptr);
        EXPECT_EQ(status, BmpSystem::Status::kInvalidOptions);
    }
}

TEST(FuzzySearchSystemTest, CreateGuardsSingleInstance) {
    BmpSystem::Status status = BmpSystem::Status::kOk;
    auto first = BmpSystem::Create(MakeOptions(8, 1, true), &status);
    ASSERT_NE(first, nullptr);
    ASSERT_EQ(status, BmpSystem::Status::kOk);

    auto second = BmpSystem::Create(MakeOptions(8, 1, true), &status);

    EXPECT_EQ(second, nullptr);
    EXPECT_EQ(status, BmpSystem::Status::kAlreadyCreated);

    first.reset();
    auto third = BmpSystem::Create(MakeOptions(8, 1, true), &status);
    EXPECT_NE(third, nullptr);
    EXPECT_EQ(status, BmpSystem::Status::kOk);
}

TEST(FuzzySearchSystemTest, AddAndSearchSubstring) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1001, "大宇宙"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(1002, "宇宙会"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(1003, "Hello"), BmpSystem::Status::kOk);

    EXPECT_TRUE(system->Contains(1001));

    const std::vector<std::uint64_t> single = SearchIds(*system, "宇");
    EXPECT_EQ(single.size(), std::size_t{2});
    EXPECT_TRUE(ContainsId(single, 1001));
    EXPECT_TRUE(ContainsId(single, 1002));

    const std::vector<std::uint64_t> bigram = SearchIds(*system, "宇宙");
    EXPECT_EQ(bigram.size(), std::size_t{2});
    EXPECT_TRUE(ContainsId(bigram, 1001));
    EXPECT_TRUE(ContainsId(bigram, 1002));

    const std::vector<std::uint64_t> prefix = SearchIds(*system, "大宇");
    EXPECT_EQ(prefix.size(), std::size_t{1});
    EXPECT_TRUE(ContainsId(prefix, 1001));

    const std::vector<std::uint64_t> folded = SearchIds(*system, "hello");
    EXPECT_EQ(folded.size(), std::size_t{1});
    EXPECT_TRUE(ContainsId(folded, 1003));

    EXPECT_TRUE(SearchIds(*system, "不存在").empty());
}

TEST(FuzzySearchSystemTest, SingleCodepointRequiresUnigram) {
    auto system = BmpSystem::Create(MakeOptions(8, 2, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "宇宙"), BmpSystem::Status::kOk);

    EXPECT_FALSE(system->stats().unigram_enabled);

    std::vector<std::uint64_t> ids;
    EXPECT_EQ(system->Search("宇", 10, ids), BmpSystem::Status::kQueryTooShort);
    EXPECT_EQ(system->Search("宇宙", 10, ids), BmpSystem::Status::kOk);
    EXPECT_EQ(ids.size(), std::size_t{1});
    EXPECT_EQ(ids[0], std::uint64_t{1});
}

TEST(FuzzySearchSystemTest, CaseInsensitiveFoldsAscii) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "Hello"), BmpSystem::Status::kOk);

    EXPECT_EQ(SearchIds(*system, "hello").size(), std::size_t{1});
    EXPECT_EQ(SearchIds(*system, "HELLO").size(), std::size_t{1});
}

TEST(FuzzySearchSystemTest, CaseSensitiveKeepsCase) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, false));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "Hello"), BmpSystem::Status::kOk);

    EXPECT_TRUE(SearchIds(*system, "hello").empty());
    EXPECT_EQ(SearchIds(*system, "Hello").size(), std::size_t{1});
}

TEST(FuzzySearchSystemTest, RejectsNonAdjacentBigrams) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "abXbc"), BmpSystem::Status::kOk);

    // "ab" 与 "bc" 都存在，但 "abc" 在名字中不相邻。
    EXPECT_TRUE(SearchIds(*system, "abc").empty());
    EXPECT_EQ(SearchIds(*system, "ab").size(), std::size_t{1});
    EXPECT_EQ(SearchIds(*system, "bc").size(), std::size_t{1});
}

TEST(FuzzySearchSystemTest, MaxResultsAndVisitorEarlyStop) {
    auto system = BmpSystem::Create(MakeOptions(16, 1, true));
    ASSERT_NE(system, nullptr);
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(system->Add(static_cast<std::uint64_t>(100 + i), "测试" + std::to_string(i)), BmpSystem::Status::kOk);
    }

    EXPECT_EQ(SearchIds(*system, "测试", 3).size(), std::size_t{3});

    std::size_t visited = 0;
    EXPECT_EQ(system->Search("测试", 100,
                             [&visited](std::uint64_t) {
                                 ++visited;
                                 return visited < 2;
                             }),
              BmpSystem::Status::kOk);
    EXPECT_EQ(visited, std::size_t{2});

    visited = 0;
    EXPECT_EQ(system->Search("测试", 5,
                             [&visited](std::uint64_t) {
                                 ++visited;
                                 return true;
                             }),
              BmpSystem::Status::kOk);
    EXPECT_EQ(visited, std::size_t{5});
}

TEST(FuzzySearchSystemTest, RejectsInvalidMaxResults) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "宇宙"), BmpSystem::Status::kOk);

    std::vector<std::uint64_t> ids;
    ASSERT_EQ(system->Search("宇宙", 10, ids), BmpSystem::Status::kOk);
    ASSERT_EQ(ids.size(), std::size_t{1});

    EXPECT_EQ(system->Search("宇宙", 0, ids), BmpSystem::Status::kInvalidMaxResults);
    EXPECT_EQ(system->Search("宇宙", BmpSystem::kMaxResults + 1, ids), BmpSystem::Status::kInvalidMaxResults);
    EXPECT_EQ(ids.size(), std::size_t{1});
}

TEST(FuzzySearchSystemTest, UpsertReplacesOldName) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "旧名字"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(1, "新名字"), BmpSystem::Status::kOk);

    EXPECT_TRUE(system->Contains(1));
    EXPECT_TRUE(SearchIds(*system, "旧名").empty());

    const std::vector<std::uint64_t> ids = SearchIds(*system, "新名");
    EXPECT_EQ(ids.size(), std::size_t{1});
    EXPECT_EQ(ids[0], std::uint64_t{1});
}

TEST(FuzzySearchSystemTest, RemoveIsIdempotent) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "宇宙"), BmpSystem::Status::kOk);

    EXPECT_EQ(system->Remove(1), BmpSystem::Status::kOk);
    EXPECT_FALSE(system->Contains(1));
    EXPECT_TRUE(SearchIds(*system, "宇宙").empty());
    EXPECT_EQ(system->Remove(999), BmpSystem::Status::kOk);
}

TEST(FuzzySearchSystemTest, CompactRebuildsAndClearsTombstones) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "大宇宙"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(2, "宇宙会"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(3, "Hello"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Remove(2), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(1, "新宇宙"), BmpSystem::Status::kOk);

    EXPECT_EQ(system->stats().tombstone_documents, std::size_t{2});
    EXPECT_GT(system->stats().delta_postings, std::size_t{0});

    EXPECT_EQ(system->Compact(), BmpSystem::Status::kOk);

    EXPECT_EQ(system->stats().tombstone_documents, std::size_t{0});
    EXPECT_EQ(system->stats().delta_gram_entries, std::size_t{0});
    EXPECT_EQ(system->stats().delta_postings, std::size_t{0});
    EXPECT_EQ(system->stats().live_documents, std::size_t{2});
    EXPECT_GT(system->stats().base_gram_entries, std::size_t{0});

    const std::vector<std::uint64_t> ids = SearchIds(*system, "宇宙");
    EXPECT_EQ(ids.size(), std::size_t{1});
    EXPECT_EQ(ids[0], std::uint64_t{1});
    EXPECT_TRUE(SearchIds(*system, "大宇").empty());

    const std::vector<std::uint64_t> folded = SearchIds(*system, "hello");
    EXPECT_EQ(folded.size(), std::size_t{1});
    EXPECT_EQ(folded[0], std::uint64_t{3});
}

TEST(FuzzySearchSystemTest, CompactReturnsOkOnEmpty) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);

    EXPECT_EQ(system->Compact(), BmpSystem::Status::kOk);
}

TEST(FuzzySearchSystemTest, BmpOnlyRejectsSupplementaryPlane) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);

    const std::string emoji_name = "a\xF0\x9F\x98\x80";
    EXPECT_EQ(system->Add(1, emoji_name), BmpSystem::Status::kUnsupportedCharacter);

    std::vector<std::uint64_t> ids;
    EXPECT_EQ(system->Search("\xF0\x9F\x98\x80", 10, ids), BmpSystem::Status::kUnsupportedCharacter);
    EXPECT_EQ(system->Add(2, "ab"), BmpSystem::Status::kOk);
}

TEST(FuzzySearchSystemTest, FullUnicodeSupportsSupplementaryPlane) {
    FullSystem::Options options;
    options.max_string_length = 8;
    options.min_search_length = 1;
    options.case_insensitive = true;

    auto system = FullSystem::Create(options);
    ASSERT_NE(system, nullptr);

    const std::string emoji_name = "a\xF0\x9F\x98\x80";
    ASSERT_EQ(system->Add(1, emoji_name), FullSystem::Status::kOk);

    std::vector<std::uint64_t> ids;
    EXPECT_EQ(system->Search("\xF0\x9F\x98\x80", 10, ids), FullSystem::Status::kOk);
    ASSERT_EQ(ids.size(), std::size_t{1});
    EXPECT_EQ(ids[0], std::uint64_t{1});

    EXPECT_EQ(system->Search("a", 10, ids), FullSystem::Status::kOk);
    EXPECT_EQ(ids.size(), std::size_t{1});
}

TEST(FuzzySearchSystemTest, RejectsInvalidStrings) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);

    EXPECT_EQ(system->Add(1, ""), BmpSystem::Status::kStringTooShort);
    EXPECT_EQ(system->Add(2, "a"), BmpSystem::Status::kStringTooShort);
    EXPECT_EQ(system->Add(3, "123456789"), BmpSystem::Status::kStringTooLong);
    EXPECT_EQ(system->Add(4, "\xFF"), BmpSystem::Status::kInvalidUtf8);
}

TEST(FuzzySearchSystemTest, RejectsInvalidQueries) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);

    std::vector<std::uint64_t> ids;
    EXPECT_EQ(system->Search("", 10, ids), BmpSystem::Status::kInvalidQuery);
    EXPECT_EQ(system->Search("123456789", 10, ids), BmpSystem::Status::kQueryTooLong);
    EXPECT_EQ(system->Search("\xFF", 10, ids), BmpSystem::Status::kInvalidUtf8);
}

TEST(FuzzySearchSystemTest, InvalidUtf8ReportedBeforeTooLong) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);

    // 9 个码点已超长，但尾部仍有非法字节：非法 UTF-8 优先于长度超限。
    EXPECT_EQ(system->Add(1, "123456789\xFF"), BmpSystem::Status::kInvalidUtf8);

    std::vector<std::uint64_t> ids;
    EXPECT_EQ(system->Search("123456789\xFF", 10, ids), BmpSystem::Status::kInvalidUtf8);
}

TEST(FuzzySearchSystemTest, DuplicateNamesAllowed) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "同名"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(2, "同名"), BmpSystem::Status::kOk);

    const std::vector<std::uint64_t> ids = SearchIds(*system, "同名");
    EXPECT_EQ(ids.size(), std::size_t{2});
    EXPECT_TRUE(ContainsId(ids, 1));
    EXPECT_TRUE(ContainsId(ids, 2));
}

TEST(FuzzySearchSystemTest, ClearResetsStateAndStats) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "宇宙"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(2, "大宇宙"), BmpSystem::Status::kOk);

    system->Clear();

    EXPECT_EQ(system->stats().live_documents, std::size_t{0});
    EXPECT_EQ(system->stats().total_documents, std::size_t{0});
    EXPECT_FALSE(system->Contains(1));
    EXPECT_TRUE(SearchIds(*system, "宇宙").empty());
}

TEST(FuzzySearchSystemTest, StatsReportCounts) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);
    ASSERT_EQ(system->Add(1, "宇宙"), BmpSystem::Status::kOk);
    ASSERT_EQ(system->Add(2, "大宇宙"), BmpSystem::Status::kOk);

    const BmpSystem::Stats stats = system->stats();
    EXPECT_EQ(stats.live_documents, std::size_t{2});
    EXPECT_EQ(stats.total_documents, std::size_t{2});
    EXPECT_GT(stats.name_bytes, std::size_t{0});
    EXPECT_TRUE(stats.unigram_enabled);
}

TEST(FuzzySearchSystemTest, CompactAndClearWithLogging) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);

    // 空实例走 Compact 的快速返回路径，会写一条 DEBUG 日志。
    EXPECT_EQ(system->Compact(), BmpSystem::Status::kOk);

    ASSERT_EQ(system->Add(1, "宇宙"), BmpSystem::Status::kOk);
    // 有 delta 时走完整重建路径，会写 INFO 日志。
    EXPECT_EQ(system->Compact(), BmpSystem::Status::kOk);

    // Clear 会写 INFO 日志。
    system->Clear();
    EXPECT_EQ(system->stats().total_documents, std::size_t{0});
}

TEST(FuzzySearchSystemTest, RandomizedMatchesBruteForce) {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    ASSERT_NE(system, nullptr);

    std::mt19937 rng(12345);
    const std::vector<std::string> alphabet = {"a", "b", "宇", "宙", "天", "会"};

    auto make_random_name = [&rng, &alphabet]() {
        // 2..5 个码点。
        const std::size_t length = 2 + (rng() % 4);
        std::string name;
        for (std::size_t i = 0; i < length; ++i) {
            name += alphabet[rng() % alphabet.size()];
        }
        return name;
    };
    auto make_random_query = [&rng, &alphabet]() {
        // 1..4 个码点。
        const std::size_t length = 1 + (rng() % 4);
        std::string query;
        for (std::size_t i = 0; i < length; ++i) {
            query += alphabet[rng() % alphabet.size()];
        }
        return query;
    };

    std::unordered_map<std::uint64_t, std::string> live;
    std::uint64_t next_id = 1;

    for (int i = 0; i < 300; ++i) {
        const int operation = static_cast<int>(rng() % 10);
        if (operation < 6) {
            std::uint64_t id = next_id++;
            if (!live.empty() && (rng() % 2 == 0)) {
                auto it = live.begin();
                std::advance(it, rng() % live.size());
                id = it->first;
            }
            const std::string name = make_random_name();
            ASSERT_EQ(system->Add(id, name), BmpSystem::Status::kOk);
            live[id] = name;
        } else if (operation < 8) {
            if (!live.empty()) {
                auto it = live.begin();
                std::advance(it, rng() % live.size());
                ASSERT_EQ(system->Remove(it->first), BmpSystem::Status::kOk);
                live.erase(it);
            }
        } else {
            ASSERT_EQ(system->Compact(), BmpSystem::Status::kOk);
        }
    }

    for (const auto& entry : live) {
        EXPECT_TRUE(system->Contains(entry.first));
    }

    for (int i = 0; i < 100; ++i) {
        const std::string query = make_random_query();
        std::vector<std::uint64_t> actual;
        ASSERT_EQ(system->Search(query, BmpSystem::kMaxResults, actual), BmpSystem::Status::kOk);

        std::vector<std::uint64_t> expected;
        for (const auto& entry : live) {
            if (entry.second.find(query) != std::string::npos) {
                expected.push_back(entry.first);
            }
        }
        std::sort(actual.begin(), actual.end());
        std::sort(expected.begin(), expected.end());
        EXPECT_EQ(actual, expected);
    }
}

TEST(FuzzySearchDetailTest, DecodeUtf8ValidatesStrictly) {
    std::array<char32_t, 8> codepoints{};

    const std::string_view invalid_inputs[] = {
        // overlong 编码 '/'
        "\xC0\xAF",
        // 代理区 U+D800
        "\xED\xA0\x80",
        // 超过 U+10FFFF
        "\xF4\x90\x80\x80",
        // 截断的三字节序列
        "\xE4\xB8",
    };
    for (std::string_view input : invalid_inputs) {
        const everest::FuzzySearchDecodeResult result = everest::FuzzySearchUtf8::Decode(input, 8, codepoints);
        EXPECT_EQ(result.status, everest::FuzzySearchStatus::kInvalidUtf8);
    }

    const everest::FuzzySearchDecodeResult valid = everest::FuzzySearchUtf8::Decode("\xE4\xB8\xAD\xF0\x9F\x98\x80", 8, codepoints);
    EXPECT_EQ(valid.status, everest::FuzzySearchStatus::kOk);
    EXPECT_EQ(valid.codepoint_count, std::size_t{2});
    EXPECT_EQ(codepoints[0], U'中');
    EXPECT_EQ(codepoints[1], U'\U0001F600');
}

TEST(FuzzySearchDetailTest, DecodeUtf8StopsStoringAtLimit) {
    std::array<char32_t, 4> codepoints{};

    const everest::FuzzySearchDecodeResult result = everest::FuzzySearchUtf8::Decode("abc", 2, codepoints);

    EXPECT_EQ(result.status, everest::FuzzySearchStatus::kOk);
    EXPECT_EQ(result.codepoint_count, std::size_t{3});
}

TEST(FuzzySearchDetailTest, PackedPostingRoundTrip) {
    const std::array<everest::FuzzySearchDocId, 4> ids = {1, 5, 300, 100000};
    std::vector<std::uint8_t> encoded;

    everest::FuzzySearchPostingCodec::EncodePostingList(ids, encoded);
    everest::FuzzySearchPackedPostingCursor cursor(encoded, static_cast<std::uint32_t>(ids.size()));

    std::vector<everest::FuzzySearchDocId> decoded;
    everest::FuzzySearchDocId value = 0;
    while (cursor.HasNext()) {
        ASSERT_TRUE(cursor.Next(&value));
        decoded.push_back(value);
    }

    EXPECT_EQ(decoded, std::vector<everest::FuzzySearchDocId>(ids.begin(), ids.end()));
    EXPECT_FALSE(cursor.HasNext());
}

TEST(FuzzySearchDetailTest, GramKeyPacking) {
    const char32_t first = U'中';
    const char32_t second = U'文';

    EXPECT_EQ(everest::FuzzySearchGramKey::MakeBigram<true>(first, second), (static_cast<std::uint32_t>(first) << 16) | static_cast<std::uint32_t>(second));
    EXPECT_EQ(everest::FuzzySearchGramKey::MakeBigram<false>(first, second), (static_cast<std::uint64_t>(first) << 32) | static_cast<std::uint64_t>(second));
    EXPECT_EQ(everest::FuzzySearchGramKey::MakeUnigram(first), static_cast<std::uint32_t>(first));
}
