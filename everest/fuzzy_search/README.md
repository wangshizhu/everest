# fuzzy_search

`metaverse::FuzzySearchSystem` 是一个与业务无关的 C++20 header-only 内存子串检索库。

输入是 UTF-8 字符串和一个 64 位整数 id，输出是匹配的 id。库不认识公会、服务、分片、排序、分页和客户端协议；调用方负责把 id 映射回自己的业务对象，并完成排序和展示。

## 1. 它是什么

- 任意位置子串匹配：查询“宇宙”可以命中“大宇宙”和“宇宙会”。
- 支持 1 到 `maxStringLength` 个 Unicode 码点的查询。
- 默认不区分 ASCII 大小写。
- 每个字符串关联一个 `int64_t` / `uint64_t` id；不保存业务 payload。
- 单实例、单线程使用；每个服务进程构造一个实例。
- header-only；除日志适配层外，不依赖 gameserver / Lua / FTEngine / protobuf。

## 2. 它不是什么

- 不做排序、评分、相关性、分页。
- 不做拼音、分词、同义词、纠错、模糊音。
- 不做 Unicode NFC/NFKC、繁简转换、全半角转换。
- 不做完整 Unicode 大小写折叠；只折叠 ASCII `A-Z`。
- 不强制名字唯一；不同 id 可以有相同名字。
- 不提供持久化、序列化、恢复；进程重启后需要重新 `Add`。
- 不加锁；不能并发调用。
- 不跨线程、不跨进程共享。

## 3. 快速开始

```cpp
#include "system/fuzzy_search/fuzzy_search_system.h"

using System = metaverse::FuzzySearchSystem<std::uint64_t, true>;  // BmpOnly=true

System::Options options;
options.maxStringLength = 8;
options.minSearchLength = 1;
options.caseInsensitive = true;

System::Status status = System::Status::kOk;
auto system = System::Create(options, &status);
if (system == nullptr) {
    // status 给出非法参数原因
}

system->Add(1001, "大宇宙");
system->Add(1002, "宇宙会");

std::vector<std::uint64_t> ids;
system->Search("宇宙", 500, ids);          // ids: 1001, 1002
system->Search("宇", 500, [](std::uint64_t id) {
    // 返回 false 提前停止
    return true;
});

system->Remove(1002);                      // 幂等
system->Compact();                         // 合并 base + delta，清理墓碑
```

## 4. 配置

```cpp
struct FuzzySearchOptions {
    std::uint32_t maxStringLength = 0;  // 必填，>= 2，按 Unicode 码点计
    std::uint32_t minSearchLength = 1;  // 必填，1..maxStringLength
    bool caseInsensitive = true;        // 仅折叠 ASCII A-Z
};
```

- 字符串最小长度固定为 2 个码点。
- 仅当 `minSearchLength == 1` 时构建 unigram；否则只构建 bigram，1 字查询返回 `kQueryTooShort`。
- `BmpOnly == true` 时只接受 BMP 码点（`<= 0xFFFF`），bigram key 使用 32 位；`false` 时支持完整 Unicode，bigram key 使用 64 位。
- 名字为 UTF-8；非法 UTF-8 返回 `kInvalidUtf8`。

## 5. API

| 接口 | 说明 |
|---|---|
| `static std::unique_ptr<FuzzySearchSystem> Create(const Options&, Status* = nullptr)` | 唯一创建入口；参数非法返回 `nullptr` |
| `static Status ValidateOptions(const Options&)` | 只校验参数，不分配 |
| `Status Add(IdT id, std::string_view utf8Name)` | 插入或覆盖（同 id 为 upsert） |
| `Status Remove(IdT id)` | 幂等；id 不存在返回 `kOk` |
| `bool Contains(IdT id)` | O(1) 存活判断 |
| `template <typename Visitor> Status Search(query, maxResults, visitor)` | visitor 返回 `false` 提前停止 |
| `Status Search(query, maxResults, std::vector<IdT>& out)` | vector 版本；结果顺序不承诺 |
| `void Reserve(documentCount, nameBytes = 0)` | 容量提示 |
| `void Compact()` | 合并 base + delta，清理墓碑；同步执行 |
| `void Clear()` | 清空数据，保留配置 |
| `Stats stats()` | 文档数、名字字节数、gram 数、posting 数、估算内存 |

