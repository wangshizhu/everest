/**
 * @file fuzzy_search_example.cpp
 * @brief fuzzy_search 库的最小使用示例：构造、添加、查询、删除和 Compact。
 */

#include "fuzzy_search/fuzzy_search_system.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief 运行 fuzzy_search 最小示例。
 *
 * @return 成功返回 0，任何一步失败返回 1。
 */
int main() {
    using System = everest::FuzzySearchSystem<std::uint64_t, true>;

    System::Options options;
    options.max_string_length = 8;
    options.min_search_length = 1;
    options.case_insensitive = true;

    System::Status status = System::Status::kOk;
    auto system = System::Create(options, &status);
    if (system == nullptr) {
        std::cerr << "create failed, status=" << static_cast<int>(status) << "\n";
        return 1;
    }

    // 添加三条名字；同一个 id 再次 Add 会覆盖旧名字。
    const std::vector<std::pair<std::uint64_t, std::string>> names = {
        {1001, "大宇宙"},
        {1002, "宇宙会"},
        {1003, "Hello"},
    };
    for (const auto& entry : names) {
        status = system->Add(entry.first, entry.second);
        if (status != System::Status::kOk) {
            std::cerr << "add failed, id=" << entry.first << " status=" << static_cast<int>(status) << "\n";
            return 1;
        }
    }

    // 1 字、2 字和多字查询都会命中任意位置子串。
    const std::vector<std::string> queries = {"宇", "宇宙", "hello", "大宇"};
    for (const std::string& query : queries) {
        std::vector<std::uint64_t> ids;
        status = system->Search(query, System::kMaxResults, ids);
        std::cout << "query [" << query << "] status=" << static_cast<int>(status) << " ids:";
        for (std::uint64_t id : ids) {
            std::cout << " " << id;
        }
        std::cout << "\n";
    }

    // 删除后调用 Compact，合并 base + delta 并清理墓碑。
    status = system->Remove(1002);
    if (status != System::Status::kOk) {
        std::cerr << "remove failed, status=" << static_cast<int>(status) << "\n";
        return 1;
    }

    status = system->Compact();
    if (status != System::Status::kOk) {
        std::cerr << "compact failed, status=" << static_cast<int>(status) << "\n";
        return 1;
    }

    const auto stats = system->stats();
    std::cout << "live=" << stats.live_documents << " tombstones=" << stats.tombstone_documents << " nameBytes=" << stats.name_bytes << " estimatedBytes=" << stats.estimated_bytes
              << "\n";

    return 0;
}
