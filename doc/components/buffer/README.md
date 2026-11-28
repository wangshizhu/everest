# buffer

`everest::Buffer` 是独占所有权、按需扩容的连续字节缓冲区。读位置与写位置都用偏移量表示，消费数据只前移读位置、不搬移剩余数据；已读前缀在下一次 `Reserve()` 真正需要空间时一次性压缩到存储开头。缓冲区不做任何内部同步，同一对象不得跨线程并发访问。

- 头文件：`everest/buffer/buffer.h`
- 实现：`everest/buffer/src/buffer.cpp`（经统一编译单元编入 `everest.lib`）
- 命名空间：`everest`
- 依赖：`common/define.h`、`non_copyable/non_copyable.h`（头文件自包含）
- 别名：`everest::UniqueBuffer` = `std::unique_ptr<everest::Buffer>`

## 1. 它是什么

- 连续存储：可读数据始终位于一段连续内存中，`GetReadPos()` 起 `Size()` 字节可直接交给 `asio::buffer`、protobuf 或 `memcmp` 使用。
- 显式游标：写入后必须 `CommitWrite()`，读取后必须 `ConsumeRead()`；也提供由缓冲区负责复制的 `Write()` / `WriteSome()` / `ReadSome()`。
- 惰性压缩：`ConsumeRead()` / `ReadSome()` 只前移读位置，剩余数据保持原位；只有 `Reserve()` 在尾部空间不足时才把剩余数据搬到存储开头。
- 有上限扩容：容量按 `kExpandStep`（512 字节）取整；扩容目标取「向上取整后的总需求」与「容量翻倍」中的较大者，最终受 `max_capacity` 约束（默认 2 MB）。
- 失败有出口：读写、提交/消费、扩容全部通过返回值报告失败，失败时缓冲区状态不变，不做日志、不抛业务异常。
- 独占所有权：固定类型的 `final` 类，删除拷贝与移动；存储由 `Buffer` 独占。

## 2. 它不是什么

- 不是线程安全容器：没有锁、原子操作或内存序，`const` 查询同样不构成同步。
- 不是可拷贝/可移动的值类型：不能按值传递或存入 `std::vector`；需要转移所有权时用 `UniqueBuffer`。
- 不是环形缓冲：消费不会腾出尾部空间，`CanWriteSize()` 不包含已读前缀。
- 不提供自动缩容：容量只会增长（或由 `Expand()` 显式增长），没有 `shrink_to_fit`。
- 不提供迭代器、`std::span`、`Data()`/`MutableData()`、批量读写或格式化接口。
- 不做内部日志：错误全部交给调用方决定（关闭连接、记日志或重试）。

## 3. 快速开始

```cpp
#include "buffer/buffer.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

everest::Buffer buffer;                       // 初始容量 512，上限 2 MB

// 方式一：写入原始字节，由缓冲区负责复制（必要时自动扩容）
const char text[] = "hello";
buffer.Write(reinterpret_cast<const std::int8_t*>(text), sizeof(text) - 1);

// 方式二：直接写 GetWritePos()，再提交（asio / protobuf 的用法）
const char more[] = "world";
std::memcpy(buffer.GetWritePos(), more, sizeof(more) - 1);
buffer.CommitWrite(sizeof(more) - 1);

// 以下两种读取方式二选一：GetReadPos() 起 Size() 字节都是有效数据。

// 方式一：复制到外部缓冲并消费
std::int8_t out[10];
const std::optional<std::size_t> read = buffer.ReadSome(out, buffer.Size());

// 方式二：原地读取，处理完再消费
const std::int8_t* data = buffer.GetReadPos();
const std::size_t size = buffer.Size();
// 使用 data[0..size) ...
buffer.ConsumeRead(size);
```

直接使用 `GetWritePos()` 前，先确认 `CanWriteSize()` 够用；不够时调用 `Reserve(need)`：

```cpp
const std::size_t need = 4096;
if (!buffer.Reserve(need)) {                  // 超过 max_capacity 时返回 false
    // 容量无法满足：关闭连接或丢弃数据
}
socket.async_read_some(asio::buffer(buffer.GetWritePos(), buffer.CanWriteSize()), ...);
```

## 4. 数据模型与不变量

缓冲区内部只有一块存储和三个量：

```text
storage:  [ 已读前缀 | 可读数据 | 可写空间 ]
偏移量:   0       read_offset  write_offset   capacity
```

