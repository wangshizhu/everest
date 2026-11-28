# multi_producer_multi_consumer

`everest::MultiProducerMultiConsumer<T>` 是支持多生产者、多消费者并发访问的 FIFO 队列。它用同一个模板在**编译期**切换两种实现：

| 模式 | 条件 | 底层实现 | 一句话定位 |
|---|---|---|---|
| 默认模式 | 未定义 `USE_LOCK_FREE_QUEUE` | `CircularQueue<T>` + 单互斥量 + 两个条件变量 | 有界、可阻塞、可覆盖、有精确计数 |
| 无锁模式 | 定义 `USE_LOCK_FREE_QUEUE` | `moodycamel::BlockingConcurrentQueue<T>` | 无界（容量只是提示）、入队不阻塞、无覆盖计数 |

- 头文件：`everest/multi_producer_multi_consumer/multi_producer_multi_consumer.h`
- 默认模式实现：`everest/multi_producer_multi_consumer/src/multi_producer_multi_consumer.cpp`（经统一编译单元编入 `everest.lib`）
- 命名空间：`everest`

## 1. 它是什么

- 所有公共方法可被多个线程并发调用；元素按先进先出顺序取出。
- 入队把实参移动到队列，出队把元素移动赋值到调用方提供的 `T&`。
- 支持带超时的出队（`DequeueFor`），也可用 0/负时长做非阻塞尝试。
- 支持关闭（`Close`）：关闭后拒绝入队、唤醒等待者、排空后出队立即返回。
- 默认模式提供三种入队语义：满时阻塞、满时覆盖最旧、满时立即拒绝；并有 `OverrunCounter()` 统计覆盖丢失。
- 支持原地构造（`Emplace` / `TryEmplace`），避免调用方先构造再移动。

## 2. 它不是什么

- 不是批量队列：没有 `enqueue_bulk` / `dequeue_bulk`。
- 没有 `Peek`、迭代器、`Clear`、重置容量、重新打开。
- 无锁模式不提供背压：`capacity` 不是硬上限，队列可能持续增长，调用方必须自行限流。
- 默认模式的 `DequeueFor()` 无法区分“超时”和“已关闭且为空”，两者都返回 `false`。
- 不保证公平性：条件变量唤醒顺序、moodycamel 的调度都不承诺 FIFO 公平或防饿死。
- 不是可拷贝/可移动类型。

## 3. 头文件、宏与链接

```cpp
#include "multi_producer_multi_consumer/multi_producer_multi_consumer.h"
```

- include 根是 `everest/`。
- 无锁模式额外需要 `3rd/` 作为 include 根，因为头文件会 `#include <lock-free-queue/blockingconcurrentqueue.h>`；仓库的 `erverest.vcxproj` 与无锁测试工程都已包含该路径。
- **宏必须在同一程序的所有翻译单元中保持一致**：两种模式的类布局不同（`shared_ptr` 核心 vs `moodycamel` 队列 + `atomic<bool>`），混用会在链接期或运行期破坏 ODR。切换模式应视为切换组件语义，而不是运行时选项。
- 默认模式依赖 `everest.lib` 中的非模板核心实现；无锁模式只做模板实例化（moodycamel 为 header-only）。仓库内测试工程仍按统一编译单元惯例链接 `everest.lib` 与 protobuf 静态库（`libeverest.a` 内含 `packet_serialize.cpp`）。

## 4. 快速开始

### 4.1 默认模式（有界、阻塞）

```cpp
#include "multi_producer_multi_consumer/multi_producer_multi_consumer.h"

#include <chrono>
#include <string>
#include <thread>

everest::MultiProducerMultiConsumer<std::string> queue(1024);

// 生产者线程
queue.Enqueue(std::string("msg"));       // 满时阻塞，Close 后返回 false
queue.EnqueueNoWait(std::string("hot")); // 满时覆盖最旧，OverrunCounter() + 1
if (!queue.TryEnqueue(std::string("x"))) {
    // 满或被关闭；实参未被移动
}

// 消费者线程
std::string item;
while (queue.DequeueFor(item, std::chrono::milliseconds(100))) {
    // 处理 item
}
// 返回 false：超时，或队列已关闭且已排空
```

### 4.2 无锁模式（`USE_LOCK_FREE_QUEUE`）

```cpp
// 编译选项：/DUSE_LOCK_FREE_QUEUE（MSVC）或 -DUSE_LOCK_FREE_QUEUE（GCC/Clang）
everest::MultiProducerMultiConsumer<std::string> queue(1024);  // 1024 只是初始分配提示

queue.Enqueue(std::string("a"));          // 不因“满”阻塞；分配失败抛 std::bad_alloc
queue.Emplace("b");                       // 先构造临时对象再入队，比默认模式多一次移动

std::string item;
queue.DequeueFor(item, std::chrono::milliseconds(100));  // 限时等待
queue.Close();                            // 不能中断已经进入等待的 DequeueFor
```

