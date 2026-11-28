# multi_producer_multi_consumer 基准计划

## 文档信息

- 组件：`everest::MultiProducerMultiConsumer<T>`（`everest/multi_producer_multi_consumer/`）
- 相关代码：`multi_producer_multi_consumer.h`、`src/multi_producer_multi_consumer.cpp`、`circular_queue.h`、`src/circular_queue.cpp`
- 相关测试：`test/multi_producer_multi_consumer_test/`、`test/multi_producer_multi_consumer_lock_free_test/`
- 对标结论来源：本目录外的组件对比（默认模式 = 单互斥量 + 双条件变量；无锁模式 = `moodycamel::BlockingConcurrentQueue`）
- 状态：**讨论中**；口径已确认（Windows + WSL 双平台、按逻辑核扫描），基准工程实现暂缓，待另行指示
- 最后更新：2026-11-28

> 本计划的目的不是「证明要优化」，而是**先建立可复现的数据**，再决定是否值得改动内部结构。没有数据支撑的优化不进入实现阶段。

## 1. 目标与非目标

### 1.1 目标

1. 量化默认模式的每操作成本，并拆分出包装层（pimpl/Bridge/hooks/环形队列）与同步层（mutex + 2 cv）各自的占比。
2. 量化无锁模式相对默认模式的吞吐与尾延迟差异，并明确两者的语义差异（有界/无界、阻塞/不阻塞）对数据的解释边界。
3. 量化关闭（`Close`）、阻塞唤醒、超时精度这些**语义路径**的成本，为关闭语义的补强（区分「超时」与「已关闭」）提供依据。
4. 量化大 `T`（1KB、4KB）在临界区内移动/构造的代价，判断是否需要「锁内换槽、锁外移动」。
5. 建立同机 A/B 的对照口径，避免引用跨机器、跨编译选项的公开数字。

### 1.2 非目标

- 不做 `CircularQueue` 的单线程微基准（已由 `doc/optimization/circular_queue/optimization_roadmap.md` 覆盖）。
- 不做网络、会话、服务生命周期相关基准。
- 不把基准纳入 CI 或自动测试；与 `benchmarks/fuzzy_search/` 一致，手动构建运行。
- 不比较不同语义实现之间的绝对排名（有界 vs 无界、阻塞 vs 非阻塞必须分表或加注）。
- 不以本计划的结果直接触发无锁队列重写；无锁算法评估见第 9 节判定规则。

## 2. 被测对象与对照组

所有组使用**同一 `T`、同一容量、同一线程配置、同一构建配置**。语义不同的组单独标注，不跨语义排名。

| 组 | 实现 | 有界 | 阻塞入队 | 超时出队 | 覆盖最旧 | 关闭 | 用途 |
|---|---|---|---|---|---|---|---|
| `ours_default` | `MultiProducerMultiConsumer<T>`（默认模式） | 是 | 是 | 是 | 否 | 是 | 主对象 |
| `ours_default_nowait` | 同上，`EnqueueNoWait` | 是 | 否 | 是 | 是 | 是 | 覆盖路径成本 |
| `ours_default_try` | 同上，`TryEnqueue` | 是 | 否 | 是 | 否 | 是 | 拒绝路径成本 |
| `ours_default_emplace` | 同上，`Emplace` | 是 | 是 | 是 | 否 | 是 | 原地构造收益 |
| `ours_lockfree` | 同组件，定义 `USE_LOCK_FREE_QUEUE` | 否（提示） | 否 | 是 | 否 | 部分 | 无锁路径 |
| `circular_only` | `CircularQueue<T>` 裸用（单线程） | 是 | 否 | 否 | 是 | 否 | 环形队列底价 |
| `mutex_deque_ref` | 基准内 header-only：`std::deque` + `std::mutex` + 2 cv，同语义子集 | 是 | 是 | 是 | 否 | 是 | 隔离包装层开销 |
| `moodycamel_ref` | `moodycamel::ConcurrentQueue<T>` 非阻塞 | 否 | 否 | 否 | 否 | 否 | 无锁快路径底价 |
| `folly_mPMCQ`（可选） | `folly::MPMCQueue<T>` | 是 | 否 | 否 | 否 | 否 | 外部对照 |
| `tbb_bounded`（可选） | `oneapi::tbb::concurrent_bounded_queue<T>` | 是 | 是 | 否 | 否 | `abort` | 外部对照 |
| `boost_lfq`（可选） | `boost::lockfree::queue<T, capacity<N>>` | 是 | 否 | 否 | 否 | 否 | 外部对照 |
| `boost_cb`（可选） | `boost::circular_buffer<T>`（外部加锁） | 是 | 外部实现 | 外部实现 | 是 | 否 | 容器形态对照 |
| `spsc_ref`（可选） | `folly::ProducerConsumerQueue<T>` / `boost::lockfree::spsc_queue<T,N>` | 是 | 否 | 否 | 否 | 否 | 仅 1P1C 单表 |