| 量 | 含义 |
|---|---|
| `read_offset` | 可读数据起点 |
| `write_offset` | 可读数据终点，也是下一个可写字节位置 |
| `capacity` | 已分配字节数，恒为 `kExpandStep` 的整数倍 |

不变量（所有公开接口返回时都成立）：

- `0 <= read_offset <= write_offset <= capacity`
- `Size() == write_offset - read_offset`
- `GetReadPos() + Size() == GetWritePos()`
- `GetWritePos() + CanWriteSize() == 存储末尾`
- `CanWriteSize() == Capacity() - Size() - read_offset`：已读前缀不算可写空间
- `Empty() == (Size() == 0)`，`Full() == (Size() >= Capacity())`，即 `read_offset == 0 && write_offset == capacity`

`ConsumeRead()` 只增加 `read_offset`，`write_offset` 与存储内容都不变；`Reserve()` 在尾部空间不足但总空闲足够时调用内部 `Compact()`，把剩余数据 `memmove` 到存储开头并清零 `read_offset`。

## 5. API

### 5.1 常量与构造

| 接口 | 说明 |
|---|---|
| `static constexpr std::size_t kDefaultCapacity = 512` | 默认初始容量，也是容量取整粒度。 |
| `static constexpr std::size_t kExpandStep = 512` | 扩容步长；所有容量都向上取整到它的整数倍。 |
| `static constexpr std::size_t kMaxCapacity = 2 * 1024 * 1024` | 默认最大容量（2 MB）。 |
| `explicit Buffer(std::size_t init_capacity = kDefaultCapacity, std::size_t max_capacity = kMaxCapacity)` | 构造并分配初始存储。 |

构造规则：

- `init_capacity` 与 `max_capacity` 都向上取整到 `kExpandStep` 的整数倍；小于 `kExpandStep` 的值按 `kExpandStep` 处理。
- 初始容量超过取整后的 `max_capacity` 时，按 `max_capacity` 分配。
- 因此 `Capacity()` 恒不小于 512：`Buffer(0, 0)` 得到 512，`Buffer(4096, 512)` 得到 512，`Buffer(600, 4096)` 得到 1024。
- 存储用 `new int8_t[]` 分配，**不做零初始化**；未提交的字节是未定义内容。
- 初始存储分配失败时抛出 `std::bad_alloc`。

`Buffer` 是固定类型且不可拷贝、不可移动（`private` 继承 `NonCopyable`）。需要所有权转移时使用 `UniqueBuffer`。

### 5.2 写入

| 接口 | 空间不足时 | 参数非法时 | 返回值 |
|---|---|---|---|
| `std::optional<std::size_t> WriteSome(const int8_t* from, std::size_t size)` | 返回 `nullopt`，不扩容、不压缩 | 返回 `nullopt` | 成功返回写入字节数 |
| `bool Write(const int8_t* from, std::size_t size)` | 自动压缩/扩容后重试；仍不够返回 `false` | 返回 `false` | 成功返回 `true` |
| `bool CommitWrite(std::size_t size) noexcept` | — | `size > CanWriteSize()` 返回 `false` | 成功返回 `true` |

- `size == 0` 一律视为成功：`WriteSome` 返回 `0`，`Write` 返回 `true`，`from` 可以为 `nullptr`。
- `from == nullptr` 且 `size > 0` 返回失败，状态不变。
- `WriteSome()` 使用 `memmove`，源区间可以与自身存储重叠（包括刚写入的数据）。
- `WriteSome()` **不会**回收已读前缀：`Size()` 未清零但 `write_offset` 已到容量末尾时，即使总空闲足够，`WriteSome()` 仍返回 `nullopt`；需要先 `Reserve()`。
- `Write()` 先尝试 `WriteSome()`；尾部空间不足时，若源区间完整落在自身存储内，先把源数据复制到临时 `std::vector`，再 `Reserve()` 并写入（避免压缩或扩容搬移数据后读到已失效内容）；源区间与存储重叠但未完整落在存储内时返回 `false`，不做部分写入。
- `CommitWrite()` 只推进写位置，不复制数据，用于「直接写 `GetWritePos()`」的路径；越界时返回 `false` 且位置不变。

### 5.3 读取