### 4.3 关闭与退出

```cpp
queue.Close();                 // 可重复调用
std::string item;
while (queue.DequeueFor(item, std::chrono::milliseconds(10))) {
    // 取完剩余元素；之后立即返回 false
}
producer.join();
consumer.join();
// 必须等所有线程退出后再析构队列
```

## 5. 类型要求

`T` 必须满足（编译期 `static_assert`）：

- 可默认构造（`std::is_default_constructible_v<T>`）；
- 可移动构造；
- 可移动赋值（出队通过 `popped_item = std::move(front)` 交付）；
- 可析构。

另外：

- `T` 的移动构造/移动赋值不得回调本队列（默认模式在持锁状态下执行，会死锁）。
- `T` 的析构函数不得抛异常。
- 只移动类型（如 `std::unique_ptr<int>`）可用；不支持拷贝语义的入队接口（本组件没有 `Enqueue(const T&)`）。

## 6. API

### 6.1 构造

| 接口 | 默认模式 | 无锁模式 |
|---|---|---|
| `explicit MultiProducerMultiConsumer(std::size_t capacity)` | `capacity` 是有界上限，`0` 归一为 1 | `capacity` 是初始分配提示，`0` 归一为 1；队列可增长到超过它 |
| 异常 | `std::length_error`（容量/字节数不可表示）、`std::bad_alloc` | `std::bad_alloc` |

对象不可拷贝、不可移动（私有继承 `NonCopyable`）。

### 6.2 入队

| 接口 | 默认模式 | 无锁模式 |
|---|---|---|
| `[[nodiscard]] bool Enqueue(T&& item)` | 满时阻塞等待空位；关闭后返回 `false`，实参不变 | 不阻塞；关闭后返回 `false`；分配失败抛 `std::bad_alloc` |
| `[[nodiscard]] bool EnqueueNoWait(T&& item)` | 满时覆盖最旧元素，`OverrunCounter() + 1`；关闭后返回 `false` | 与 `Enqueue()` 相同 |
| `[[nodiscard]] bool TryEnqueue(T&& item)` | 满时返回 `false`（不计数、不覆盖）；关闭后返回 `false` | 仅关闭或分配失败返回 `false`（不抛异常） |
| `template <class... Args> [[nodiscard]] bool Emplace(Args&&... args)` | 锁内原地构造；满时阻塞；关闭后 `false` | 先构造临时对象再入队（多一次移动）；关闭后 `false`；分配失败抛 `bad_alloc` |
| `template <class... Args> [[nodiscard]] bool TryEmplace(Args&&... args)` | 满时或关闭后返回 `false`；不计数 | 关闭或分配失败返回 `false`；不抛异常 |

- 所有入队接口都返回 `[[nodiscard]] bool`，`false` 只表示“没有入队”，必须处理。
- 成功入队后实参处于已移动状态；失败时（关闭、拒绝）默认模式保证实参不变。
- `Emplace` 的参数只在本次调用内有效，构造/析构发生在调用线程。

### 6.3 出队

```cpp
[[nodiscard]] bool DequeueFor(T& popped_item, std::chrono::milliseconds wait_duration);
```

| 情形 | 行为 |
|---|---|
| 队列非空 | 立即取队首，移动赋值到 `popped_item`，返回 `true` |
| 队列为空且 `wait_duration > 0` | 最多等待 `wait_duration`；期间入队则取走并返回 `true`，否则返回 `false` |
| 队列为空且 `wait_duration <= 0` | 只做一次非阻塞尝试 |
| 已关闭且队列为空 | 立即返回 `false`，不再等待 |
| 已关闭且队列非空 | 继续取出剩余元素；排空后按上一条处理 |

- 返回 `false` 无法区分“超时”与“已关闭且为空”。需要区分时，调用方应自己记录 `Close()` 的调用，或维护独立的 `stopping` 标志。
- 默认模式下移动赋值在锁内完成；若 `T` 的移动赋值抛异常，元素仍在队列中，`popped_item` 内容未定义，队列本身可继续使用。无锁模式下 moodycamel 不保证移动赋值抛异常时的强异常安全。
- 无锁模式下，`Close()` 不能中断已经进入 `wait_dequeue_timed` 的等待，该调用仍会在超时后返回；消费者必须使用有界超时。

### 6.4 关闭

