// fuzzy_search 库的单元测试：覆盖参数校验、增删查、Compact、BMP 模式和随机差分测试。
#include "system/fuzzy_search/fuzzy_search_system.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

int gFailures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed: " #condition " at " << __FILE__ << ":" \
                      << __LINE__ << "\n";                                      \
            ++gFailures;                                                        \
        }                                                                       \
    } while (false)

using BmpSystem = metaverse::FuzzySearchSystem<std::uint64_t, true>;
using FullSystem = metaverse::FuzzySearchSystem<std::uint64_t, false>;

BmpSystem::Options MakeOptions(std::uint32_t maxLength, std::uint32_t minSearch, bool caseInsensitive) {
    BmpSystem::Options options;
    options.maxStringLength = maxLength;
    options.minSearchLength = minSearch;
    options.caseInsensitive = caseInsensitive;
    return options;
}

std::vector<std::uint64_t> SearchIds(const BmpSystem& system, std::string_view query, std::size_t maxResults = BmpSystem::kMaxResults) {
    std::vector<std::uint64_t> ids;
    const auto status = system.Search(query, maxResults, ids);
    CHECK(status == BmpSystem::Status::kOk);
    return ids;
}

bool ContainsId(const std::vector<std::uint64_t>& ids, std::uint64_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

// 验证进程级单实例守卫：第二次 Create 失败，实例析构后可重新创建。
void TestSingleInstanceGuard() {
    BmpSystem::Status status = BmpSystem::Status::kOk;
    auto first = BmpSystem::Create(MakeOptions(8, 1, true), &status);
    CHECK(first != nullptr);
    CHECK(status == BmpSystem::Status::kOk);

    auto second = BmpSystem::Create(MakeOptions(8, 1, true), &status);
    CHECK(second == nullptr);
    CHECK(status == BmpSystem::Status::kAlreadyCreated);

    first.reset();
    auto third = BmpSystem::Create(MakeOptions(8, 1, true), &status);
    CHECK(third != nullptr);
    CHECK(status == BmpSystem::Status::kOk);
}

void TestOptionsAndCreate() {
    BmpSystem::Status status = BmpSystem::Status::kOk;
    {
        auto system = BmpSystem::Create(MakeOptions(1, 1, true), &status);
        CHECK(system == nullptr);
        CHECK(status == BmpSystem::Status::kInvalidOptions);
    }
    {
        auto system = BmpSystem::Create(MakeOptions(8, 0, true), &status);
        CHECK(system == nullptr);
        CHECK(status == BmpSystem::Status::kInvalidOptions);
    }
    {
        auto system = BmpSystem::Create(MakeOptions(8, 9, true), &status);
        CHECK(system == nullptr);
        CHECK(status == BmpSystem::Status::kInvalidOptions);
    }
    {
        auto system = BmpSystem::Create(MakeOptions(8, 1, true), &status);
        CHECK(system != nullptr);
        CHECK(status == BmpSystem::Status::kOk);
    }
}

void TestAddAndSearch() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    CHECK(system->Add(1001, "大宇宙") == BmpSystem::Status::kOk);
    CHECK(system->Add(1002, "宇宙会") == BmpSystem::Status::kOk);
    CHECK(system->Add(1003, "Hello") == BmpSystem::Status::kOk);
    CHECK(system->Contains(1001));

    auto ids = SearchIds(*system, "宇");
    CHECK(ids.size() == 2);
    CHECK(ContainsId(ids, 1001));
    CHECK(ContainsId(ids, 1002));

    ids = SearchIds(*system, "宇宙");
    CHECK(ids.size() == 2);
    CHECK(ContainsId(ids, 1001));
    CHECK(ContainsId(ids, 1002));

    ids = SearchIds(*system, "大宇");
    CHECK(ids.size() == 1);
    CHECK(ContainsId(ids, 1001));

    ids = SearchIds(*system, "hello");
    CHECK(ids.size() == 1);
    CHECK(ContainsId(ids, 1003));

    ids = SearchIds(*system, "不存在");
    CHECK(ids.empty());
}