约定：

- `folly` / `oneTBB` / `boost` 属**可选外部对照**：头文件不进入 `3rd/`、不提交，配置期探测，未安装则该组不参与。不得为此引入运行时强依赖。
- `moodycamel_ref` 已在 `3rd/`，默认启用。
- `multi_producer_multi_consumer.h` 的模式是编译期宏切换，因此 `ours_default` 与 `ours_lockfree` 由同一份源码构建为**两个可执行文件**，不放在同一进程内。
- 对照组必须在结果中标注语义差异；统计表按「有界阻塞」「有界非阻塞」「无界」分组。

## 3. 场景矩阵

| ID | 场景 | 线程配置 | 关注指标 | 说明 |
|---|---|---|---|---|
| S1 | 单线程稳态交替 | 1 线程，push/pop 交替，容量 2 或 4 | ns/op、分配次数 | 无竞争底价；`circular_only` 同场景 |
| S2 | 1P1C 双线程 | 1 生产者 + 1 消费者，容量 1024 | ops/s、p50/p99 | 最常见形态 |
| S3 | 多生产者多消费者 | P=C，总线程数 2/4/8…扫描到逻辑核数；最大档加 3P1C、1P3C 各一组 | ops/s、p99、扩展比 | 锁护送与扩展性主指标；需记录 SMT 拓扑 |
| S4 | 满时覆盖 | 1P1C，容量已满，持续 `EnqueueNoWait` | ops/s、overrun 计数 | 仅 `ours_default_nowait`、`boost_cb` |
| S5 | 满时拒绝 | 1P1C，容量已满，持续 `TryEnqueue` | ops/s、拒绝率 | 仅拒绝语义可比的组 |
| S6 | 满时阻塞唤醒 | 容量 1，生产者阻塞，消费者周期性取走 | 唤醒延迟 p50/p99、ops/s | 只测默认模式与 `tbb_bounded` |
| S7 | 超时精度 | 空队列，`DequeueFor(1ms/10ms/100ms)` | 实际耗时 − 请求耗时 | 默认模式与无锁模式分表 |
| S8 | 关闭唤醒 | N 个生产者/消费者阻塞后调用 `Close` | 唤醒延迟 p50/p99 | 默认模式；无锁模式记录「不唤醒，等超时」 |
| S9 | 元素大小扫描 | S2/S3，`T` = 4B/64B/256B/1KB/4KB/`unique_ptr` | ns/op、p99 | 观察临界区内移动成本 |
| S10 | 构造/析构成本 | 单线程，只构造/只析构 | ns/op | 避免把预构造成本算进队路径 |
| S11 | 内存占用 | 构造容量 1000 与 1024 | 进程 RSS、队列估算字节 | 观察 2 的幂上浮与共享控制块 |
| S12 | 每操作分配 | S2/S4，覆写 `operator new/delete` 计数 | 次/操作 | 默认模式应为 0 |
| S13 | 公平性观察 | 4C，统计各消费者取走数量；4P，统计阻塞时长分布 | max/min 比、直方图 | 只描述，不做硬性断言 |
| S14 | 批量（参考） | 只跑 `moodycamel_ref` 的 `enqueue_bulk`/`try_dequeue_bulk` | ops/s | 量化本组件缺批量接口的差距，不要求本组件达标 |
| S15 | 无界背压缺失（语义） | 生产者持续快于消费者，运行固定时长 | 队列估算长度、RSS 增长 | 只跑 `ours_lockfree`，作为语义说明而非性能排名 |