| 接口 | 前置条件 | 说明 |
|---|---|---|
| `int8_t* GetReadPos()` / `const int8_t* GetReadPos() const` | — | 可读数据起点。需要跨线程或长期持有数据时，应在存储可能被压缩/扩容前复制出去。 |
| `std::optional<std::size_t> ReadSome(int8_t* to, std::size_t size)` | `size <= Size()` | 复制到 `to` 并前移读位置；`to` 可与存储重叠（`memmove`）。数据不足或 `to == nullptr` 返回 `nullopt`，状态不变。`size == 0` 返回 `0`。 |
| `bool ConsumeRead(std::size_t size) noexcept` | `size <= Size()` | 只前移读位置，不搬移数据、不释放尾部空间；`size > Size()` 返回 `false`，状态不变。 |

原地读取的模式是：用 `GetReadPos()` / `Size()` 读出数据 → 处理成功（或决定丢弃）后 `ConsumeRead(size)`。`ReadSome()` 与 `ConsumeRead()` 都不压缩，因此消费后 `CanWriteSize()` **不会变大**，尾部空间需要靠后续 `Reserve()` 回收。

### 5.4 容量管理

| 接口 | 说明 |
|---|---|
| `bool Reserve(std::size_t size)` | 保证从 `GetWritePos()` 起至少有 `size` 字节连续可写空间。 |
| `bool Expand(std::size_t expand_size = kExpandStep)` | 立即把容量增加 `expand_size`（取整到步长）。 |

`Reserve(size)` 的判定顺序：

1. `size <= CanWriteSize()`：直接返回 `true`，**不压缩**（尾部够用就不搬数据）。
2. `size <= Capacity() - Size()`：总空闲够用，压缩已读前缀后返回 `true`，容量不变。
3. 其余情况扩容：需要总量 `total = Size() + size`，目标容量取 `max(向上取整到步长的 total, 容量翻倍)` 并封顶 `max_capacity`；若仍未达到 `total`（即超过上限）返回 `false`，状态不变。

`Reserve()` 的参数是**追加**的连续可写字节数，不是总容量：`Size() + size` 才是所需的总容量。扩容成功时剩余数据被搬到新存储开头，`read_offset` 归零，`Size()` 不变。

`Expand(expand_size)` 以当前容量为基数增加 `expand_size` 后取整；若取整结果超过 `max_capacity` 返回 `false`。`Expand(0)` 是无操作并返回 `true`。正常写入路径只需 `Reserve()`，`Expand()` 用于提前保留大批空间、避免反复扩容。

### 5.5 状态查询

| 接口 | 说明 |
|---|---|
| `std::size_t Size() const noexcept` | 可读字节数，范围 `[0, Capacity()]`。 |
| `std::size_t Capacity() const noexcept` | 已分配字节数，恒为 `kExpandStep` 的整数倍（构造参数无法表示时除外）。 |
| `std::size_t CanWriteSize() const noexcept` | 从 `GetWritePos()` 起的连续可写空间，`= Capacity() - Size() - read_offset`；不含已读前缀。 |
| `bool Empty() const noexcept` | `Size() == 0`。注意此时 `read_offset` 可能非 0，`CanWriteSize()` 可能小于 `Capacity()`。 |
| `bool Full() const noexcept` | 可读数据占满容量且没有可回收前缀（`read_offset == 0 && write_offset == capacity`）。 |
| `int8_t* GetWritePos()` / `const int8_t* GetWritePos() const` | 下一个可写字节地址；与 `CanWriteSize()` 组成连续可写区间。 |

## 6. 行为约定

### 6.1 失败时状态不变

所有可能失败的接口在失败路径上不修改容量、可读数据与读写位置：

| 调用 | 失败条件 | 结果 |
|---|---|---|
| `WriteSome` | 空间不足 / `from == nullptr` 且 `size > 0` | 返回 `nullopt`，状态不变 |
| `Write` | 超过 `max_capacity` / 空指针 / 源区间部分越界 | 返回 `false`，状态不变 |
| `ReadSome` | 数据不足 / `to == nullptr` 且 `size > 0` | 返回 `nullopt`，状态不变 |
| `CommitWrite` | `size > CanWriteSize()` | 返回 `false`，位置不变 |
| `ConsumeRead` | `size > Size()` | 返回 `false`，位置不变 |
| `Reserve` | 所需总量超过 `max_capacity` 或无法表示 | 返回 `false`，状态不变 |
| `Expand` | 取整后超过 `max_capacity` 或加法溢出 | 返回 `false`，状态不变 |

容量取整与 `Size() + size`、`Capacity() + expand_size` 都先做 `std::size_t` 溢出检查，`Reserve(SIZE_MAX)`、`Expand(SIZE_MAX)` 返回 `false` 而不是崩溃。

