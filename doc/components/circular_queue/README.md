# circular_queue

`everest::CircularQueue<T>` 是固定容量、先进先出（FIFO）的环形队列。写满时有两种策略可选：覆盖最旧元素，或拒绝写入。队列不做任何内部同步，必须在单个线程内使用，或由调用方自行加锁。

- 头文件：`everest/multi_producer_multi_consumer/circular_queue.h`
- 实现：`everest/multi_producer_multi_consumer/src/circular_queue.cpp`（非模板实现，经统一编译单元编入 `everest.lib`）
- 命名空间：`everest`
- 依赖：`common/define.h`、`non_copyable/non_copyable.h`

## 1. 它是什么

- 固定容量：构造时确定 `Capacity()`，运行期不扩张、不缩容。
- 惰性构造：槽位首次写入时才 `placement new`，出队、覆盖或队列析构时调用析构。
- 移动语义：入队把实参移动构造到槽位；不支持拷贝，支持移动构造/移动赋值（转移底层存储）。
- 写满策略方法化：
  - `PushBack()` / `EmplaceBack()`：覆盖最旧元素；
  - `TryPushBack()` / `TryEmplaceBack()`：拒绝写入并返回 `false`，实参不移动。
- 可观测性：`OverrunCounter()` 累计因队列已满而丢失的元素数。
- 过对齐支持：存储按 `alignof(T)` 对齐分配，支持 `alignas` 大于默认对齐的类型。

## 2. 它不是什么

- 不是并发容器：没有任何锁或原子操作，`const` 查询方法同样不是线程安全的。
- 不是通用容器：没有迭代器、`Insert`/`Erase`、`Resize`、`clear()`。
- 不提供阻塞能力：满时阻塞或等待由 `MultiProducerMultiConsumer<T>` 等包装层提供。
- 不支持拷贝；没有默认构造函数，必须提供容量。
- 不提供容量 0 的空对象构造：容量 0 会被归一为 1。

## 3. 快速开始

```cpp
#include "multi_producer_multi_consumer/circular_queue.h"

#include <string>

everest::CircularQueue<std::string> queue(3);  // 最多容纳 3 个元素

queue.PushBack(std::string("a"));              // 未满：普通入队
queue.EmplaceBack(2, 'b');                     // 原地构造 "bb"，无额外移动

if (queue.TryPushBack(std::string("c"))) {     // 成功
    // ...
}

// 已满时：PushBack 覆盖最旧，TryPushBack 拒绝
queue.PushBack(std::string("d"));              // 挤掉 "a"，OverrunCounter() == 1
const bool pushed = queue.TryPushBack(std::string("e"));  // false，"e" 未被移动

while (!queue.Empty()) {
    const std::string& front = queue.Front();
    // 使用 front；下一次 PushBack/PopFront 或队列析构后引用失效
    queue.PopFront();
}

const std::size_t lost = queue.OverrunCounter();  // 因满丢失的元素累计数
```

## 4. 类型要求

`T` 需要满足：

- 可移动构造（`std::is_move_constructible_v<T>`）；
- 可析构（`std::is_destructible_v<T>`）。

不要求默认构造，也不要求移动赋值。二者在编译期由 `static_assert` 校验，`CircularQueue<NoDefaultCtorValue>` 可以直接使用。

## 5. API

### 5.1 构造与移动

| 接口 | 说明 |
|---|---|
| `explicit CircularQueue(std::size_t capacity)` | 构造队列。`capacity == 0` 归一为 1。内部槽位取不小于 `capacity` 的最小 2 的幂；`Capacity()` 返回请求值而不是槽位数。 |
| `CircularQueue(CircularQueue&&) noexcept = default` | 移动构造。源对象变为空队列。 |
| `CircularQueue& operator=(CircularQueue&&) noexcept` | 移动赋值。释放当前存储（未弹出的元素随之析构）并接管源对象。 |

构造函数可能抛出：

- `std::length_error`：`capacity` 超过可表示的最大 2 的幂，或 `槽位数 * sizeof(T)` 无法用 `std::size_t` 表示；
- `std::bad_alloc`：对齐存储分配失败。

### 5.2 入队

