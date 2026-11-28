# lru

`everest::LruList<K, V, Hash>` 是带滑动 TTL 的内存缓存，`everest::LruManager` 是进程级清扫器。缓存按条目自己的到期时间淘汰；命中会续期并移到链表头部。

- 头文件：`everest/lru/lru.h`
- 实现：`everest/lru/src/lru.cpp`（非模板部分，经统一编译单元编入 `everest.lib`）
- 命名空间：`everest`
- 依赖：`common/define.h`、`non_copyable/non_copyable.h`
- 测试：`test/lru_test/`
- 被 `common/include.h` 收录，通常直接包含 `"common/include.h"` 即可；也可以单独包含 `"lru/lru.h"`。

## 1. 它是什么

- **滑动 TTL**：`Get()` / `Put()` 命中时把到期时间刷新为 `now + ttl`，并把条目移到链表头部。
- **过期自校验**：`Get()` 自己判断 `expiry <= now`，过期立即移除并返回空指针，不依赖清扫。
- **全局清扫**：`LruManager` 只持弱引用登记缓存，按不小于 1 秒的间隔调用各缓存的 `Update()`，回收不再被访问的过期条目所占内存。
- **线程亲和**：不做任何同步，所有接口只能在缓存所属线程调用。
- **非模板句柄**：`LruBase` 让清扫器以 `LruBaseSharedPtr` / `LruBaseWeakPtr` 持有任意 `LruList`。

## 2. 它不是什么

- **不是线程安全容器**：没有锁或原子操作，`Size()` 这类 `const` 查询也不例外；跨线程访问是数据竞争。
- **没有容量上限**：只按 TTL 淘汰，不提供 `max_size`、LRU 容量淘汰或分页。
- **值类型固定**：`V` 必须是 `std::shared_ptr<T>`，空指针表示未命中；不保存裸对象、`unique_ptr` 或其他值类型。
- **没有持久化**：进程重启后缓存内容丢失。
- **没有诊断接口**：不提供 `Clear()`、条目遍历、命中率、剩余 TTL 等接口。
- **不注入时钟**：内部固定使用 `std::chrono::steady_clock`，单元测试通过缩短 TTL 并等待来制造过期。
- **不自动接入线程**：`LruManager::Update()` 由调用方按自己的节拍驱动，不挂到 `ThreadBase::Update()`。

## 3. 快速开始

```cpp
#include "lru/lru.h"

#include <chrono>
#include <cstdint>
#include <memory>

using PlayerCache = everest::LruList<std::uint64_t, std::shared_ptr<int>>;

// 创建并自动登记到 LruManager；默认 TTL 为 1 小时。
auto cache = PlayerCache::MakeLruList(std::chrono::minutes(10));

cache->Put(1001, std::make_shared<int>(42));        // 写入并移动值

std::shared_ptr<int> value = cache->Get(1001);      // 命中：续期并移到表头
if (value == nullptr) {
    // 未命中或已过期
}

cache->Erase(1001);                                 // 主动删除；返回是否删除成功

cache->Update();                                    // 立即清理本缓存的过期条目

// 周期性清扫所有已登记缓存；必须与缓存访问在同一线程。
everest::LruManager::GetInstance()->Update();
```

## 4. 模板参数与类型要求

```cpp
template <class K, class V, class Hash = std::hash<K>>
class LruList final : public LruBase;
```

| 参数 | 要求 |
|---|---|
| `K` | 键类型；需要可哈希（默认 `std::hash<K>`）且可比较（`std::unordered_map` 使用 `std::equal_to<K>`，即需要 `operator==`）。 |
| `V` | 必须是 `std::shared_ptr<T>`；由 `static_assert` 在编译期校验。 |
| `Hash` | 键哈希策略；默认 `std::hash<K>`。只参数化哈希，不参数化相等比较。 |

- `LruList` 是 `final`，不能继续派生。
- `LruList` 不可拷贝、不可移动；只能通过 `MakeLruList()` 返回的 `std::shared_ptr` 持有。
- 若要大小写不敏感的字符串键，需要键类型自身提供忽略大小写的 `operator==`，并传入忽略大小写的 `Hash`；只改 `Hash` 不够。

## 5. API

### 5.1 `LruList`

