# object_pool 优化路线图

## 文档信息

- 组件：`everest::ObjectPool<T>`（`everest/object_pool/`）
- 相关代码：`everest/object_pool/object_pool.h`、`everest/object_pool/src/object_pool.cpp`
- 相关测试：`test/object_pool_test/`
- 状态：**待办，暂缓执行**；待整体基础设施完善后再启动
- 最后更新：2026-11-25

> 本文只记录演进方向和验收口径，不改变当前实现。当前版本优先正确性与生命周期安全，性能优化必须在基准数据支持下按阶段推进。

## 1. 启动条件

满足以下任一条件时，再进入对应阶段：

1. 对象池被放到高频路径（每消息、每帧、每请求）上使用。
2. Profile 显示对象池的锁竞争或分配调用占比明显（例如热路径中 `mutex` 等待或 `new/delete` 占比超过可接受阈值）。
3. 会话/服务线程模型稳定，能够明确对象池的分片边界（按线程、按 service、按连接）。
4. 基础设施（线程池、service 生命周期、配置系统）完善，优化不会因上层模型变化而返工。

不满足条件时保持现状：当前实现的安全性（池先于租约销毁、拒绝非法归还）和可控性（`max_free`、`Shrink()`）优先。

## 2. 现状与对标结论

当前实现的核心结构：

- `detail::ObjectPoolCore`：`std::mutex` + `expand_step_` + `max_free_` + `ObjectPoolHooks` + `std::vector<void*> free_` + `std::unordered_set<void*> used_`。
- 首次申请或空闲耗尽时，锁外批量 `new T()`，锁内并入空闲链并取走一个对象。
- `Clear()` 在锁外调用；超过 `max_free_` 的归还在锁外析构。
- 租约 `SharedObject` / `UniqueObject` 的 `Deleter` 持有 `std::shared_ptr<ObjectPoolCore>`。

与大厂公开实现的对比结论：

| 维度 | 当前实现 | 大厂同类 |
|---|---|---|
| 生命周期安全 | 池可先于租约销毁；拒绝非法归还 | Boost/folly/XLA 默认要求池先于对象存活，误用即 UB |
| 空闲上限 | `max_free` + `Shrink()` | `boost::object_pool` 无；`IndexedMemPool` 固定容量；XLA 无收缩 |
| 线程模型 | 单 `mutex` | folly 分片无锁、Google Arena per-thread、tcmalloc 线程缓存 |
| 存储局部性 | 每对象一次 `new T()`，指针容器 | Boost chunk、Arena bump、`IndexedMemPool` 大块 mmap |
| 每租约开销 | shared 控制块 + deleter 内 `shared_ptr<Core>` | 索引/侵入式句柄或裸指针 |
| 类型契约 | 默认构造 + `Clear()` | Boost placement new；XLA `optional<T>` + builder |
| 可观测性/可注入 | 无统计、硬编码 `new/delete` | 多数提供容量/计数或 allocator 政策 |

一句话定位：**正确性和可控性达到或超过大厂公开组件，吞吐扩展性和内存局部性目前弱于大厂高频路径实现。**

补充事实（用于避免错误对标）：

- Boost 提供 `boost::object_pool`，但**非线程安全**，chunk + placement new，`object_pool` 没有 `release_memory`；`singleton_pool` 才带同步与 `release_memory/purge_memory`。
- folly 当前主线**没有**通用 `ObjectPool`；相关设施是 `IndexedMemPool`（索引化无锁节点池）、`ThreadCachedArena`、`Arena`。
- Abseil **没有**公共对象池 API。
- Google 的对象池实践主要是 protobuf `Arena`（区域/批量分配）与 tcmalloc（线程缓存 + 中央空闲链）；XLA runtime 的 `ObjectPool` 是独立实现，不属于 Abseil。
- 网上的公开 object pool benchmark 多为零散、跨机器、口径不一致的对比，**不能直接引用绝对数字**；只能复现其 workload 后同机 A/B。

## 3. 差距与优化方向

### P0：先建立基准，不急着优化

| ID | 方向 | 现状 | 目标 |
|---|---|---|---|
| B1 | 可移植 benchmark harness | 组件无基准；仓库现有 fuzzy_search benchmark 仅 Windows | 单线程/多线程、Windows + Linux、可重复输出 JSON/表格 |
| B2 | 基线对照 | 无数据 | 我们的池 vs `new/delete` vs `std::pmr` 池 vs 本地 slab 参考实现 |