```cpp
void Close();
```

- 幂等，可重复调用；没有“重新打开”。
- 关闭后所有入队接口返回 `false`，实参不移动。
- 默认模式会唤醒所有阻塞在 `Enqueue()` 与 `DequeueFor()` 的线程。
- 无锁模式只设置关闭标志，已阻塞的 `wait_dequeue_timed` 不会被立即唤醒。
- 关闭只影响入队与等待行为，不影响剩余元素的取出。

### 6.5 查询

| 接口 | 默认模式 | 无锁模式 |
|---|---|---|
| `[[nodiscard]] std::size_t Size() const` | 锁内精确快照 | `size_approx()` 估算值 |
| `[[nodiscard]] std::size_t Capacity() const noexcept` | 构造时的有界上限 | 构造时的初始容量提示 |
| `[[nodiscard]] std::size_t OverrunCounter() const` | 仅统计 `EnqueueNoWait()` 覆盖最旧的累计次数 | 恒为 0 |

- `Size()` 是瞬时快照，不能用于同步判断（如“Size()>0 就一定能取到”）。无锁模式下尤其只是估算。
- 默认模式下 `Size()` / `OverrunCounter()` 会同所有入队/出队争用同一把互斥量；不要在 `T` 的移动构造/移动赋值里调用它们（死锁）。
- `Capacity()` 不加锁。

## 7. 两种模式的行为对照

| 维度 | 默认模式 | 无锁模式 |
|---|---|---|
| 有界性 | 有界，`Capacity()` 是硬上限 | 无界，容量只是分配提示 |
| 满时 `Enqueue` | 阻塞直到有空位或被关闭 | 直接入队，不阻塞 |
| 满时 `EnqueueNoWait` | 覆盖最旧并计数 | 与普通入队相同 |
| 满时 `TryEnqueue` | 返回 `false`，不覆盖、不计数 | 只有关闭/分配失败才返回 `false` |
| `Emplace` | 原地构造，无额外移动 | 临时对象 + 一次移动 |
| `Close` 唤醒等待者 | 是（`Enqueue` 与 `DequeueFor`） | 否，等待者按超时返回 |
| `Size()` | 精确（锁内） | 估算 |
| `OverrunCounter()` | 覆盖计数 | 恒 0 |
| 内存增长 | 构造后固定 | 随负载增长，需要外部限流 |
| 每操作分配 | 0 | 可能分配（moodycamel 分块） |

选择建议：

- 需要背压、固定内存、精确丢弃统计，或消费者/生产者数量较少：默认模式。
- 生产者不能被阻塞、可接受无界内存与估算 `Size()`：无锁模式。
- 不要在同一个程序里混用两种模式（见第 3 节）。

## 8. 所有权与生命周期

- 队列拥有全部元素；出队把元素移动赋值给调用方对象。
- 队列对象不可拷贝、不可移动，构造后地址稳定。
- **析构前必须 `Close()` 并 join 所有可能调用队列的线程**。在仍有线程阻塞于 `Enqueue()` / `DequeueFor()` 时析构是未定义行为。
- 默认模式内部核心通过 `std::shared_ptr` 持有，但公共类不暴露共享所有权；生命周期由调用方对象本身决定。

## 9. 异常保证

| 场景 | 默认模式 | 无锁模式 |
|---|---|---|
| 构造 | `std::length_error` / `std::bad_alloc`，对象未构造 | `std::bad_alloc` |
| 入队时 `T` 移动构造抛异常 | 未满：队列不变（强保证）；覆盖最旧：最旧元素已丢、`Size()` 减 1、`OverrunCounter()` 已加 1，队列可继续使用（基本保证） | 异常向外传播；moodycamel 自身保证取决于实现 |
| 出队时 `T` 移动赋值抛异常 | 元素仍在队列中，`popped_item` 内容未定义，队列可继续使用 | moodycamel 不保证强异常安全 |
| `Close` / 查询 | 不抛异常 | `Close` 不抛；查询不抛 |

- 默认模式的 `Enqueue()` 不会因为内存不足抛 `bad_alloc`（分配只发生在构造时）；异常只可能来自 `T` 的移动构造。
- 无锁模式下 `TryEnqueue()` / `TryEmplace()` 不抛异常，分配失败以 `false` 返回。

## 10. 线程安全与并发语义

- 所有公共方法可被多个线程并发调用。
- 默认模式用单互斥量保护内部状态，`not_full_cv_` 通知生产者、`not_empty_cv_` 通知消费者；临界区包含 `T` 的移动构造/移动赋值，因此大对象会拉长临界区。
- 无锁模式由 moodycamel 保证并发安全；`closed_` 是 `std::atomic<bool>`。
- 条件变量不保证公平；多个消费者竞争时可能出现取走数量不均，这是允许的。
- `OverrunCounter()`（默认模式）与 `Size()` 的读取会加锁，属于快照，不是同步原语。