| 接口 | 满时行为 | 实参状态 | 额外移动 |
|---|---|---|---|
| `void PushBack(T&& item)` | 覆盖最旧元素 | 成功后处于已移动状态 | 一次移动构造 |
| `[[nodiscard]] bool TryPushBack(T&& item)` | 返回 `false`，不写入 | 被拒绝时保持不变 | 成功时一次移动构造，失败无 |
| `template <class... Args> void EmplaceBack(Args&&... args)` | 覆盖最旧元素 | 参数被转发给 `T` 构造 | 无（原地构造） |
| `template <class... Args> [[nodiscard]] bool TryEmplaceBack(Args&&... args)` | 返回 `false`，不构造 | 参数只在成功时被使用 | 无（原地构造） |

- 覆盖最旧的实现顺序是：析构队首（最旧）元素 → `head_` 前进 → `OverrunCounter() + 1` → 在尾部槽位构造新元素。
- 被 `TryPushBack()` / `TryEmplaceBack()` 因满拒绝时，同样累加 `OverrunCounter()`（语义是“因满丢失的元素数”，详见 6.2）。
- 成功入队后，之前通过 `Front()` / `At()` 获得的引用仍然有效，直到该元素被弹出、被覆盖或队列销毁。

### 5.3 访问

| 接口 | 前置条件 | 说明 |
|---|---|---|
| `T& Front() noexcept` / `const T& Front() const noexcept` | 队列非空 | 返回队首元素引用。 |
| `T& At(std::size_t index) noexcept` / `const T& At(std::size_t index) const noexcept` | `index < Size()` | `index == 0` 表示队首，依次向后。 |
| `void PopFront() noexcept` | 队列未移动（空队列允许） | 析构并移除队首；空队列上调用是无操作。移动后的对象上调用会触发 Debug `assert`（Release 为未定义行为）。 |

- `Front()` / `At()` 的前置条件在 Debug 下用 `assert` 检查，Release 下不保证；违反前置条件属于未定义行为。
- 引用失效时机：下一次覆盖该元素的 `PushBack()`/`EmplaceBack()`、`PopFront()`、移动赋值、队列析构。其它元素的引用不受影响（除非移动赋值整体释放存储）。

### 5.4 状态查询

| 接口 | 说明 |
|---|---|
| `bool Empty() const noexcept` | `Size() == 0`。 |
| `bool Full() const noexcept` | `Size() == Capacity()`。 |
| `std::size_t Size() const noexcept` | 当前元素数，范围 `[0, Capacity()]`。 |
| `std::size_t Capacity() const noexcept` | 可用元素数（构造时的请求值，0 已归一为 1）。 |
| `std::size_t OverrunCounter() const noexcept` | 因满丢失的元素累计次数，单调递增，无重置接口。 |

## 6. 行为约定

### 6.1 容量与内存

- `Capacity()` 是可用元素数，构造后不变。
- 内部槽位数 `slot_count` 是不小于 `capacity` 的最小 2 的幂，环形下标用 `& (slot_count - 1)` 计算。
- 因此常驻内存约为 `slot_count * sizeof(T)` 加上 `std::shared_ptr` 控制块。请求 1000 实际分配 1024 个槽位；请求 0 实际 1 个。
- 槽位只在首次写入时构造，未构造槽位不持有 `T` 对象。

### 6.2 写满语义与 OverrunCounter

`OverrunCounter()` 统计的是“因队列已满而没有留下的元素数”，两条路径都会累加：

| 调用 | 已满时 | 计数 | 结果 |
|---|---|---|---|
| `PushBack` / `EmplaceBack` | 丢弃队首最旧元素，写入新元素 | +1 | 新元素入队，队列长度不变 |
| `TryPushBack` / `TryEmplaceBack` | 不写入 | +1 | 新元素被拒绝，队列不变 |

> 注意：`MultiProducerMultiConsumer<T>` 的 `TryEnqueue()`/`TryEmplace()` 有意绕过 `TryPushBack()`，因此调用方的正常拒绝不会污染其 `OverrunCounter()`（该计数只反映 `EnqueueNoWait()` 的覆盖）。`CircularQueue` 自身没有这个区分。

### 6.3 移动后的对象状态

移动构造或移动赋值之后，源对象处于“空壳”状态：