优先级：**S1、S2、S3 为第一阶段必做**；S6、S7、S8 为第二阶段必做；其余按需。

## 4. 指标与口径

- 吞吐：`ops/s = 成功操作数 / 稳态窗口时长`，窗口由所有工作线程就绪后统一开始、固定时长后统一停止。
- 每操作成本：`ns/op = 窗口时长 / 成功操作数`；S1/S2 单线程场景可换算。
- 延迟：只对**阻塞与唤醒**场景采样本，`p50/p99/p999`；吞吐场景不逐操作打点，避免计时器开销污染。
- 超时误差：`实际返回耗时 − 请求时长`，正负分别统计。
- 唤醒延迟：从释放方操作（入队/出队/`Close`）到被唤醒线程继续执行的时长，使用 `steady_clock` 双端打点。
- 分配次数：窗口内 `operator new` 与 `operator new[]` 的调用次数 / 成功操作数。
- 内存：进程 RSS 增量 + 按容量与 `sizeof(T)` 计算的队列常驻字节。
- 公平性：各消费者成功计数、各生产者阻塞时长，只报告分布，不设通过线。
- 所有数值必须带：机器、CPU、核数、OS、编译器版本、构建配置、线程亲和性是否生效、组名与语义标签。

## 5. 测量方法

1. **构建配置**：只跑 Release（Windows `/O2 /MT`，Linux `-O2`）。Debug 数据单独记录，不参与结论。
2. **进程隔离**：默认模式与无锁模式各自独立进程；可选外部组各自独立进程；不同 `T` 大小分进程运行，避免分配器与 CPU 状态污染。
3. **预热**：先运行窗口的 20%（至少 100ms）不计入结果；随后连续跑 `R≥5` 轮，取中位数并报告最大/最小值。
4. **防优化**：所有被测量结果通过 `DoNotOptimize`（编译期选择 `asm volatile` 或 `_ReadWriteBarrier`）保留；禁止把计算结果只写局部变量后结束。
5. **计时代价**：吞吐场景只在窗口首尾取时间；延迟场景每 64 次采样 1 次，或先测空循环计时器底价并在报告中扣除。
6. **线程亲和**：Windows 用 `SetThreadAffinityMask`，Linux 用 `pthread_setaffinity_np`，均以 `#ifdef` 隔离；绑定到互不相同的逻辑 CPU，失败则记录「未生效」，不得静默。SMT 机器需记录逻辑核到物理核的映射（Windows `GetLogicalProcessorInformation`，Linux `/sys/devices/system/cpu/cpuN/topology/thread_siblings_list`）。
7. **停止协议**：统一 `std::atomic<bool> stop` + 有界超时出队（≤1ms）轮询，生产者/消费者 join 前先 `stop.store(true)` 再 `Close()`；禁止无限阻塞入队导致挂起。阻塞场景单独用握手信号控制，不用 `sleep` 近似。
8. **容量选择**：同时覆盖非 2 的幂（1000）与 2 的幂（1024），以观察 `CircularQueue` 内部槽位上浮对内存的影响。
9. **数据排除**：构造、析构、预热、`Close` 收尾均不计入稳态吞吐；S10 单独测生命周期成本。
10. **重复与噪声**：同机 A/B；固定 CPU 频率或记录频率策略；报告至少 5 轮；跨轮离散度超过 20% 的场景标记为不可信。