`maxResults` 必须在 `1..500`（`kMaxResults`）之间，否则返回 `kInvalidMaxResults`。

## 6. 存储与 Compact

- **base**：`Compact()` 时由存活文档构建的紧凑只读索引，包含按 key 排序的 gram 表和 delta+varint 编码的扁平 posting。
- **delta**：运行期 `Add` / `Remove` 写入的可变索引，每个 gram 一个 `std::vector<DocId>`，删除只打墓碑。
- 查询同时查 base 和 delta，跳过墓碑；`Compact()` 合并两者并丢弃墓碑。
- 文档号全局唯一、单调递增；base 中的文档号一定小于 delta 中的文档号。
- 不调用 `Compact()` 时墓碑会持续占用内存；建议按 `stats().tombstoneDocuments` 或业务低峰定期调用。

## 7. 复杂度

| 查询长度 Q | 操作 | 复杂度 |
|---|---|---|
| Q = 1 | unigram posting 遍历 | O(posting) |
| Q = 2 | bigram posting 遍历 | O(posting) |
| 3 <= Q <= max | Q-1 个 bigram 列表求交 + 原文校验 | 取决于最短 posting 列表 |

- 结果数达到 `maxResults` 或 visitor 返回 false 时立即停止。
- `Compact()` 是 O(存活文档数 × 平均名字长度)，并可能产生旧 base + 新 base 的瞬时内存峰值。

## 8. 内存估算

以 500 万条名字、平均 6 个码点、CJK 为主为例（全服合计，按分片分摊时除以分片数）：

| 项 | 估算 |
|---|---|
| 名字原文（UTF-8） | 约 90MB |
| unigram posting | 约 60–90MB |
| bigram posting | 约 50–75MB |
| bigram 词表 | 约 16–72MB |
| 文档号 → 外部 id | 约 40MB |
| 名字 offset/length | 约 25–40MB |
| **合计** | **约 300–400MB** |

注意：以上为紧凑结构估算；delta 的 `std::unordered_map` 和未 `Compact` 的墓碑会增加额外开销。

## 9. 线程安全与实例模型

- 库内没有任何锁；所有接口必须在同一个线程调用。
- 同一进程只允许一个存活实例；第二次 `Create` 返回 `nullptr` + `kAlreadyCreated`，实例析构后可重新创建。
- 默认的进程级守卫在 `detail/fuzzy_search_lifecycle.h`；定义 `FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES` 可关闭守卫，供允许同进程多实例的项目使用。
- 类禁止拷贝和移动；只能通过 `std::unique_ptr` 持有。

## 10. 日志与移植

- 唯一引用 gameserver 日志的文件是 `detail/fuzzy_search_log.h`。
- 迁移到新项目时，只替换这个文件为 no-op 或目标项目的日志实现即可，其余代码不变。
- 也可以编译时定义 `FUZZY_SEARCH_DISABLE_GAMESERVER_LOG`，让日志宏变成 no-op。
- 单实例守卫在 `detail/fuzzy_search_lifecycle.h`；允许同进程多实例的项目定义 `FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES` 关闭守卫。
- 日志记录点：`Create` 参数非法、`Compact` 开始/结束、`Clear`；`Add` / `Remove` / `Search` 正常路径不打日志。

## 11. 构建与测试

库本身是 header-only，无需编译。仓库内提供独立测试目标：

```bat
cmake --preset x64-debug
cmake --build --preset win-vs-x64-debug --target fuzzy_search_tests
```

测试源码：`tests/fuzzy_search_system_test.cpp`；示例：`examples/fuzzy_search_example.cpp`。

测试目标使用 `FUZZY_SEARCH_DISABLE_GAMESERVER_LOG`，因此不依赖 gameserver 日志链接。

## 12. License

待项目确认后补充。
