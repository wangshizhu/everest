# fuzzy_search 设计说明

## 1. 目标与约束

- C++20，header-only；只依赖 `common/define.h`（命名空间宏与 `EVEREST_LOG_*`）和 `spdlog_wrapper/everest_spdlog.h`（日志实现）。
- UTF-8 字符串，一个字符串对应一个 64 位 id。
- 任意位置子串匹配；1 到 `max_string_length` 个码点的查询，`max_string_length` 上限 256。
- 字符串最小长度 2 个码点。
- 仅当 `min_search_length == 1` 时构建 unigram。
- 单实例单线程；索引数据无全局状态，仅有一个进程级单实例守卫（跨模板实例化共享）；无内部锁。
- 禁止拷贝和移动；私有构造 + `std::unique_ptr` 工厂。
- 日志统一走项目日志宏 `EVEREST_LOG_*`，不再提供日志注入层。

## 2. 总体结构

```
FuzzySearchSystem
├── documents_        全量文档表（base + delta 共用）
├── id_to_doc_        外部 id -> 内部 FuzzySearchDocId
├── name_arena_       折叠后的 UTF-8 名字
├── base_index_       紧凑只读索引（Compact 时构建）
└── delta_index_      可变增量索引（Add 时写入）
```

### 2.1 文档表

```cpp
struct DocumentRecord {
    IdT id;                    // 外部 id
    std::uint32_t name_offset; // name_arena_ 字节偏移
    std::uint32_t name_length; // name_arena_ 字节长度
    bool alive;                // false 表示墓碑
};
```

- `FuzzySearchDocId` 是 `std::uint32_t`，全局唯一、单调递增。
- `id_to_doc_` 只保存存活文档；`Remove` / 覆盖时删除映射，但 `documents_` 记录保留为墓碑。
- `name_arena_` 只追加；`Compact()` 时重建并压缩。

### 2.2 不变量

1. **base 中的文档号一定小于 delta 中的文档号**。
   - `Compact()` 之后 delta 清空；后续 `Add` 分配更大的文档号。
   - 因此 base posting + delta posting 直接拼接就是按文档号升序的完整列表。
2. `documents_` 中 `alive == true` 的文档数量等于 `id_to_doc_.size()`。
3. `base_index_` 只包含最近一次 `Compact()` 时的存活文档；之后新增/覆盖/删除都在 `delta_index_` 和墓碑中体现。
4. posting 列表按文档号升序；同一个 (gram, 文档) 只出现一次。
5. delta 中同一个文档号只会连续追加到单个 gram 的列表末尾，去重只需比较最后一个元素（Debug 下有断言）。

## 3. 字符与 gram

### 3.1 UTF-8 解码

- 严格校验：拒绝 overlong、代理区 `U+D800–U+DFFF`、`> U+10FFFF`。
- 最多保存 `max_string_length + 1` 个码点；超出后只校验不存储，调用方据此判断是否超长。
- 先做字节预算：`input.size() > 4 * max_string_length` 直接判超长，不扫描内容。
- 字节预算内完整解码；非法字节返回 `kInvalidUtf8`，**优先于长度超限**。
- 按码点计算长度，不按字节。

### 3.2 大小写折叠

- 只折叠 ASCII `A-Z` 到 `a-z`。
- CJK、假名及其他码点原样保留。
- 名字和查询使用同一套折叠；arena 中保存折叠后的字节。
- UTF-8 自同步，ASCII 字节不会出现在多字节序列内部，因此字节级折叠和字节级 `find` 都安全。

### 3.3 gram key

| 模式 | unigram key | bigram key |
|---|---|---|
| `BmpOnly == true` | `std::uint32_t` 码点 | `(cp1 << 16) | cp2`，32 位 |
| `BmpOnly == false` | `std::uint32_t` 码点 | `(uint64(cp1) << 32) | cp2`，64 位 |

- `BmpOnly == true` 时，任何 `> 0xFFFF` 的码点返回 `kUnsupportedCharacter`。
- `BmpOnly == false` 支持完整 Unicode；emoji 等补充平面字符也可索引。

## 4. posting 编码

base 的 posting 使用 delta + varint（LEB128）压缩：

- 输入按文档号升序；
- 第一个值直接编码，后续编码 `value - previous`；
- 解码时累加回原文档号；
- 每个 gram 的 posting 在 posting 缓冲区中是一段连续字节，gram 表记录 `offset` 和 `count`；
- 游标遇到截断或过长 varint 时 Debug 断言、Release 返回 false。

delta 的 posting 不做压缩，直接使用 `std::vector<FuzzySearchDocId>`；因为增量数据量小，且需要频繁追加。

## 5. 查询流程

### 5.1 Q = 1

