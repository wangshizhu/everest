# circular_queue 优化路线图（对齐计划与基准方案）

## 文档信息

- 组件：`everest::CircularQueue<T>`（`everest/multi_producer_multi_consumer/`）
- 相关代码：`everest/multi_producer_multi_consumer/circular_queue.h`、`everest/multi_producer_multi_consumer/src/circular_queue.cpp`
- 相关测试：`test/circular_queue_test/`
- 状态：**讨论中**；O2、O3 与 O4 的 emplace 部分已实现（O3/O4 先于基准落地），其余阶段暂缓执行
- 最后更新：2026-11-28

> 本文只记录演进方向、判断依据和验收口径，不改变当前实现。当前版本优先正确性与契约清晰，性能优化必须在基准数据支持下按阶段推进。

## 1. 启动条件

满足以下任一条件时，再进入对应阶段：

1. 环形队列被放到高频路径（每消息、每帧、每请求）上使用，且 profile 显示其开销可观测。
2. 出现明确的并发队列需求（需要 SPSC/MPMC 无锁语义），而不是继续由调用方加锁包裹。
3. 出现明确的调用方需求（批量入队、`PushBack(const T&)`、按配置选择写满策略、可观测性计数）。
4. 会话/服务线程模型稳定，优化不会因上层模型变化而返工。

不满足条件时保持现状：当前实现的契约清晰性（移动后为空队列、覆盖/拒绝两种写满写法、对齐正确、容量上限校验）优先。

## 2. 现状与对标结论

当前实现的核心结构：

- `CircularQueue<T>` 是 pimpl 外壳：私有 `std::shared_ptr<detail::CircularQueueCore>`，移动即转移 `shared_ptr`。
- `detail::CircularQueueCore` 是非模板实现，位于 `circular_queue.cpp`，通过统一编译单元 `include.cpp` 编入 `everest.lib`。
- 类型擦除：`CircularQueueHooks` 持有移动构造 / 移动赋值 / 析构三个函数指针与 `sizeof(T)`、`alignof(T)`，核心只操作 `void*`；`CircularQueue` 自身不使用 `move_assign`，它由需要「出队到 `T&`」的 `MultiProducerMultiConsumer` 补上。
- 容量语义：`capacity` 为可用元素数；内部槽位取不小于 `capacity` 的最小 2 的幂，用 `size_` 计数区分空/满，环形下标用 `& slot_mask_` 替代取模（2026-11-26 实现）。`Capacity()`、`Size()`、`Full()` 的可观测语义不变。
- 写满策略（2026-11-26 已实现）：
  - `PushBack(T&&)` 覆盖最旧元素并累加 `OverrunCounter()`；
  - `TryPushBack(T&&)` 满时拒绝、实参不移动、返回 `false`，同样累加 `OverrunCounter()`（统一语义：因满而丢失的元素数）。
- 存储：`::operator new(bytes, std::align_val_t{alignof(T)})`，析构时成对释放；支持过对齐类型。
- 构造策略：惰性构造——构造时只分配对齐存储，首次写入时 `placement new`，出队、覆盖或队列析构时调用析构；`T` 必须可移动构造、可析构，由 `static_assert` 校验（不要求默认构造或移动赋值）。
- 线程安全：不做任何同步，调用方负责；现有唯一消费者 `MultiProducerMultiConsumer` 用 `mutex + condition_variable` 包裹。

与大厂/主流公开实现的对比结论：