void TestMinSearchLength() {
    auto system = BmpSystem::Create(MakeOptions(8, 2, true));
    CHECK(system != nullptr);
    CHECK(system->Add(1, "宇宙") == BmpSystem::Status::kOk);
    CHECK(!system->stats().unigramEnabled);

    std::vector<std::uint64_t> ids;
    CHECK(system->Search("宇", 10, ids) == BmpSystem::Status::kQueryTooShort);
    CHECK(system->Search("宇宙", 10, ids) == BmpSystem::Status::kOk);
    CHECK(ids.size() == 1);
    CHECK(ids[0] == 1);
}

void TestCaseSensitivity() {
    {
        auto system = BmpSystem::Create(MakeOptions(8, 1, true));
        CHECK(system != nullptr);
        CHECK(system->Add(1, "Hello") == BmpSystem::Status::kOk);
        CHECK(SearchIds(*system, "hello").size() == 1);
        CHECK(SearchIds(*system, "HELLO").size() == 1);
    }
    {
        auto system = BmpSystem::Create(MakeOptions(8, 1, false));
        CHECK(system != nullptr);
        CHECK(system->Add(1, "Hello") == BmpSystem::Status::kOk);
        CHECK(SearchIds(*system, "hello").empty());
        CHECK(SearchIds(*system, "Hello").size() == 1);
    }
}

// 验证多个 gram 同时存在但不相邻时不会误报。
void TestFalsePositive() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    CHECK(system->Add(1, "abXbc") == BmpSystem::Status::kOk);

    // "ab" and "bc" are both present, but "abc" is not contiguous.
    CHECK(SearchIds(*system, "abc").empty());
    CHECK(SearchIds(*system, "ab").size() == 1);
    CHECK(SearchIds(*system, "bc").size() == 1);
}

void TestMaxResultsAndVisitor() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    for (int i = 0; i < 10; ++i) {
        CHECK(system->Add(static_cast<std::uint64_t>(100 + i), "测试" + std::to_string(i)) == BmpSystem::Status::kOk);
    }

    std::vector<std::uint64_t> ids;
    CHECK(system->Search("测试", 3, ids) == BmpSystem::Status::kOk);
    CHECK(ids.size() == 3);
    CHECK(system->Search("测试", 0, ids) == BmpSystem::Status::kInvalidMaxResults);
    CHECK(system->Search("测试", BmpSystem::kMaxResults + 1, ids) == BmpSystem::Status::kInvalidMaxResults);

    std::size_t visited = 0;
    CHECK(system->Search("测试", 100, [&visited](std::uint64_t) { ++visited; return visited < 2; }) == BmpSystem::Status::kOk);
    CHECK(visited == 2);

    visited = 0;
    CHECK(system->Search("测试", 5, [&visited](std::uint64_t) { ++visited; return true; }) == BmpSystem::Status::kOk);
    CHECK(visited == 5);
}

void TestUpsertAndRemove() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    CHECK(system->Add(1, "旧名字") == BmpSystem::Status::kOk);
    CHECK(system->Add(1, "新名字") == BmpSystem::Status::kOk);
    CHECK(system->Contains(1));

    CHECK(SearchIds(*system, "旧名").empty());
    auto ids = SearchIds(*system, "新名");
    CHECK(ids.size() == 1);
    CHECK(ids[0] == 1);

    CHECK(system->Remove(1) == BmpSystem::Status::kOk);
    CHECK(!system->Contains(1));
    CHECK(SearchIds(*system, "新名").empty());
    CHECK(system->Remove(999) == BmpSystem::Status::kOk);
}