P0 是后续所有优化的前提。没有基准数据，不进入 P1。

### P1：性能与扩展性

| ID | 方向 | 现状 | 目标 | 代价/风险 |
|---|---|---|---|---|
| O1 | slab/block 分配 | 每个对象一次 `new T()`；`free_` 为 `vector<void*>` | 一次分配一块连续存储，块内 placement new；空闲链存块内索引或指针 | 需要管理块生命周期；`Clear()` 契约保持不变；与逐对象析构逻辑兼容 |
| O2 | 分片/线程本地池 | 所有线程争用同一个 `mutex` | 按线程/service 分片；或线程本地空闲链 + 全局批量回收 | 分片边界依赖上层线程模型；跨线程归还语义需明确 |
| O3 | 快路径免锁 | `Apply/Release` 全程持锁 | 单线程/分片场景下快路径无锁或仅一次 CAS | 需要 ABA 处理；错误路径复杂化 |

### P2：接口与工程能力

| ID | 方向 | 现状 | 目标 | 代价/风险 |
|---|---|---|---|---|
| O4 | 侵入式/索引句柄 | `SharedObject` 每次分配控制块；`Deleter` 再持 `shared_ptr<Core>` | 句柄内嵌于槽位，或使用 32 位索引 + tag | 改变租约实现；需要兼容期和新旧 API 共存方案 |
| O5 | allocator 政策 | 硬编码 `new/delete` | 可注入分配器（tcmalloc、预分配内存、Arena 适配） | 模板参数会改变类型名；需要默认行为不变 |
| O6 | 统计与诊断 | 无统计 | 可选暴露 `created/free/in_use/contention` | 避免热路径开销；默认关闭 |
| O7 | 代际句柄 | 裸指针租约，`.get()` 可能悬垂 | 带 generation 的 handle，debug 下检测陈旧句柄 | 改动 API；仅在确有跨线程/长生命周期句柄需求时做 |
| O8 | Arena/生命周期分流 | 只有复用池一种模型 | 请求级对象提供 Arena 式批量分配 | 属于新能力，不是对象池内部优化 |

### 明确不推荐的方向

- 直接引入 folly `IndexedMemPool`：依赖整套 folly，收益不足以抵消集成成本。
- 本地重写 XLA `ObjectPool` 作为对照：结果不能代表原库，容易得出错误结论。
- 用公开 benchmark 的绝对数字与自己的测量结果直接比较：跨机器、跨编译选项，无意义。

## 4. Benchmark 方案（P0 细化）

### 4.1 对照矩阵

| 组 | 说明 |
|---|---|
| `ours_unique` | `ApplyUniqueObject`，推荐热路径用法 |
| `ours_shared` | `ApplySharedObject`，量化控制块与额外原子计数成本 |
| `new_delete` | 每次 `new T` / `delete` 基线 |
| `pmr_unsync` | `std::pmr::unsynchronized_pool_resource` + placement new |
| `pmr_sync` | `std::pmr::synchronized_pool_resource`，对比锁成本 |
| `slab_local` | 本地参考实现：预分配数组 + 空闲索引栈（单线程无锁） |
| `ours_sharded` | 每线程一个 `ObjectPool`，量化全局 `mutex` 代价 |
| `boost_object_pool` | 可选；需把固定版本 Boost 头文件放到 benchmark 外部目录（不进 `3rd/`、不提交） |
| `protobuf_arena` | 可选；不同生命周期模型，仅作参考，不与复用池混排名 |

### 4.2 Workload

1. 单线程稳态：取一个、立即归还（LIFO）。
2. Burst churn：同时持有 K 个，再全部归还，重复 M 轮。
3. 多线程竞争：1/2/4/8 线程共享同一个池。
4. 分片：每线程独立池，与 3 对比。
5. 峰值后空闲：记录保留内存，调用 `Shrink()`/`release()` 后再记录。

两种复位语义分开测，避免把“重建对象”成本算到对方头上：

- `Clear()` 复用（当前契约）。
- `destroy + placement new`（Boost/pmr 的做法）。

### 4.3 指标

- ns/op、Mops/s。
- 堆分配次数（benchmark 内覆写全局 `operator new/delete` 计数）。
- 峰值 RSS / 保留内存。
- 结束时空闲对象数与在用对象数。
- 可选：多线程下的锁等待时间或竞争次数。