| 维度 | 当前实现 | 主流同类 |
|---|---|---|
| 并发模型 | 单线程，无内部同步 | folly PCQ、boost `spsc_queue`、rigtorp `SPSCQueue`、Linux `kfifo` 为 SPSC 无锁；moodycamel 为 MPMC 无锁；Disruptor 为序列栅栏 |
| 写满策略 | 覆盖最旧 / 拒绝写入（两方法） | folly/boost/moodycamel 满时失败返回；`boost::circular_buffer` 覆盖最旧；Disruptor 按策略 |
| 构造策略 | 惰性构造（首次写入 placement-new，出队析构） | folly/boost spsc 多为惰性 placement-new；Disruptor 预分配事件 |
| 每操作开销 | 跨 TU 调用 + 函数指针间接调用 + 掩码下标 | header-only 全内联；`kfifo`/Disruptor 用 2 的幂掩码替代取模 |
| 实现可见性 | 控制在 `.cpp`，头文件只留声明与胶水 | 多数为 header-only 模板 |
| 正确性契约 | 空队列 `PopFront` 无操作、容量 0 归一、溢出抛 `std::length_error`、过对齐支持、移动后为空队列 | spdlog 血统的原实现存在 disabled 态、空 `PopFront` 破坏状态等问题 |
| 可观测性 | `OverrunCounter()` | 多数无；需自行统计 |

一句话定位：**正确性与契约清晰度达到或超过同血统实现；每操作开销与并发能力弱于高频路径实现；构造策略已与主流的惰性构造对齐。**

补充事实（用于避免错误对标与无效工作）：

- 本组件旧实现与 spdlog `details/circular_q.h` 同血统；spdlog 原版的空 `PopFront()` 会把 `head_` 推过 `tail_`，`Size()` 反算出假元素。
- `folly::ProducerConsumerQueue` 是 SPSC 无锁固定容量队列，写入在满时返回失败，不支持覆盖最旧。
- `boost::circular_buffer` 是通用容器（迭代器、`resize`、`push_front`），满时 `PushBack` 覆盖最旧，非线程安全。
- `boost::lockfree::spsc_queue` 的 `push`/`pop` 返回 `bool`，固定容量，不做覆盖。
- `Linux kfifo` 是内核 SPSC 字节环形缓冲，容量取 2 的幂，用掩码替代取模，回绕时两段拷贝。
- 公开 benchmark 的绝对数字跨机器、跨编译选项，不能直接引用；只能复现 workload 后同机 A/B。

### 2.1 构建期事实（LTO 现状）

- MSVC `Release|Win32` / `Release|x64` 已在 `windows_sln/erverest.vcxproj` 开启 `WholeProgramOptimization`（`/GL`），测试工程 Release 同样开启，链接阶段实际执行 `/LTCG`（Release 构建日志出现 "正在生成代码"）。
- Debug 配置不开 WPO；Linux 构建默认 `Debug`，根 `CMakeLists.txt` 未启用 `-flto`。
- 结论：`Bridge::PushBack` 这类跨 TU 调用在 MSVC Release 中**可能已被 LTO 内联**；但 `hooks_.move_construct`、`hooks_.destroy` 的函数指针间接调用不会被 LTO 消除；环形下标已改用掩码（O2），不再有运行期除模。
- 若后续给 Linux 开启 LTO，建议只对 Release 生效（`CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`）；本仓库 `build.sh` 默认构建 Debug，改了也不生效。
- `/GL` 产物是 IL 而非机器码，消费方必须用 `/LTCG` 链接；VS 工程勾选 Whole Program Optimization 时自动满足。

## 3. 差距与优化方向

### P0：先建立基准，不急着优化

| ID | 方向 | 现状 | 目标 |
|---|---|---|---|
| B1 | 可移植 benchmark harness | 组件无基准；仓库现有 fuzzy_search benchmark 仅 Windows | Windows + Linux、可重复输出 JSON/表格 |
| B2 | 基线对照 | 无数据 | 当前实现 vs 同语义 header-only 参考实现 vs `boost::circular_buffer`（可选）vs `std::deque` 基线 |

P0 是后续所有优化的前提。没有基准数据，不进入 P1。O2 是唯一例外：按用户要求先行实现，收益仍需基准确认。

### P1：每操作开销