// 验证 Compact 前后查询结果一致，墓碑和 delta 被清理。
void TestCompact() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    CHECK(system->Add(1, "大宇宙") == BmpSystem::Status::kOk);
    CHECK(system->Add(2, "宇宙会") == BmpSystem::Status::kOk);
    CHECK(system->Add(3, "Hello") == BmpSystem::Status::kOk);
    CHECK(system->Remove(2) == BmpSystem::Status::kOk);
    CHECK(system->Add(1, "新宇宙") == BmpSystem::Status::kOk);

    CHECK(system->stats().tombstoneDocuments == 2);
    CHECK(system->stats().deltaPostings > 0);

    system->Compact();

    CHECK(system->stats().tombstoneDocuments == 0);
    CHECK(system->stats().deltaGramEntries == 0);
    CHECK(system->stats().deltaPostings == 0);
    CHECK(system->stats().liveDocuments == 2);
    CHECK(system->stats().baseGramEntries > 0);

    auto ids = SearchIds(*system, "宇宙");
    CHECK(ids.size() == 1);
    CHECK(ids[0] == 1);
    CHECK(SearchIds(*system, "大宇").empty());
    ids = SearchIds(*system, "hello");
    CHECK(ids.size() == 1);
    CHECK(ids[0] == 3);
}

// 验证 BmpOnly 模式拒绝 BMP 之外的码点。
void TestBmpOnly() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    CHECK(system->Add(1, "a\xF0\x9F\x98\x80") == BmpSystem::Status::kUnsupportedCharacter);

    std::vector<std::uint64_t> ids;
    CHECK(system->Search("\xF0\x9F\x98\x80", 10, ids) == BmpSystem::Status::kUnsupportedCharacter);
    CHECK(system->Add(2, "ab") == BmpSystem::Status::kOk);
}

// 验证完整 Unicode 模式支持补充平面字符。
void TestFullUnicode() {
    FullSystem::Options options;
    options.maxStringLength = 8;
    options.minSearchLength = 1;
    options.caseInsensitive = true;
    auto system = FullSystem::Create(options);
    CHECK(system != nullptr);

    CHECK(system->Add(1, "a\xF0\x9F\x98\x80") == FullSystem::Status::kOk);
    std::vector<std::uint64_t> ids;
    CHECK(system->Search("\xF0\x9F\x98\x80", 10, ids) == FullSystem::Status::kOk);
    CHECK(ids.size() == 1);
    CHECK(ids[0] == 1);
    CHECK(system->Search("a", 10, ids) == FullSystem::Status::kOk);
    CHECK(ids.size() == 1);
}

void TestValidationErrors() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    std::vector<std::uint64_t> ids;

    CHECK(system->Add(1, "") == BmpSystem::Status::kStringTooShort);
    CHECK(system->Add(2, "a") == BmpSystem::Status::kStringTooShort);
    CHECK(system->Add(3, "123456789") == BmpSystem::Status::kStringTooLong);
    CHECK(system->Add(4, "\xFF") == BmpSystem::Status::kInvalidUtf8);

    CHECK(system->Search("", 10, ids) == BmpSystem::Status::kInvalidQuery);
    CHECK(system->Search("123456789", 10, ids) == BmpSystem::Status::kQueryTooLong);
    CHECK(system->Search("\xFF", 10, ids) == BmpSystem::Status::kInvalidUtf8);
}

void TestDuplicateNames() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    CHECK(system->Add(1, "同名") == BmpSystem::Status::kOk);
    CHECK(system->Add(2, "同名") == BmpSystem::Status::kOk);

    auto ids = SearchIds(*system, "同名");
    CHECK(ids.size() == 2);
    CHECK(ContainsId(ids, 1));
    CHECK(ContainsId(ids, 2));
}