### 4.4 公平性与可复现性

- 同一个测试类型 `T`、同样的对齐、同样的编译选项。
- Windows Release `/O2 /MT`，Linux `-O2`；记录 CPU、编译器版本、命令行。
- 预热 + 多轮取中位数；使用 `DoNotOptimize` 防止循环被优化掉。
- 线程绑定可选；报告中注明是否绑定。
- 只做同机 A/B，不做跨机器绝对数字对比。

### 4.5 交付物（执行 P0 时）

- `everest/object_pool/benchmarks/object_pool_benchmark.cpp`
- 对应 `CMakeLists.txt`、`build.sh`、Windows `.vcxproj` / `.sln`
- 控制台表格 + JSON 结果 + 一份记录环境与结论的 Markdown 小结

## 5. 分阶段路线

| 阶段 | 内容 | 启动条件 | 完成标志 |
|---|---|---|---|
| 阶段 0 | B1/B2：基准与基线 | 本文确认后 | 有可重复的同机对比数据和结论 |
| 阶段 1 | O1：slab 分配 | 基准显示分配调用/局部性是主要瓶颈，且对象池将进入高频路径 | 稳态无逐对象堆分配；吞吐提升达到基线设定的目标；现有测试全绿 |
| 阶段 2 | O2/O3：分片与免锁快路径 | 基准显示多线程受同一 `mutex` 限制 | 4/8 线程扩展性明显改善；跨线程归还语义与测试补齐 |
| 阶段 3 | O4/O5/O6 | 有明确的热路径租约开销或可观测性需求 | 新接口与旧接口共存，默认行为不变；统计可关闭 |
| 阶段 4 | O7/O8 | 出现跨线程/长生命周期句柄或请求级批量生命周期需求 | 新能力有独立文档与迁移说明 |

每个阶段都必须保持：

- `ApplySharedObject` / `ApplyUniqueObject` / `Shrink()` 的现有语义，除非另行设计版本化接口。
- 池先于租约销毁安全、非法归还拒绝等现有测试继续通过。
- 不修改 `3rd/` 作为优化的前提。

## 6. 验收标准（通用）

- 现有 `test/object_pool_test` 全部通过，且新增优化有对应测试。
- 优化前后使用同一 benchmark、同一机器、同一编译选项对比。
- 性能收益有数据支撑；没有数据的优化不合并。
- 默认配置下行为不变；新增能力 opt-in。
- 新增第三方依赖必须单独说明来源、版本、许可证和是否提交。

## 7. 不在本路线图范围

- 改变 `T` 必须提供 `Clear()` 的既有契约（如改为析构 + placement new），除非作为新组件版本单独评估。
- 引入 folly / Abseil / tcmalloc 作为运行时强依赖。
- 用跨机器公开数字替代同机测量。
- 在基础设施未稳定前改造线程模型或 service 生命周期。

## 8. 风险与注意事项

- **过早优化**：当前单锁实现对本仓库现有使用频率可能完全够用；先测再改。
- **分片改变内存行为**：每线程一个池会放大峰值内存；`max_free`/`Shrink()` 语义需要重新定义。
- **slab 与 `Clear()` 的交互**：块内对象仍需逐对象复位；块释放前必须确保没有租约存活。
- **句柄改动影响面大**：侵入式/索引句柄会改变租约类型和调用方代码，需保留兼容期。
- **allocator 注入改变类型**：模板参数会改变 `ObjectPool<T>` 的类型名，需评估对现有代码和 ABI 的影响。
- **基准噪声**：CPU 频率、线程调度、分配器状态都会影响结果；报告必须带环境信息并重复验证。

## 附录：参考资料

- Boost.Pool 接口文档：<https://www.boost.org/doc/libs/1_87_0/libs/pool/doc/html/boost_pool/pool/interfaces.html>
- `boost::object_pool` 线程安全讨论：<https://stackoverflow.com/questions/1663284/is-boostobject-pool-synchronized>
- folly `IndexedMemPool`：<https://github.com/facebook/folly/blob/main/folly/IndexedMemPool.h>
- XLA runtime `ObjectPool`：<https://github.com/openxla/xla/blob/main/xla/runtime/object_pool.h>
- protobuf Arena vs tcmalloc：<https://github.com/protocolbuffers/protobuf/issues/4327>
- 现有实现与设计文档：`doc/code-plan/object_pool_plan.md`、`everest/object_pool/object_pool.h`