| ID | 方向 | 现状 | 目标 | 代价/风险 |
|---|---|---|---|---|
| O1 | 消除跨 TU 与函数指针间接层 | `Bridge` 非内联；`hooks_` 为运行期函数指针 | header-only inline，或模板化 core；快路径直接数组写 + 索引推进 | 与 pimpl/类型擦除的既定设计冲突；模板化会让每个 TU 重复实例化，编译时间与二进制增大；MSVC Release 已靠 LTO 消除跨 TU 一跳，收益需基准证明 |
| O2 | 取模改掩码 | `% slot_count_` 的除数是运行期值 | **已于 2026-11-26 实现**：槽位取整到 2 的幂 + `size_` 计数，`Capacity()` 仍返回请求值 | 内部槽位最多上浮到 2 倍（请求 1000 分配 1024）；不再是「空槽标记」算法；收益待基准量化 |
| O3 | 惰性构造 | **已实现**：构造时只分配存储，首次写入 `placement-new`，出队/覆盖时析构 | 写入时 placement-new、出队时析构 | 已落地：`T` 契约现为可移动构造 + 可析构；未构造槽位不持有对象 |
| O4 | 批量接口 / `emplace` | `EmplaceBack`/`TryEmplaceBack` 已实现；仍无 `PushBack(const T&)`、无批量接口 | `PushBack(const T&)`、批量入队/出队 | API 面积扩大；仓库内暂无批量接口调用方 |
| O5 | 缓存行填充 | 无 | 仅当实现无锁变体时，为 head/tail 填充避免伪共享 | 单线程无意义；增加内存占用 |

### P2：并发与工程能力

| ID | 方向 | 现状 | 目标 | 代价/风险 |
|---|---|---|---|---|
| O6 | SPSC 无锁变体 | 无 | 独立的 `SpscQueue<T>`：原子读写索引 + acquire/release + 缓存行填充 | 新组件、新测试、ABA/回绕语义；不应塞进现有 `CircularQueue`，避免破坏单线程契约 |
| O7 | MPMC / 阻塞策略 | 由 `MultiProducerMultiConsumer` 加锁实现 | 若确需无锁 MPMC，评估 moodycamel 式实现或直接引入成熟库 | 复杂度高；阻塞策略必须留在包装层，不能进队列 |
| O8 | 可观测性拆分 | `OverrunCounter()` 统一统计两种策略的丢失 | 视需要拆出 `rejection_counter()` 或分策略计数 | API 增长；现有消费者只读总量 |
| O9 | 写满策略枚举化 | 方法级（方案 A）已实现 | 仅当策略需由配置在运行期决定时，增加构造期枚举 | 引入策略字段与分支；覆盖路径被迫做 `Full()` 判断 |

### P3：工程与构建

| ID | 方向 | 现状 | 目标 | 代价/风险 |
|---|---|---|---|---|
| O10 | Linux Release 启用 LTO | 根 CMake 默认 Debug，未启用 `-flto` | 为 Release 开启 `CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`，与 MSVC `/GL + /LTCG` 对齐 | 构建时间与内存上升；Debug 不受影响；静态库产物需确认兼容 |
| O11 | 编译期容量模板参数 | 容量是运行期构造参数 | 新增 `CircularQueue<T, N>`：编译器自动把 `% N` 优化为掩码/乘移位，零运行期取整 | 固定容量、每个 N 一份实例化；应作为新类型与现有运行期版本并存，不改现有类 |
| O12 | 基准工程基建 | 无 | `benchmarks/circular_queue/` 的 `.cpp`、`CMakeLists.txt`、`build.sh`、Windows `.vcxproj` / `.sln`、`README.md`，不依赖平台头 | 与 B1 是同一交付物，按 B1 一并完成，避免重复建设 |

### 明确不推荐的方向

- 为 `CircularQueue` 引入 folly / boost 运行时依赖：收益不足以抵消集成成本。
- 把 `CircularQueue` 直接改造成无锁队列：会破坏「单线程、无内部同步」的既有契约，应新增独立类型。
- 用公开 benchmark 的绝对数字与自己的测量结果直接比较：跨机器、跨编译选项，无意义。
- 在基础设施与线程模型稳定前改造与会话/服务强相关的用法。

## 4. Benchmark 方案（P0 细化）

### 4.1 对照矩阵