## 6. 程序结构、CLI 与输出

### 6.1 目录与目标

```
benchmarks/multi_producer_multi_consumer/
├── multi_producer_multi_consumer_benchmark.cpp   # 唯一源码，两种模式共用
├── CMakeLists.txt                                # 生成 default / lockfree 两个目标
├── build.sh                                      # LF：rm -rf build; cmake ..; make
├── README.md                                     # 构建、运行、参数、结果解读
├── results/                                      # 历史 JSON 快照（不随代码更新）
└── multi_producer_multi_consumer_benchmark.vcxproj / .sln   # Windows，两个配置/目标
```

- 同一份 `.cpp` 构建两个可执行文件：`bench_default`（未定义宏）与 `bench_lockfree`（定义 `USE_LOCK_FREE_QUEUE`）。
- 不依赖 `LauncherBase`、`g_control_monitor`、日志初始化；只链接 `everest.lib` 与 protobuf 静态库（统一编译单元需要）。
- 平台相关代码（亲和性、RSS 读取、`DoNotOptimize`）集中在 `#ifdef` 区，其余保持可移植；不包含 `windows.h` 之外的平台头，Linux 可编译。

### 6.2 CLI

| 参数 | 默认 | 说明 |
|---|---|---|
| `--scenario=S1..S15` | `S2` | 场景 |
| `--producers=N` | `1` | 生产者线程数 |
| `--consumers=N` | `1` | 消费者线程数 |
| `--capacity=N` | `1024` | 队列容量/容量提示 |
| `--item-bytes=N` | `64` | 载荷大小；`--item=unique_ptr` 可切换只移动类型 |
| `--duration-ms=N` | `1000` | 稳态窗口 |
| `--warmup-ms=N` | `200` | 预热窗口 |
| `--repetitions=N` | `5` | 轮数 |
| `--pin=0/1` | `0` | 是否绑定线程 |
| `--output=path.json` | — | 结果 JSON |
| `--list` | — | 打印场景列表 |

### 6.3 输出

控制台：按组打印表格（组、场景、ops/s、ns/op、p50、p99、分配/操作、RSS、备注）。

JSON：

```json
{
  "meta": {
    "git_commit": "3700b37",
    "timestamp": "2026-11-28T10:00:00+08:00",
    "os": "Windows 11 10.0.26200",
    "cpu": "...",
    "physical_cores": 8,
    "compiler": "MSVC 19.38",
    "build": "Release",
    "pinned": false,
    "mode": "default"
  },
  "case": {
    "scenario": "S3_mpmc",
    "mode": "default",
    "capacity": 1024,
    "item_bytes": 64,
    "producers": 4,
    "consumers": 4,
    "duration_ms": 1000,
    "warmup_ms": 200,
    "repetitions": 5
  },
  "results": [
    { "repeat": 1, "ops_per_sec": 0, "p50_ns": 0, "p99_ns": 0, "allocations_per_op": 0, "rss_kb": 0 }
  ],
  "summary": { "ops_per_sec_median": 0, "ops_per_sec_min": 0, "ops_per_sec_max": 0, "p99_ns_median": 0 }
}
```

## 7. 交付物

| 阶段 | 交付物 |
|---|---|
| P1 | `README.md`、`CMakeLists.txt`、`build.sh`、`.vcxproj`/`.sln`、基准 `.cpp`（S1/S2）、S1/S2 结果 JSON 与小结 |
| P2 | S3–S5 场景与结果；扩展比表 |
| P3 | S6–S8 场景与结果；关闭/超时/唤醒小结 |
| P4 | S9–S15 场景与结果；大 `T`、分配、内存、批量差距表 |
| P5 | `benchmarks/multi_producer_multi_consumer/results/` 历史快照 + 一份带环境与结论的 Markdown 报告 |