## 11. 推荐使用模式

### 11.1 消费者可退出的循环

```cpp
std::atomic<bool> stop{false};
std::thread producer([&] {
    for (int i = 0; i < 100000 && !stop.load(); ++i) {
        while (!queue.Enqueue(Item(i))) {  // 被关闭
            return;
        }
    }
});

std::thread consumer([&] {
    Item item;
    while (queue.DequeueFor(item, std::chrono::milliseconds(100))) {
        Consume(item);
    }
    // 超时或已关闭排空；结合 stop 判断是否继续
});

// 退出顺序：先停生产者，再关闭队列唤醒消费者，最后 join
stop.store(true);
queue.Close();
producer.join();
consumer.join();
```

### 11.2 需要丢弃统计（默认模式）

```cpp
queue.EnqueueNoWait(Item(...));  // 允许覆盖最旧
if (queue.OverrunCounter() > 0) {
    // 上报覆盖丢失；TryEnqueue 的拒绝不会进入这个计数
}
```

### 11.3 无锁模式下的限流

```cpp
// capacity 不是上限；生产者过快时队列会持续增长
if (queue.Size() < kHighWatermark) {
    queue.Enqueue(Item(...));
} else {
    // 自行降速、丢弃或落盘
}
```

## 12. 与 CircularQueue 的关系

- 默认模式是 `CircularQueue<T>` 的并发封装，语义映射见 `doc/components/circular_queue/README.md`。
- 但 `TryEnqueue()` 有意不使用 `CircularQueue::TryPushBack()`：先在锁内判断 `Full()`，满则直接拒绝，从而避免把调用方的正常拒绝计入 `OverrunCounter()`。
- 单独使用 `CircularQueue` 时 `OverrunCounter()` 会同时统计“覆盖丢旧”和“拒绝新”，两者不要混为一谈。

## 13. 构建与测试

Windows（先构建核心库，再构建测试）：

```bat
msbuild windows_sln\erverest.vcxproj /p:Configuration=Debug /p:Platform=x64 /m

msbuild test\multi_producer_multi_consumer_test\multi_producer_multi_consumer_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
test\multi_producer_multi_consumer_test\x64\Debug\multi_producer_multi_consumer_test.exe

msbuild test\multi_producer_multi_consumer_lock_free_test\multi_producer_multi_consumer_lock_free_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
test\multi_producer_multi_consumer_lock_free_test\x64\Debug\multi_producer_multi_consumer_lock_free_test.exe
```

Linux（WSL 的 Linux 发行版内）：

```powershell
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/linux_build && bash build.sh'

wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/multi_producer_multi_consumer_test && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/multi_producer_multi_consumer_test && ./bin/multi_producer_multi_consumer_test'

wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/multi_producer_multi_consumer_lock_free_test && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/multi_producer_multi_consumer_lock_free_test && ./bin/multi_producer_multi_consumer_lock_free_test'
```

- 两个测试工程共享同一份用例头 `test/multi_producer_multi_consumer_test/multi_producer_multi_consumer_test_suite.h`；无锁测试通过 `USE_LOCK_FREE_QUEUE` 编译定义选择模式，并额外包含 `3rd/` 头文件根。
- 基准计划见 [doc/optimization/multi_producer_multi_consumer/benchmark_plan.md](../../optimization/multi_producer_multi_consumer/benchmark_plan.md)（尚未实现基准工程）。

## 14. 已知限制与注意事项

- **模式切换是编译期的**：宏改变类布局与语义，不能在同一程序内混用；已有编译产物（`everest.lib`）不受消费者宏影响，但所有使用该模板的 TU 必须一致。
- 默认模式 `DequeueFor()` 的 `false` 无法区分超时与关闭；需要精确判断时维护外部停止标志。
- 无锁模式 `Close()` 不能唤醒已阻塞的消费者，必须使用有界超时轮询。
- 无锁模式没有背压，`Size()` 只是估算，不能用于严格限流判断。
- 默认模式在锁内移动 `T`：大对象会显著拉长临界区，高并发时表现为吞吐下降与尾延迟抬升。
- 没有批量接口、`Peek`、`Clear`；元素只能出队后处理。
- `T` 的移动构造/移动赋值/析构不得回调队列（默认模式会死锁）。
- 无锁模式 `Emplace` 比默认模式多一次移动；对构造代价高、移动昂贵的类型不划算。