### 6.2 指针失效时机

- `GetReadPos()` / `GetWritePos()` 返回的指针在下一次可能压缩或扩容的调用之后失效：`Reserve()`（无论是否真的搬移，都应视为失效）、`Expand()`、以及触发扩容或压紧的 `Write()`。
- `ConsumeRead()`、`CommitWrite()`、`WriteSome()`、`ReadSome()` 不搬移存储，但读写位置指针所指向的地址会随游标变化。
- 典型错误：先取 `GetWritePos()`，再调 `Reserve()`，然后仍向旧指针写入。正确顺序是先 `Reserve()`，再取指针。
- 指针有效期内不要与任何写操作并发使用，见 6.5。

### 6.3 空、满与可写空间的语义

- `CanWriteSize()` 只统计尾部空间；`ConsumeRead()` 之后它不变。直接用 `GetWritePos()` / `CommitWrite()` 的读写循环必须周期性 `Reserve()`，否则写位置会一路推进直到 `CanWriteSize()` 为 0；用 `Write()` 追加时内部会自动 `Reserve()`。
- `Empty()` 不代表 `CanWriteSize() == Capacity()`；全部消费后尾部空间仍需 `Reserve()` 回收。
- `Full()` 为真时尾部与已读前缀都为 0，任何扩容前写入都会失败；此时 `Reserve(1)` 要么扩容，要么因超过 `max_capacity` 失败。
- `Reserve(0)` 恒为成功（`0 <= CanWriteSize()`）。

### 6.4 自别名读写

- 源区间与自身存储重叠时，`WriteSome()` / `ReadSome()` 使用 `memmove`，结果正确。
- `Write()` 在需要扩容时若发现源区间与存储重叠：完整落在存储内的，先复制到临时 `std::vector` 再扩容写入；未完整落在存储内的，直接返回 `false`，不做部分写入。
- 自别名路径只影响扩容场景；同一份数据「原地追加」是受支持的用法。

### 6.5 线程安全

- 全部接口都不做同步，同一对象上的任意并发访问（包括 `const` 查询与 `GetReadPos()`）都是数据竞争。
- 需要跨线程传递数据时，复制出字节或转移 `UniqueBuffer` 所有权，不要共享同一个 `Buffer`。

### 6.6 异常

| 场景 | 保证 |
|---|---|
| 构造时分配初始存储失败 | 抛出 `std::bad_alloc`，对象不构造。 |
| `Reserve` / `Expand` / `Write` 扩容时分配失败 | 新存储先分配、数据先复制，再替换旧存储，因此抛出 `std::bad_alloc` 时**强保证**：原状态完整可用。 |
| `Write` 自别名路径复制到 `std::vector` 失败 | 在修改状态之前抛出，强保证。 |
| 其余接口 | 不抛异常；越界与非法参数通过返回值报告。 |
| 析构 | `noexcept`。 |

## 7. 复杂度

| 操作 | 复杂度 |
|---|---|
| `Size` / `Capacity` / `CanWriteSize` / `Empty` / `Full` / `GetReadPos` / `GetWritePos` | O(1) |
| `CommitWrite` / `ConsumeRead` | O(1)（只改偏移量，不搬数据） |
| `WriteSome` / `ReadSome` | O(size)（一次 `memmove`） |
| `Write`（尾部够用） | O(size) |
| `Write` / `Reserve` 触发压缩 | O(Size())（`memmove` 剩余数据） |
| `Reserve` / `Expand` 触发扩容 | O(Size())（分配新存储 + `memcpy` 可读数据） |

每个 `Buffer` 有两次堆分配：`Impl` 本身与存储。扩容会分配新存储并释放旧存储（旧存储没有复用池）。`Reserve` 的倍增策略让连续追加的摊还拷贝成本为 O(1) 每次写入。

## 8. 与网络 / 序列化层的集成