## 8. 分阶段执行与验收

| 阶段 | 内容 | 验收 |
|---|---|---|
| P0 | 本文确认 | 口径、对照矩阵、判定规则无异议 |
| P1 | 工程基建 + S1/S2 | 一条命令构建并运行；Windows Release 与 WSL Ubuntu-24.04 均可跑；输出表格 + JSON |
| P2 | S3–S5 | 同机同配置 A/B；扩展比数据；语义标签齐全 |
| P3 | S6–S8 | 唤醒延迟与超时误差可复现；无锁模式的「不唤醒」被记录为已知语义 |
| P4 | S9–S15 | 大 `T` 与分配数据完整；无界背压缺失有量化说明 |
| P5 | 报告 | 结论只依赖同机数据；每条结论附组名与场景 ID |

> 本轮不启动 P1：基准工程（`.cpp`、CMake、`build.sh`、`.vcxproj`、README）待用户另行指示后再实现。本文保持为口径与计划。

验收硬性条件：

- 默认模式与无锁模式在**独立进程**中运行，结果文件分开。
- 每个数值都能追溯到 `meta` + `case`；缺环境信息的结果视为无效。
- 基准程序不修改组件代码；若为测量插桩，必须用编译开关隔离且默认关闭。
- 不新增第三方运行时依赖；可选组配置期探测，缺失即跳过。

## 9. 结果判定规则（何时才做优化）

用于把数据映射到决策，避免「凭感觉优化」。

| 观察 | 判定 | 动作 |
|---|---|---|
| `ours_default` 吞吐 ≥ `mutex_deque_ref` 的 90%（S1/S2） | 包装层（pimpl/Bridge/hooks）开销可忽略 | 不做 header-only 化等 O1 类改造 |
| `ours_default` 在 4P4C 下吞吐随线程数继续上升（S3） | 单锁尚可 | 不动同步结构 |
| 4P4C 吞吐较 1P1C 明显下降且 p99 抬升 | 锁护送 | 才评估双锁/分段；先做基准复现 |
| `ours_lockfree` 相对 `ours_default` 在 4P4C 提升 < 1.5×（S3） | 无锁模式不是吞吐答案 | 不再以「性能」为由推广宏；保留为语义选项 |
| 1KB/4KB 载荷的 ns/op 较 64B 劣化 > 50%（S9） | 临界区内移动是瓶颈 | 评估「锁内换槽、锁外移动」；需单独计划 |
| `DequeueFor(1ms)` 的 p99 误差 > 5ms（S7） | 超时精度不足 | 排查条件变量实现与调度；记录平台差异 |
| `Close` 唤醒 p99 > 50ms（S8） | 唤醒路径异常 | 排查通知顺序与线程调度 |
| 目标消息速率下 p99 < 1ms 且吞吐余量 > 2× | 当前实现已够用 | 不优化内部；优先补 P0 关闭语义 |
| `ours_lockfree` 在 S15 中 RSS 单调增长 | 无界语义确认 | 在文档与选型建议中固化，不改实现 |

以上阈值是初始口径，允许在 P1 后按实测噪声调整，但调整必须记录原因。

## 10. 公平性、风险与限制

- **语义不同不能同排名**：moodycamel 无界、folly/boost 不阻塞、TBB 关闭抛异常；这些组只做参考列，不参与「本组件是否慢」的结论。
- **WSL 与原生 Windows 分开报告**：WSL 的时钟虚拟化、调度与亲和性都可能失真，不得混表。
- **容量上浮**：`CircularQueue` 内部槽位取 2 的幂，容量 1000 实际分配 1024；S11 专门记录，避免把内存差异误判为泄漏。
- **预构造 vs 惰性构造**：当前是惰性构造，S10 单独测生命周期成本，避免污染 S1/S2。
- **`T` 的移动可能抛异常**：基准只使用不抛异常的类型，异常路径由单元测试覆盖，不进基准。
- **条件变量无公平性**：S13 只描述分布；不得据此断言「饿死」。
- **分配器差异**：Windows 与 Linux 的 `operator new` 不同；跨平台只比趋势，不比绝对值。
- **第三方版本漂移**：可选组固定版本；报告中记录版本与头文件来源。
- **测量开销**：`steady_clock` 单次约几十纳秒；延迟数据必须说明采样率与是否扣除底价。
- **逻辑核与 SMT**：口径按逻辑核扫描；同一物理核上的两个超线程共享执行单元，扩展比在 SMT 机器上天然偏低。必须记录逻辑核数、物理核数与 SMT 状态，结论以逻辑核口径为准并注明拓扑。