| 组 | 说明 |
|---|---|
| `ours_overwrite` | `PushBack` 覆盖最旧路径（稳态 + 满时覆盖） |
| `ours_reject` | `TryPushBack` 拒绝路径 |
| `ours_access` | `Front()` / `At(i)` / `PopFront()` 读路径 |
| `inline_reference` | 同语义 header-only 参考实现（无类型擦除、无跨 TU），量化 pimpl 成本 |
| `deque_baseline` | `std::deque` 满即弹出，作为通用容器基线 |
| `boost_circular_buffer` | 可选；固定版本 Boost 头文件放在 benchmark 外部目录（不进 `3rd/`、不提交） |
| `mutex_wrapped` | 按当前用法（`mutex + condition_variable`）包裹后的端到端成本 |
| `spsc_reference`（后续） | 仅当评估 O6 时，与 SPSC 无锁参考实现对比，不与单线程组混排名 |

### 4.2 Workload

1. 稳态往返：队列保持半满，交替 push/pop。
2. 满时覆盖：容量已满后持续 `PushBack`，统计丢弃与耗时。
3. 满时拒绝：容量已满后持续 `TryPushBack`，统计拒绝与耗时。
4. 随机访问：固定大小元素上反复 `At(i)`。
5. 类型大小扫描：`int`、64 字节结构、`std::unique_ptr<int>`、4KB 结构，量化入队构造、移动与出队析构成本。
6. 生命周期成本：入队构造成本与出队析构成本分别测量；队列构造函数本身不再构造元素。

### 4.3 指标

- ns/op、Mops/s。
- 每操作堆分配次数（benchmark 内覆写全局 `operator new/delete` 计数）。
- 队列常驻内存：`next_pow2(capacity) * sizeof(T)` 与 `shared_ptr` 控制块。
- 编译时间与二进制增量（评估 O1/O2 时记录）。
- 可选：多线程包裹下的锁等待或端到端吞吐。

### 4.4 公平性与可复现性

- 同一个测试类型、同样的对齐、同样的容量。
- Windows Release `/O2 /MT`，Linux `-O2`；记录 CPU、编译器版本、命令行。
- 预热 + 多轮取中位数；使用 `DoNotOptimize` 级手段防止循环被优化掉。
- 容量的选取要覆盖非 2 的幂（如 1000）与 2 的幂（如 1024），用于观察内部槽位向上取整的内存上浮。
- 只做同机 A/B，不做跨机器绝对数字对比。
- 基准程序不依赖 `windows.h` 等平台头，保证 Windows + Linux 都能跑；平台相关计时与输出用条件编译隔离。

### 4.5 交付物（执行 P0 时）

- `benchmarks/circular_queue/circular_queue_benchmark.cpp`
- 对应 `CMakeLists.txt`、`build.sh`、Windows `.vcxproj` / `.sln`、`README.md`
- 控制台表格 + JSON 结果 + 一份记录环境与结论的 Markdown 小结

## 5. 分阶段路线

| 阶段 | 内容 | 启动条件 | 完成标志 |
|---|---|---|---|
| 阶段 0 | B1/B2：基准与基线 | 本文确认且基础设施稳定 | 有可重复的同机对比数据和结论 |
| 阶段 1 | O1：按基准决定是否消除间接层（O2 已实现） | 基准显示每操作开销是主要瓶颈，且队列进入高频路径 | 有量化收益；现有测试全绿 |
| 阶段 2 | O4 剩余接口/O8/O9 | 出现明确的调用方需求 | 新能力 opt-in，默认行为不变 |
| 阶段 3 | O6/O7：SPSC/MPMC 变体 | 出现真实的并发队列需求，且线程模型稳定 | 独立组件与独立测试，不改变现有 `CircularQueue` 契约 |

每个阶段都必须保持：

- `PushBack` 覆盖最旧、`TryPushBack` 拒绝写入（实参不移动）、`OverrunCounter()` 的现有语义，除非另行设计版本化接口。
- 空队列 `PopFront()` 无操作、容量 0 归一、容量溢出抛 `std::length_error`、过对齐支持等现有测试继续通过。
- 不修改 `3rd/` 作为优化的前提。