时长参数类型为 `LruBase::Duration`（即 `std::chrono::steady_clock::duration`）；`std::chrono::seconds` / `milliseconds` / `minutes` / `hours` 都可以隐式传入。

| 接口 | 说明 |
|---|---|
| `static std::shared_ptr<LruList> MakeLruList(Duration ttl = std::chrono::hours(1))` | 唯一创建入口；创建缓存并登记到 `LruManager`。`ttl <= 0` 时条目写入即过期。 |
| `[[nodiscard]] V Get(const K& key)` | 取值。命中：刷新到期时间、移到链表头部、返回值的副本；未命中或已过期：返回空指针。过期条目会被移除。 |
| `void Put(const K& key, V&& value)` | 写入或覆盖。键存在时覆盖值并刷新到期时间与头部位置；键不存在时插入。值被移动进缓存。 |
| `bool Erase(const K& key)` | 删除条目；成功返回 `true`，不存在返回 `false`。 |
| `std::size_t Size() const` | 当前条目数；**包含尚未被清理的过期条目**。 |
| `void Update() override` | 清理本缓存中所有已过期条目。 |

### 5.2 `LruManager`

| 接口 / 成员 | 说明 |
|---|---|
| `static constexpr std::chrono::seconds kSweepInterval{1}` | 相邻两次实际清扫的最小间隔。 |
| `static LruManager* GetInstance()` | 取进程内唯一实例；返回指针，始终非空。 |
| `void RegisterLruList(const LruBaseSharedPtr& cache)` | 登记缓存；空指针忽略。登记时顺带清理已销毁缓存的失效项；重复登记同一缓存会重复登记，不影响正确性。`MakeLruList()` 已自动登记，通常无需手动调用。 |
| `void Update()` | 清理所有已登记缓存中的过期条目。距上次实际清扫不足 `kSweepInterval` 时直接返回。 |

`Update()` 的实际动作：

1. 判断节流；未到间隔直接返回；
2. `lock()` 每个弱引用，失效项从登记表移除，存活的存入临时 `snapshot`；
3. 遍历完成后，对 `snapshot` 中每个缓存调用 `Update()`。

先取快照再调用缓存，是为了让值的析构里登记新缓存时不会破坏 `caches_` 的遍历。

### 5.3 `LruBase` 与类型别名

`LruBase` 是清扫器使用的非模板基类，普通使用方不需要直接接触它。

| 成员 | 说明 |
|---|---|
| `virtual ~LruBase()` | 虚析构。 |
| `virtual void Update() = 0` | 清理本缓存的全部过期条目；由派生类实现。 |
| `using LruBaseSharedPtr = std::shared_ptr<LruBase>` | 缓存基类的共享所有权。 |
| `using LruBaseWeakPtr = std::weak_ptr<LruBase>` | 管理器登记时使用的弱引用。 |

`LruBase` 的构造、`Clock` / `TimePoint` / `Duration` 类型别名和 `Ttl()` 访问器都是 `protected`，供派生缓存使用。

## 6. 行为约定

### 6.1 TTL 与过期判定

- 过期条件统一为 `expiry <= now`；到期时间点恰好等于当前时刻也算过期。
- `ttl <= 0`：条目写入后立即过期，`Get()` 返回空指针，不需要等待。
- `Get()` 命中会续期：`expiry = now + ttl`，并移到链表头部；`Put()` 覆盖或插入也会刷新到期时间与头部位置。
- 因此这是**滑动 TTL**：热点条目会持续续期，不会自然过期。

### 6.2 内存回收

- `Get()` 命中过期条目时立即移除。
- `Erase()` 主动移除。
- `Update()` 从链表尾部回收过期条目：链表按到期时间从表头到表尾单调递减，遇到第一个未过期条目即停止。
- `Size()` 不触发清理，过期但未被访问的条目仍计入。
- `LruManager` 只负责周期性回收内存；即便从不调用 `Update()`，`Get()` 的正确性也不受影响，只是过期条目会多占内存。

### 6.3 异常保证

| 场景 | 保证 |
|---|---|
| `Put()` 插入新键时 `index_.emplace` 抛 `std::bad_alloc` | 回滚已插入的链表节点，缓存内容与 `Size()` 不变，异常继续抛出。 |
| `Put()` 覆盖已有键 | `std::shared_ptr` 移动赋值不抛异常。 |
| 其余接口 | 不主动抛异常；容器操作自身的分配失败按标准库行为抛出。 |