- `SessionBase` 的读缓冲：`DoRead()` 先 `Reserve(1)`，再用 `asio::buffer(GetWritePos(), CanWriteSize())` 异步读，回调里 `CommitWrite(bytes_transferred)`，然后 `OnRead()` 用 `GetReadPos()` / `Size()` 取数据、`ConsumeRead()` 消费。
- `SessionBase` 的发送缓冲：`Write()` 把包复制进 `send_data_`，`DoWrite()` 用 `asio::buffer(GetReadPos(), Size())` 异步发送，回调里 `ConsumeRead(bytes_transferred)`。
- `packet_serialize` 的 `operator<<`：`Reserve(ByteSizeLong())` → 序列化到 `GetWritePos()` → `CommitWrite(实际字节数)`；空间不足或序列化失败时不提交任何字节。
- `packet_serialize` 的 `operator>>`：用 `GetReadPos()` / `Size()` 解析，**无论解析成功与否都消费整个缓冲区**（保持原有「一条缓冲一条消息、解析失败即整包丢弃」的行为）。
- 超过 `INT_MAX` 的消息被拒绝：protobuf 的 `int` 长度参数无法表示，`operator<<` 记错误并返回，`operator>>` 丢弃整个缓冲。

## 9. 构建与测试

组件实现位于 `src/buffer.cpp`，随 `everest/common/src/include.cpp` 的统一编译单元编入 `everest.lib`，使用方必须链接 `everest.lib`；由于统一编译单元还包含 `packet_serialize.cpp`，链接时还需要 protobuf 静态库（Windows `libs/libprotobufd.lib`，Linux `libs/<构建类型>/libprotobuf.a`）。

Windows（先构建核心库，再构建测试）：

```bat
msbuild windows_sln\erverest.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild test\buffer_test\buffer_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
test\buffer_test\x64\Debug\buffer_test.exe
```

Linux（WSL 的 Linux 发行版内）：

```powershell
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/linux_build && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/buffer_test && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/buffer_test && ./bin/buffer_test'
```

测试源码：`test/buffer_test/buffer_test.cpp`（GoogleTest）。用例覆盖构造与容量取整、两种写入方式、读后压缩时机、倍增扩容与上限、越界失败不改状态、自别名写入、protobuf 往返与超限不提交、不可拷贝/不可移动。

## 10. 已知限制与注意事项

- **`CanWriteSize()` 不包含已读前缀**，这是最容易被误用的点：直接用 `asio::buffer(GetWritePos(), CanWriteSize())` 前，必须先 `Reserve(1)`；只想追加数据时用 `Write()`（内部会自动 `Reserve()`）。
- **消费不自动压缩**：`ConsumeRead()` / `ReadSome()` 是 O(1)，但已读前缀要等到下一次 `Reserve()` 触发压缩才回收；长期「读一点、写一点」而不调用 `Reserve()` 的循环会提前触发扩容。
- **容量不自动缩容**：没有 `shrink_to_fit`；峰值容量会一直保留到对象析构。
- **不接受外部存储**：构造只分配自己的存储，无法包装调用方内存；需要零拷贝解析时先复制进缓冲区或直接使用原始指针。
- **不可拷贝、不可移动**：不能放进标准容器；所有权转移用 `UniqueBuffer`，读写需要固定对象时由调用方持有引用/指针。
- **pimpl 开销**：构造多一次 `Impl` 分配；成员函数定义在 `.cpp`，统一编译单元内的调用可内联，其他 TU 的调用没有内联机会。会话级对象（每连接 2 个 `Buffer`）会多 2 次堆分配。
- **存储不零初始化**：`CommitWrite()` 之前读取未写入区域是未定义内容；写入的字节数必须准确。
- **单线程**：没有同步，跨线程共享必须先复制或转移所有权。
- **上限由构造决定**：默认 2 MB，超过后 `Reserve()` / `Write()` 返回失败，由调用方决定重试、丢弃或关闭连接；`packet_serialize` 另外拒绝超过 `INT_MAX` 的单条消息。

## 11. 与旧接口的关系

旧 `BufferBase` / `ExpandBuffer` 两层结构已删除，统一为 `Buffer`。迁移对照：

| 旧接口 | 新接口 |
|---|---|
| `BufferBase` / `ExpandBuffer` | `Buffer` |
| `UniqueExpandBuffer` / `SharedExpandBuffer` | `UniqueBuffer`（`SharedExpandBuffer` 无调用方，直接删除） |
| `MoveWritePos(size)` | `CommitWrite(size)`，返回 `bool` |
| `MoveReadPos(size)` | `ConsumeRead(size)`，返回 `bool` |
| `ExpandWrite(from, size)` | `Write(from, size)`，返回 `bool` |
| `RemainSize()` | 删除；需要容量保证时用 `Reserve(size)`，需要尾部可写空间时用 `CanWriteSize()` |
| `Tidy()` / `FixCapacity()` / 外部裸内存构造 | 删除；压缩由 `Reserve()` 内部完成 |