## 6. 验收标准（通用）

- 现有 `test/circular_queue_test` 全部通过，新增优化有对应测试。
- 优化前后使用同一 benchmark、同一机器、同一编译选项对比。
- 性能收益有数据支撑；没有数据的优化不合并。
- 默认配置下行为不变；新增能力 opt-in。
- 新增第三方依赖必须单独说明来源、版本、许可证和是否提交。
- 头文件仍保持自包含；新增实现不得把存储细节泄漏回头文件（除非按 O1 决策整体改为 header-only）。

## 7. 不在本路线图范围

- 把 `CircularQueue` 改成无锁或可拷贝类型。
- 改变「惰性构造 + `T` 需可移动构造与可析构」的现有契约。
- 引入 folly / boost / moodycamel 作为运行时强依赖。
- 用跨机器公开数字替代同机测量。
- 在基础设施未稳定前改造线程模型或 service 生命周期。

## 8. 风险与注意事项

- **过早优化**：当前单线程 + 外部锁的实现对现有使用频率完全够用；先测再改。
- **pimpl 与性能冲突**：O1 会推翻当前的实现隐藏方式，收益要靠数据证明；MSVC Release 已有 LTO，实际差距可能小于预期。
- **取模改掩码的取舍**：采用「槽位取整到 2 的幂 + `size_` 计数」，保持 `Capacity()` 语义不变；风险转为内部槽位最多上浮到 2 倍内存。
- **惰性构造契约已落地**：`T` 现要求可移动构造与可析构（不要求默认构造/移动赋值）；异常路径、前置条件与移动后状态已由 `test/circular_queue_test` 覆盖。
- **无锁变体是新组件**：不要为了「名字里带 multi_producer_multi_consumer」就把并发能力塞进这个类。
- **基准噪声**：CPU 频率、编译器版本、静态库链接方式都会影响结果；报告必须带环境信息并重复验证。

## 9. 待讨论问题

1. 基准是否一开始就做 Windows + Linux 双平台，还是先在 Windows 上定口径？
2. 是否存在真实的高频路径需求？若没有，是否只保留本文档而不启动阶段 0？
3. O2 已按保持 `Capacity()` 语义的方式实现；是否接受内部槽位向上取整带来的内存上浮（如请求 1000 实际分配 1024 槽）？
4. O3（惰性构造）已实现，不再作为待讨论项；原问题保留作决策记录（`T` 契约现为可移动构造 + 可析构）。
5. `OverrunCounter()` 是否要拆成「覆盖丢旧」和「拒绝新」两个计数？
6. 基准中是否引入 `boost::circular_buffer` 作为对照（需外部头文件，不进仓库）？

## 10. 待办清单

> 勾选条件：代码完成、对应测试通过、有基准数据或文档支撑。状态随实现更新。

### 已完成

- [x] **C1 写满策略方法化**：`PushBack` 覆盖最旧 + `TryPushBack` 满时拒绝（实参不移动、可重试）。
  - 验收：现有用例全绿；新增拒绝路径用例；`MultiProducerMultiConsumer` 无需改动。
- [x] **O2 取模改掩码**：内部槽位取整到 2 的幂 + `size_` 计数，环形下标用掩码。
  - 验收：`Capacity()` / `Size()` / `Full()` 语义不变；非 2 的幂容量用例通过；Windows Debug/Release + Linux 全绿。
- [x] **O3 惰性构造**：构造时只分配对齐存储，首次写入 `placement-new`，出队/覆盖时析构。
  - 验收：`T` 契约改为可移动构造 + 可析构；`NoDefaultCtorPayload` 等用例通过。
- [x] **O4（emplace 部分）**：`EmplaceBack()` / `TryEmplaceBack()` 在槽位上原地构造，无额外移动。
  - 剩余：`PushBack(const T&)` 与批量接口仍待真实调用方。

### 阶段 0：基准（P0）