void TestClearAndStats() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);
    CHECK(system->Add(1, "宇宙") == BmpSystem::Status::kOk);
    CHECK(system->Add(2, "大宇宙") == BmpSystem::Status::kOk);

    auto stats = system->stats();
    CHECK(stats.liveDocuments == 2);
    CHECK(stats.totalDocuments == 2);
    CHECK(stats.nameBytes > 0);
    CHECK(stats.unigramEnabled);

    system->Clear();
    CHECK(system->stats().liveDocuments == 0);
    CHECK(system->stats().totalDocuments == 0);
    CHECK(!system->Contains(1));

    std::vector<std::uint64_t> ids;
    CHECK(system->Search("宇宙", 10, ids) == BmpSystem::Status::kOk);
    CHECK(ids.empty());
}

// 随机增删改 + 暴力扫描对比，验证 base/delta/墓碑组合下的正确性。
void TestRandomizedAgainstBruteForce() {
    auto system = BmpSystem::Create(MakeOptions(8, 1, true));
    CHECK(system != nullptr);

    std::mt19937 rng(12345);
    const std::vector<std::string> alphabet = {"a", "b", "宇", "宙", "天", "会"};

    auto makeRandomName = [&rng, &alphabet]() {
        const std::size_t length = 2 + (rng() % 4);  // 2..5 code points
        std::string name;
        for (std::size_t i = 0; i < length; ++i) {
            name += alphabet[rng() % alphabet.size()];
        }
        return name;
    };
    auto makeRandomQuery = [&rng, &alphabet]() {
        const std::size_t length = 1 + (rng() % 4);  // 1..4 code points
        std::string query;
        for (std::size_t i = 0; i < length; ++i) {
            query += alphabet[rng() % alphabet.size()];
        }
        return query;
    };

    std::unordered_map<std::uint64_t, std::string> live;
    std::uint64_t nextId = 1;

    for (int i = 0; i < 300; ++i) {
        const int operation = static_cast<int>(rng() % 10);
        if (operation < 6) {
            std::uint64_t id = nextId++;
            if (!live.empty() && (rng() % 2 == 0)) {
                auto it = live.begin();
                std::advance(it, rng() % live.size());
                id = it->first;
            }
            const std::string name = makeRandomName();
            CHECK(system->Add(id, name) == BmpSystem::Status::kOk);
            live[id] = name;
        } else if (operation < 8) {
            if (!live.empty()) {
                auto it = live.begin();
                std::advance(it, rng() % live.size());
                CHECK(system->Remove(it->first) == BmpSystem::Status::kOk);
                live.erase(it);
            }
        } else {
            system->Compact();
        }
    }

    for (const auto& entry : live) {
        CHECK(system->Contains(entry.first));
    }

    for (int i = 0; i < 100; ++i) {
        const std::string query = makeRandomQuery();
        std::vector<std::uint64_t> actual;
        CHECK(system->Search(query, BmpSystem::kMaxResults, actual) == BmpSystem::Status::kOk);

        std::vector<std::uint64_t> expected;
        for (const auto& entry : live) {
            if (entry.second.find(query) != std::string::npos) {
                expected.push_back(entry.first);
            }
        }
        std::sort(actual.begin(), actual.end());
        std::sort(expected.begin(), expected.end());
        CHECK(actual == expected);
    }
}

}  // namespace

// 依次运行全部测试；返回非 0 表示存在失败。
int main() {
    TestSingleInstanceGuard();
    TestOptionsAndCreate();
    TestAddAndSearch();
    TestMinSearchLength();
    TestCaseSensitivity();
    TestFalsePositive();
    TestMaxResultsAndVisitor();
    TestUpsertAndRemove();
    TestCompact();
    TestBmpOnly();
    TestFullUnicode();
    TestValidationErrors();
    TestDuplicateNames();
    TestClearAndStats();
    TestRandomizedAgainstBruteForce();

    if (gFailures == 0) {
        std::cout << "fuzzy_search_system_test: all tests passed\n";
        return 0;
    }
    std::cerr << "fuzzy_search_system_test: " << gFailures << " failure(s)\n";
    return 1;
}
