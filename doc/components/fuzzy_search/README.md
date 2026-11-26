# fuzzy_search

`everest::FuzzySearchSystem` 是一个与业务无关的 C++20 header-only 内存子串检索库。

输入是 UTF-8 字符串和一个 64 位整数 id，输出是匹配的 id。库不认识公会、服务、分片、排序、分页和客户端协议；调用方负责把 id 映射回自己的业务对象，并完成排序和展示。

## 1. 它是什么

- 任意位置子串匹配：查询“宇宙”可以命中“大宇宙”和“宇宙会”。
- 支持 1 到 `max_string_length` 个 Unicode 码点的查询。
- 默认不区分 ASCII 大小写。
- 每个字符串关联一个 `int64_t` / `uint64_t` id；不保存业务 payload。
- 单实例、单线程使用；每个服务进程构造一个实例。
- header-only；只依赖框架的 `common/define.h` 与 `spdlog_wrapper/everest_spdlog.h`（日志）。

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
#include "fuzzy_search/fuzzy_search_system.h"

using System = everest::FuzzySearchSystem<std::uint64_t, true>;  // BmpOnly=true

System::Options options;
options.max_string_length = 8;
options.min_search_length = 1;
options.case_insensitive = true;

System::Status status = System::Status::kOk;
auto system = System::Create(options, &status);
if (system == nullptr) {
    // status 给出失败原因（kInvalidOptions / kAlreadyCreated）
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
const System::Status compact_status = system->Compact();  // 合并 base + delta，清理墓碑
```

## 4. 配置

```cpp
struct FuzzySearchOptions {
    std::uint32_t max_string_length = 64;  // [2, 256]，按 Unicode 码点计
    std::uint32_t min_search_length = 1;   // [1, max_string_length]
    bool case_insensitive = true;          // 仅折叠 ASCII A-Z
};
```

- 字符串最小长度固定为 2 个码点；`max_string_length` 上限为 `FuzzySearchSystem::kMaxStringLength`（256）。
- 仅当 `min_search_length == 1` 时构建 unigram；否则只构建 bigram，1 字查询返回 `kQueryTooShort`。
- `BmpOnly == true` 时只接受 BMP 码点（`<= 0xFFFF`），bigram key 使用 32 位；`false` 时支持完整 Unicode，bigram key 使用 64 位。
- 名字为 UTF-8；非法 UTF-8 返回 `kInvalidUtf8`。
- 错误优先级：字节数超过 `4 * max_string_length` 时直接判超长（不扫描）；否则完整解码，非法字节优先返回 `kInvalidUtf8`，再判断长度。

## 5. API

| 接口 | 说明 |
|---|---|
| `static std::unique_ptr<FuzzySearchSystem> Create(const Options&, Status* = nullptr)` | 唯一创建入口；参数非法或已有实例返回 `nullptr` |
| `static Status ValidateOptions(const Options&)` | 只校验参数，不分配 |
| `Status Add(IdT id, std::string_view utf8_name)` | 插入或覆盖（同 id 为 upsert）；失败时不改动索引 |
| `Status Remove(IdT id)` | 幂等；id 不存在返回 `kOk` |
| `bool Contains(IdT id)` | O(1) 存活判断 |
| `template <typename Visitor> Status Search(query, max_results, visitor)` | visitor 返回 `false` 提前停止；要求 `std::invocable<Visitor&, IdT>` |
| `Status Search(query, max_results, std::vector<IdT>& out)` | vector 版本；先校验参数再清空写入，结果顺序不承诺 |
| `void Reserve(document_count, name_bytes = 0)` | 容量提示 |
| `Status Compact()` | 合并 base + delta，清理墓碑；同步执行；失败时保留旧索引 |
| `void Clear()` | 清空数据，保留配置 |
| `Stats stats()` | 文档数、名字字节数、gram 数、posting 数、估算内存 |

- 返回 `Status` 的接口都带 `[[nodiscard]]`，调用方必须处理返回值。
- `max_results` 必须在 `1..500`（`kMaxResults`）之间，否则返回 `kInvalidMaxResults`。
- 名字 arena 超出 uint32 偏移范围时 `Add` 返回 `kNameArenaFull`（旧版本统一返回 `kTooManyDocuments`）。

## 6. 存储与 Compact

- **base**：`Compact()` 时由存活文档构建的紧凑只读索引，包含按 key 排序的 gram 表和 delta+varint 编码的扁平 posting。
- **delta**：运行期 `Add` / `Remove` 写入的可变索引，每个 gram 一个 `std::vector<DocId>`，删除只打墓碑。
- 查询同时查 base 和 delta，跳过墓碑；`Compact()` 合并两者并丢弃墓碑。
- 文档号全局唯一、单调递增；base 中的文档号一定小于 delta 中的文档号。
- 没有墓碑且 delta 为空时，`Compact()` 直接快速返回 `kOk`。
- 不调用 `Compact()` 时墓碑会持续占用内存；建议按 `stats().tombstone_documents` 或业务低峰定期调用。
- `Compact()` 失败（名字 arena 或文档号超限、名字解码失败）时返回错误码，且不改动任何索引成员，调用方可以重试或回退。

## 7. 复杂度

| 查询长度 Q | 操作 | 复杂度 |
|---|---|---|
| Q = 1 | unigram posting 遍历 | O(posting) |
| Q = 2 | bigram posting 遍历 | O(posting) |
| 3 <= Q <= max | Q-1 个 bigram 列表求交 + 原文校验 | 取决于最短 posting 列表 |

- 结果数达到 `max_results` 或 visitor 返回 false 时立即停止。
- 3 码点以上的查询把 Q-1 个列表写入一块连续缓冲，不在每次查询时逐个列表分配。
- `Add` / `Search` 热路径使用栈上固定缓冲（码点与折叠字节），不产生堆分配。
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
- 守卫是进程级的，**被所有模板实例化共享**：`FuzzySearchSystem<uint64_t, true>` 与 `FuzzySearchSystem<uint64_t, false>` 不能同时存在。
- 默认守卫在 `detail/fuzzy_search_lifecycle.h`；定义 `FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES` 可关闭守卫，供允许同进程多实例的项目使用。
- 类禁止拷贝和移动；只能通过 `std::unique_ptr` 持有。

## 10. 日志与链接

- 日志直接使用框架宏 `EVEREST_LOG_INFO/DEBUG/ERROR`（`common/define.h`），实现来自 `spdlog_wrapper/everest_spdlog.h`。
- 首次使用日志会惰性创建名为 `everest` 的 spdlog logger（默认异步、控制台 + 每日/轮转文件）。
- 因此任何实例化 `FuzzySearchSystem` 的翻译单元都需要链接 `everest.lib`，include 目录需要 `3rd/spd/include`。
- 日志记录点：`Create` 参数非法 / 已被创建、`Compact` 开始 / 结束 / 跳过 / 失败、`Clear`；`Add` / `Remove` / `Search` 正常路径不打日志。

## 11. 构建与测试

库本身是 header-only，不需要单独编译；编译覆盖与行为验证由 `test/fuzzy_search_test` 承担。

Windows（先构建核心库，再构建测试）：

```bat
msbuild windows_sln\erverest.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild test\fuzzy_search_test\fuzzy_search_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
test\fuzzy_search_test\x64\Debug\fuzzy_search_test.exe
```

Linux（WSL 的 Linux 发行版内，先构建 `libs/Debug/libeverest.a`）：

```bash
cd linux_build && bash build.sh
cd test/fuzzy_search_test && bash build.sh
./bin/fuzzy_search_test
```

测试链接 `libeverest.a` 时还会需要 protobuf 静态库（统一编译单元包含 `packet_serialize.cpp`）：Windows 使用 `libs/libprotobufd.lib`，Linux 使用 `libs/<构建类型>/libprotobuf.a`。

测试源码：`test/fuzzy_search_test/fuzzy_search_test.cpp`；示例：`test/fuzzy_search_test/fuzzy_search_example.cpp`；基准：`benchmarks/fuzzy_search/fuzzy_search_benchmark.cpp`（Windows 专用，手动构建运行，见该目录的 `README.md`）。

`benchmarks/fuzzy_search/report/` 与 `benchmarks/fuzzy_search/results/` 是历史基准快照，不随代码变更更新。

## 12. License

待项目确认后补充。