- `Capacity() == 0`、`Size() == 0`、`Empty() == true`、`Full() == false`、`OverrunCounter() == 0`；
- `PushBack()` / `EmplaceBack()` 是无操作；`TryPushBack()` / `TryEmplaceBack()` 返回 `false`；
- **不可**调用 `Front()` / `At()` / `PopFront()`：Debug 下 `assert` 失败，Release 下是未定义行为。

### 6.4 异常保证

| 场景 | 保证 |
|---|---|
| 未满时构造新元素抛异常 | **强保证**：`size_`、`tail_` 不变，队列内容与入队前完全一致。 |
| 已满时覆盖最旧，构造新元素抛异常 | **基本保证**：最旧元素已析构、`OverrunCounter()` 已 +1、新元素未入队、`Size()` 减 1；队列仍可继续使用。 |
| `TryPushBack` / `TryEmplaceBack` 因满拒绝 | 不抛异常，不构造，队列不变。 |
| 析构函数 | `noexcept`；`T` 的析构函数不得抛异常。 |

### 6.5 线程安全

- 全部接口都不做同步，同一对象上的任意并发访问（包括 `const` 查询）都是数据竞争。
- 需要多线程生产/消费时使用 `MultiProducerMultiConsumer<T>`，或由调用方提供外部互斥。

## 7. 复杂度

| 操作 | 复杂度 |
|---|---|
| `PushBack` / `TryPushBack` / `EmplaceBack` / `TryEmplaceBack` | O(1)（外加 `T` 的构造/析构代价） |
| `PopFront` / `Front` / `At` | O(1) |
| `Empty` / `Full` / `Size` / `Capacity` / `OverrunCounter` | O(1) |

每操作不产生堆分配；堆分配只发生在构造与移动赋值时。

## 8. 与 MultiProducerMultiConsumer 的关系

`MultiProducerMultiConsumer<T>` 的默认模式（未定义 `USE_LOCK_FREE_QUEUE`）内部就是：

```text
CircularQueue<T>  +  std::mutex  +  not_full_cv_ / not_empty_cv_
```

- 它是 `CircularQueue` 唯一的多线程封装，`Enqueue()` 对应“满时等待空位”，`EnqueueNoWait()` 对应 `PushBack()`，`TryEnqueue()` 对应“满时拒绝且不计数”。
- `CircularQueue` 的接口与契约不因该包装层改变；单独使用 `CircularQueue` 时仍需自行加锁。
- 性能优化方向与基准计划见 [优化路线图](../../optimization/circular_queue/optimization_roadmap.md)。

## 9. 构建与测试

组件头文件自包含，但非模板部分在 `circular_queue.cpp`，随 `everest/common/src/include.cpp` 的统一编译单元编入 `everest.lib`，因此使用方必须链接 `everest.lib`；由于统一编译单元还包含 `packet_serialize.cpp`，链接时还需要 protobuf 静态库（Windows `libs/libprotobufd.lib`，Linux `libs/<构建类型>/libprotobuf.a`）。

Windows（先构建核心库，再构建测试）：

```bat
msbuild windows_sln\erverest.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild test\circular_queue_test\circular_queue_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
test\circular_queue_test\x64\Debug\circular_queue_test.exe
```

Linux（WSL 的 Linux 发行版内）：

```powershell
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/linux_build && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/circular_queue_test && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/circular_queue_test && ./bin/circular_queue_test'
```

测试源码：`test/circular_queue_test/circular_queue_test.cpp`（GoogleTest）。

## 10. 已知限制与注意事项

- 单线程、无同步；不要把它当作 MPMC/SPSC 队列使用。无锁变体属于独立类型评估范围，不塞进本类。
- pimpl + 类型擦除（函数指针 hooks）意味着每次操作有一次跨 TU 调用与间接调用开销；是否值得内联化需要基准数据，见优化路线图。
- 前置条件（空队列 `Front()`/`At()`、越界 `At()`）只靠 Debug `assert` 兜底，Release 下不检查。
- `OverrunCounter()` 不区分“覆盖丢旧”和“拒绝新”，需要区分时由调用方按调用点自行统计。
- 移动后的对象只能查询和调用无操作接口，不能访问元素（见 6.3）。
- 不提供 `clear()`、批量入队/出队、`capacity` 运行期调整。