1. 要求 `unigram_enabled_ == true`（即 `min_search_length == 1`）。
2. 取 unigram posting：base 游标 + delta span。
3. 跳过墓碑，调用 visitor，达到 `max_results` 或 visitor 返回 false 停止。

### 5.2 Q = 2

1. 计算 bigram key。
2. 取 bigram posting：base 游标 + delta span。
3. 单个 bigram 即精确结果，不需要原文校验。

### 5.3 3 <= Q <= max_string_length

1. 计算 Q-1 个相邻 bigram key，并统计各列表长度之和。
2. 用一块 `std::vector<FuzzySearchDocId>` 按列表顺序写入 Q-1 个 posting；每个列表的位置记在栈上的 `std::array` 里。
   - 任一列表为空时交集必然为空，直接返回。
3. 选最短的列表作为候选集。
4. 对每个候选：
   - 跳过墓碑；
   - 对其余每个列表做二分查找；
   - 用 `name_arena_` 中的名字做 `find(folded_query)` 连续性校验；
   - 通过则调用 visitor。
5. 达到 `max_results` 或 visitor 返回 false 停止。

为什么需要第 4 步的原文校验：多个 bigram 同时存在不代表它们相邻。例如名字 `abXbc` 同时包含 `ab` 和 `bc`，但查询 `abc` 不应命中。

### 5.4 热路径缓冲

`Add` / `Search` 复用栈上的固定缓冲 `TextScratch`：

- 码点缓冲 `std::array<char32_t, kMaxStringLength + 1>`；
- 折叠字节缓冲 `std::array<char, 4 * kMaxStringLength + 1>`。

单次调用约 2KB 栈空间；调大 `kMaxStringLength` 时需要重新评估线程栈。

## 6. Compact

`Compact()` 的步骤：

1. 没有墓碑且 delta 为空时直接返回 `kOk`；
2. 校验存活文档数未超出 `FuzzySearchDocId` 范围；
3. 遍历 `documents_`，只处理 `alive == true` 的记录；
4. 为存活文档重新分配连续的 `FuzzySearchDocId`；
5. 在局部变量中重建文档表、id 映射与名字 arena；
6. 从折叠后的名字重新解码码点，生成 bigram（以及可选的 unigram）的 `(key, DocId)` 对；
7. 交给 `FuzzySearchBaseIndex::Build`：排序、按 key 分组、去重、delta+varint 编码；
8. 全部成功后才整体替换旧 base，并清空 delta 和墓碑。

注意：

- 同步执行，可能阻塞调用线程；应在低峰触发。
- 临时 gram 数组的预留量按名字字节中实际出现的码点数估算（UTF-8 续字节不计），不再按 `文档数 × max_string_length` 无界预留。
- 内存峰值约为“旧 base + 新 base + 临时 `(key, DocId)` 数组”。
- 名字 arena 超过 `uint32_t` 可表示范围返回 `kNameArenaFull`；文档号超限返回 `kTooManyDocuments`；名字解码失败返回具体解码错误。以上失败都不改动任何索引成员。

## 7. 复杂度

| 操作 | 复杂度 |
|---|---|
| `Add` | O(名字长度 × 常数) + delta 哈希表插入，无堆分配 |
| `Remove` | O(1) |
| `Contains` | O(1) |
| `Search` Q=1/2 | O(posting 列表长度)，可提前停止 |
| `Search` 3+ | O(最短列表长度 × (Q-2) × log S + 候选数 × 名字长度) |
| `Compact` | O(存活文档数 × 平均名字长度 + 总 posting 数) |

## 8. 内存

- 每条文档：`sizeof(DocumentRecord)` + 名字字节。
- 每个 base posting：约 1–3 字节（delta + varint，取决于文档号间隔）。
- 每个 delta posting：4 字节/文档（未压缩）。
- 每个 gram：gram 表项 + delta 哈希节点。
- 未 `Compact` 的墓碑会持续占用 `documents_` 和名字 arena 空间。

以 500 万条名字、平均 6 个码点估算，全服紧凑索引约 300–400MB；按 guild 分片分摊时除以分片数。

## 9. 边界

- 单线程；无锁。
- 进程级单实例：同一进程只允许一个存活实例，第二次 `Create` 返回 `kAlreadyCreated`；实例析构后可重新创建；守卫被所有模板实例化共享；定义 `FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES` 可关闭守卫。
- 无持久化；重启后重新 `Add`。
- 结果顺序不承诺；调用方自行排序。
- 名字不要求唯一。
- 只折叠 ASCII 大小写；不做 Unicode 大小写折叠。
- `max_results` 硬上限 500。
- `BmpOnly == true` 时拒绝 BMP 之外的码点。
- 日志只记录低频路径；`Add` / `Search` 正常路径不打日志。