## 11. 与既有文档/工作的关系

- 本计划是 `doc/optimization/circular_queue/optimization_roadmap.md` 中 B1/B2 在本组件上的延续：同一套「先基准、后优化」原则，但**独立工程、独立结果**。
- 组件对比中提出的 P0 项（区分关闭与超时、无锁模式 `Close` 竞态、宏改变语义）**不依赖基准**，可先行处理；本计划为其提供「关闭唤醒成本」的量化依据。
- P1 项（单锁扩展性、临界区内移动、批量接口）以本计划 S3/S9/S14 的数据为准入条件；数据不支持则关闭该项并记录原因。
- 基准结果不进入自动测试；新增代码与文档同一次提交，`benchmarks/<组件>/` 与 `doc/optimization/<组件>/` 同步登记。

## 12. 已确认口径与待确认项

### 已确认（2026-11-28）

- 平台：Windows 原生 + WSL `Ubuntu-24.04` 双平台并列为一等公民，验收要求两平台都跑通，结果分表。
- 线程口径：按**逻辑核**扫描；总线程数 2/4/8…直到逻辑核数，记录 SMT 拓扑。
- 工程实现：本轮暂缓，先不写基准工程；本文只维护口径。

### 待确认

1. 可选外部组（folly/oneTBB/boost）首批是否引入？建议先不引入，P2 得到本组件内部数据后再决定。
2. S4/S5 的「满时」如何构造：持续 `EnqueueNoWait`/`TryEnqueue` 保持满，还是周期性消费？建议前者，另加一个「半满稳态」对照。
3. 是否需要把结果生成 HTML 报告（与 fuzzy_search 一致）？建议先只产出 JSON + Markdown，HTML 按需。

## 附录：构建与运行

Windows（Developer Command Prompt，先构建核心库）：

```bat
msbuild windows_sln\erverest.vcxproj /p:Configuration=Release /p:Platform=x64 /m
msbuild benchmarks\multi_producer_multi_consumer\multi_producer_multi_consumer_benchmark.vcxproj /p:Configuration=Release /p:Platform=x64 /m
benchmarks\multi_producer_multi_consumer\x64\Release\bench_default.exe --scenario=S3 --producers=4 --consumers=4 --output=results\S3_default_4p4c.json
benchmarks\multi_producer_multi_consumer\x64\Release\bench_lockfree.exe --scenario=S3 --producers=4 --consumers=4 --output=results\S3_lockfree_4p4c.json
```

Linux（WSL `Ubuntu-24.04`）：

```powershell
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/linux_build && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/benchmarks/multi_producer_multi_consumer && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/benchmarks/multi_producer_multi_consumer && ./build/bench_default --scenario=S2'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/benchmarks/multi_producer_multi_consumer && ./build/bench_lockfree --scenario=S2'
```

- `build.sh` 必须是 LF；所有 `.sh` 转换后与索引一致。
- 基准目录的 CMake 需要能解析 `everest/` 与 `3rd`（无锁模式需要 `lock-free-queue/...`）。
- 链接 `libeverest.a` / `everest.lib` 时必须同时链接对应的 protobuf 静态库（统一编译单元内含 `packet_serialize.cpp`）。