`Put()` 失败时传入的右值可能已处于已移动状态。

### 6.4 值的析构

- 值的析构可能在容器操作过程中执行（例如 `Erase()`、`Update()`、覆盖写入、缓存销毁）。
- 值的析构中**不得访问本缓存**（`Get` / `Put` / `Erase` / `Size` / `Update`），否则属于未定义行为。
- 缓存只影响值对象的生命周期；`std::shared_ptr` 指向对象本身的线程安全由调用方负责。

## 7. 线程、生命周期与单例

### 7.1 线程模型

- `LruList` 与 `LruManager` 都**不做同步**。
- 同一个缓存的所有操作必须在同一线程调用。
- `LruManager::RegisterLruList()` 与 `Update()` 必须与缓存访问在同一线程；跨线程调用是数据竞争。
- 如果需要多线程访问，由调用方在缓存外层加锁，并把 `Update()` 安排到同一把锁/同一线程下。

### 7.2 缓存生命周期

- `MakeLruList()` 返回 `std::shared_ptr<LruList>`；最后一个 `shared_ptr` 释放时缓存析构，全部值随之析构。
- 管理器只持 `std::weak_ptr`，不会延长缓存生命周期。
- 缓存先于管理器销毁是安全的；登记表中的失效项在下次 `RegisterLruList()` 或实际清扫时移除。

### 7.3 单例

- `LruManager::GetInstance()` 返回进程内唯一实例的指针，构造/析构实现位于 `lru/src/lru.cpp`。
- 进程退出阶段（静态对象析构期间）不得调用 `GetInstance()`。
- 管理器是所有 `LruList` 共享的；没有按业务拆分的多个管理器。

## 8. 复杂度与内存

| 操作 | 复杂度 |
|---|---|
| `Get` / `Put` / `Erase` | 平均 O(1)（哈希查找 + 链表 splice/erase） |
| `Size` | O(1) |
| `LruList::Update` | O(k)，k 为被清理的过期条目数；遇到第一个未过期条目即停止 |
| `LruManager::RegisterLruList` | O(n)，n 为当前登记数（顺带清理失效项） |
| `LruManager::Update` | 每次实际清扫 O(n)，n 为登记数；间隔不小于 `kSweepInterval` |

- 每个条目包含 `K`、`V`、一个 `TimePoint`，以及 `std::list` 节点和 `std::unordered_map` 节点的开销。
- 管理器额外为每次登记保存一个 `std::weak_ptr`；重复登记会重复占用登记项，直到下次清理。

## 9. 构建与测试

`lru.h` 自包含；非模板部分在 `lru/src/lru.cpp`，随 `everest/common/src/include.cpp` 的统一编译单元编入 `everest.lib`。使用方需要链接 `everest.lib`；由于统一编译单元还包含 `packet_serialize.cpp`，链接时还需要 protobuf 静态库（Windows `libs/libprotobufd.lib`，Linux `libs/<构建类型>/libprotobuf.a`）。

Windows（先构建核心库，再构建测试）：

```bat
msbuild windows_sln\erverest.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild test\lru_test\lru_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
test\lru_test\x64\Debug\lru_test.exe
```

Linux（WSL 的 Linux 发行版内）：

```powershell
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/linux_build && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/lru_test && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/lru_test && ./bin/lru_test'
```

测试源码：`test/lru_test/lru_test.cpp`（GoogleTest）。

## 10. 已知限制与注意事项

- 没有容量上限；同一 TTL 窗口内的条目数不可预期。需要容量淘汰时应在外层另做限制。
- 值类型固定为 `std::shared_ptr`，`nullptr` 同时表示“未命中”和“过期”。
- 滑动 TTL 下热点条目会无限续期；需要绝对过期的场景不适用。
- 清扫间隔固定 1 秒；短 TTL 缓存的过期条目最多额外保留约 1 秒内存，正确性由 `Get()` 自校验保证。
- 非线程安全；`Size()` 等 `const` 方法也不能跨线程调用。
- 单例管理器进程级共享，没有显式注销接口；失效登记懒清理。
- 不提供 `Clear()`、条目遍历、命中率、剩余 TTL 等诊断能力。
- 值的析构中不得访问本缓存，否则是未定义行为。
