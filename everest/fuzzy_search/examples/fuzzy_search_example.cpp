// fuzzy_search 库的最小使用示例：构造、添加、查询、删除和 Compact。
#include "system/fuzzy_search/fuzzy_search_system.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using System = metaverse::FuzzySearchSystem<std::uint64_t, true>;

    System::Options options;
    options.maxStringLength = 8;
    options.minSearchLength = 1;
    options.caseInsensitive = true;

    System::Status status = System::Status::kOk;
    auto system = System::Create(options, &status);
    if (system == nullptr) {
        std::cerr << "create failed, status="
                  << static_cast<int>(status) << "\n";
        return 1;
    }

    // 添加三条名字；同一个 id 再次 Add 会覆盖旧名字。
    system->Add(1001, "大宇宙");
    system->Add(1002, "宇宙会");
    system->Add(1003, "Hello");

    // 1 字、2 字和多字查询都会命中任意位置子串。
    const std::vector<std::string> queries = {"宇", "宇宙", "hello", "大宇"};
    for (const std::string& query : queries) {
        std::vector<std::uint64_t> ids;
        const auto result = system->Search(query, System::kMaxResults, ids);
        std::cout << "query [" << query << "] status="
                  << static_cast<int>(result) << " ids:";
        for (std::uint64_t id : ids) {
            std::cout << " " << id;
        }
        std::cout << "\n";
    }

    // 删除后调用 Compact，合并 base + delta 并清理墓碑。
    system->Remove(1002);
    system->Compact();

    const auto stats = system->stats();
    std::cout << "live=" << stats.liveDocuments
              << " tombstones=" << stats.tombstoneDocuments
              << " nameBytes=" << stats.nameBytes
              << " estimatedBytes=" << stats.estimatedBytes << "\n";

    return 0;
}