- [ ] **B1 + O12 基准 harness 与工程基建**
  - 交付：`benchmarks/circular_queue/` 下的 `.cpp`、`CMakeLists.txt`、`build.sh`、Windows `.vcxproj` / `.sln`、`README.md`；不依赖 `windows.h` 等平台头。
  - 验收：一条命令完成构建与运行；Windows + Linux 均可跑；输出控制台表格 + JSON。
- [ ] **B2 基线对照**
  - 交付：`ours_overwrite` / `ours_reject` / `ours_access` / `inline_reference` / `deque_baseline` / `mutex_wrapped`；可选 `boost_circular_buffer`。
  - 验收：同机、同编译选项、同容量 A/B 数据；报告记录 CPU、编译器版本、命令行；容量覆盖非 2 的幂（如 1000）与 2 的幂（如 1024）。

### 阶段 1：热路径（按基准数据决定）

- [ ] **O1 消除跨 TU 与函数指针间接层**
  - 前置：B2 显示每次操作开销是主要瓶颈。
  - 验收：目标 workload 下每操作耗时明显下降；现有测试全绿；若数据不支持则关闭该项并记录原因。
- [ ] **O4 剩余接口（`PushBack(const T&)` / 批量）**
  - 前置：至少一个真实调用方。
  - 验收：新接口有测试；默认行为不变。
- [ ] **O5 缓存行填充**
  - 前置：O6 启动。
  - 验收：多线程 SPSC 场景无伪共享；单线程版本不受影响。

### 阶段 2：能力与并发

- [ ] **O6 SPSC 无锁变体**
  - 交付：独立的 `SpscQueue<T>`（新类型，不改 `CircularQueue`）。
  - 验收：SPSC 压力测试通过；现有 `CircularQueue` 契约与测试不变。
- [ ] **O7 MPMC / 阻塞策略**
  - 前置：出现真实并发队列需求且线程模型稳定。
  - 验收：阻塞语义留在包装层；吞吐/延迟有数据。
- [ ] **O8 计数拆分**
  - 前置：消费者需要区分「覆盖丢旧」和「拒绝新」。
  - 验收：默认 API 不变；新计数有测试。
- [ ] **O9 写满策略枚举化**
  - 前置：策略来自配置而非调用点。
  - 验收：覆盖路径无新增分支，或数据证明分支成本可接受。

### 工程与构建

- [ ] **O10 Linux Release 启用 LTO**
  - 做法：`CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`，与 MSVC `/GL + /LTCG` 对齐。
  - 验收：Linux Release 构建通过；基准不退化；Debug 构建不受影响。
- [ ] **O11 编译期容量模板参数**
  - 做法：新增 `CircularQueue<T, N>`（固定容量），不改现有运行期容量类型；编译器自动消除取模/取整。
  - 验收：与运行期版本行为一致；测试对所有使用的 N 有实例化。

### 保持现状（不做）

- 不引入 folly / boost / moodycamel 作为运行时依赖。
- 不把 `CircularQueue` 改成无锁或可拷贝。
- 不用跨机器公开数字替代同机测量。
- 不在基础设施稳定前改线程模型或 service 生命周期。

## 附录：参考资料

- folly `ProducerConsumerQueue`：<https://github.com/facebook/folly/blob/main/folly/ProducerConsumerQueue.h>
- `boost::circular_buffer`：<https://www.boost.org/doc/libs/release/doc/html/circular_buffer.html>
- `boost::lockfree::spsc_queue`：<https://www.boost.org/doc/libs/release/doc/html/boost/lockfree/spsc_queue.html>
- rigtorp `SPSCQueue`：<https://github.com/rigtorp/SPSCQueue>
- moodycamel `ConcurrentQueue`：<https://github.com/cameron314/concurrentqueue>
- Linux `kfifo`：<https://www.kernel.org/doc/html/latest/core-api/kernel-api.html#fifo-buffer>
- LMAX Disruptor：<https://lmax-exchange.github.io/disruptor/>
- 现有实现与计划：`doc/code-plan/circular_queue_plan.md`、`everest/multi_producer_multi_consumer/circular_queue.h`
