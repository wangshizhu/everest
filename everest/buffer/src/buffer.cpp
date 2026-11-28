/**
 * @file buffer.cpp
 * @brief Buffer 的存储、游标与扩容实现。
 */

// 本文件在统一编译单元中被 include，公共头由 common/include.h 先行包含。
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

NAMESPACE_EVEREST_BEGIN

// 缓冲区内部 helper；统一编译单元下按模块命名，避免与其他 .cpp 的同名定义冲突。
namespace buffer_detail {

/**
 * @brief 把容量向上取整到 kExpandStep 的整数倍。
 * @param capacity 期望容量。
 * @return 取整后的容量；结果无法用 std::size_t 表示时返回 nullopt。
 */
std::optional<std::size_t> FixCapacity(std::size_t capacity) noexcept {
    if (capacity < Buffer::kDefaultCapacity) {
        return Buffer::kDefaultCapacity;
    }

    const std::size_t remainder = capacity % Buffer::kExpandStep;
    if (remainder == 0) {
        return capacity;
    }

    const std::size_t delta = Buffer::kExpandStep - remainder;
    if (capacity > std::numeric_limits<std::size_t>::max() - delta) {
        return std::nullopt;
    }

    return capacity + delta;
}

}

/**
 * @brief Buffer 的存储、游标与扩容策略。
 */
struct Buffer::Impl final {
    // 存储；长度等于 capacity。
    std::unique_ptr<int8_t[]> storage;
    // 已分配字节数，恒为 kExpandStep 的整数倍（构造参数无法表示时除外）。
    std::size_t capacity = 0;
    // 可读数据起点。
    std::size_t read_offset = 0;
    // 可写数据终点。
    std::size_t write_offset = 0;
    // 容量上限。
    std::size_t max_capacity = 0;

    /**
     * @brief 可读字节数。
     * @return 当前可读字节数。
     */
    [[nodiscard]] std::size_t Size() const noexcept {
        return write_offset - read_offset;
    }

    /**
     * @brief 把剩余数据前移到存储开头并清零读偏移。
     */
    void Compact() noexcept {
        if (read_offset == 0) {
            return;
        }

        if (read_offset == write_offset) {
            read_offset = 0;
            write_offset = 0;
            return;
        }

        const std::size_t remain = write_offset - read_offset;
        std::memmove(storage.get(), storage.get() + read_offset, remain);
        read_offset = 0;
        write_offset = remain;
    }

    /**
     * @brief 判断源区间是否与存储重叠。
     * @param from 源数据起点。
     * @param size 源数据字节数。
     * @return 两个区间有交集返回 true。
     */
    [[nodiscard]] bool OverlapsStorage(const int8_t* from, std::size_t size) const noexcept;

    /**
     * @brief 判断源区间是否完整落在存储内。
     * @param from 源数据起点。
     * @param size 源数据字节数。
     * @return 源区间完全位于存储内返回 true。
     */
    [[nodiscard]] bool CoversStorage(const int8_t* from, std::size_t size) const noexcept;

    /**
     * @brief 扩容到 new_capacity 并保留可读数据。
     * @param new_capacity 目标容量；调用方保证大于当前容量且不超过 max_capacity。
     * @return 成功返回 true；参数检查或分配失败返回 false，状态不变。
     */
    bool ResizeTo(std::size_t new_capacity);
};

/**
 * @brief 判断源区间是否与存储重叠。
 * @param from 源数据起点。
 * @param size 源数据字节数。
 * @return 两个区间有交集返回 true。
 */
bool Buffer::Impl::OverlapsStorage(const int8_t* from, std::size_t size) const noexcept {
    if (size == 0) {
        return false;
    }

    // 这里只比较地址数值，不通过指针跨对象做关系比较，避免未定义行为。
    const auto storage_begin = reinterpret_cast<std::uintptr_t>(storage.get());
    const auto storage_end = storage_begin + capacity;
    const auto source_begin = reinterpret_cast<std::uintptr_t>(from);
    const auto source_end = source_begin + size;

    return source_begin < storage_end && storage_begin < source_end;
}

/**
 * @brief 判断源区间是否完整落在存储内。
 * @param from 源数据起点。
 * @param size 源数据字节数。
 * @return 源区间完全位于存储内返回 true。
 */
bool Buffer::Impl::CoversStorage(const int8_t* from, std::size_t size) const noexcept {
    const auto storage_begin = reinterpret_cast<std::uintptr_t>(storage.get());
    const auto storage_end = storage_begin + capacity;
    const auto source_begin = reinterpret_cast<std::uintptr_t>(from);

    if (source_begin < storage_begin || source_begin > storage_end) {
        return false;
    }

    return size <= storage_end - source_begin;
}

/**
 * @brief 扩容到 new_capacity 并保留可读数据。
 * @param new_capacity 目标容量；调用方保证大于当前容量且不超过 max_capacity。
 * @return 成功返回 true；参数检查或分配失败返回 false，状态不变。
 */
bool Buffer::Impl::ResizeTo(std::size_t new_capacity) {
    if (new_capacity <= capacity || new_capacity > max_capacity) {
        return false;
    }

    // 不做零初始化，避免对大块存储引入整块清零的开销。
    std::unique_ptr<int8_t[]> new_storage(new int8_t[new_capacity]);
    if (new_storage == nullptr) {
        return false;
    }

    const std::size_t remain = Size();
    if (remain > 0) {
        std::memcpy(new_storage.get(), storage.get() + read_offset, remain);
    }

    storage = std::move(new_storage);
    capacity = new_capacity;
    read_offset = 0;
    write_offset = remain;

    return true;
}

Buffer::Buffer(std::size_t init_capacity, std::size_t max_capacity) : impl_(std::make_unique<Impl>()) {
    const std::optional<std::size_t> fixed_max = buffer_detail::FixCapacity(max_capacity);
    impl_->max_capacity = fixed_max.has_value() ? *fixed_max : std::numeric_limits<std::size_t>::max();

    const std::optional<std::size_t> fixed_init = buffer_detail::FixCapacity(init_capacity);
    const std::size_t init = fixed_init.has_value() ? *fixed_init : impl_->max_capacity;
    impl_->capacity = init < impl_->max_capacity ? init : impl_->max_capacity;
    impl_->storage.reset(new int8_t[impl_->capacity]);
}

Buffer::~Buffer() = default;

std::size_t Buffer::Size() const noexcept {
    return impl_->Size();
}

std::size_t Buffer::Capacity() const noexcept {
    return impl_->capacity;
}

std::size_t Buffer::CanWriteSize() const noexcept {
    return impl_->capacity - impl_->write_offset;
}

bool Buffer::Empty() const noexcept {
    return impl_->Size() == 0;
}

bool Buffer::Full() const noexcept {
    return Size() >= impl_->capacity;
}

int8_t* Buffer::GetWritePos() noexcept {
    return impl_->storage.get() + impl_->write_offset;
}

const int8_t* Buffer::GetWritePos() const noexcept {
    return impl_->storage.get() + impl_->write_offset;
}

int8_t* Buffer::GetReadPos() noexcept {
    return impl_->storage.get() + impl_->read_offset;
}

const int8_t* Buffer::GetReadPos() const noexcept {
    return impl_->storage.get() + impl_->read_offset;
}

std::optional<std::size_t> Buffer::WriteSome(const int8_t* from, std::size_t size) {
    if (size == 0) {
        return 0;
    }

    if (from == nullptr || size > CanWriteSize()) {
        return std::nullopt;
    }

    std::memmove(impl_->storage.get() + impl_->write_offset, from, size);
    impl_->write_offset += size;

    return size;
}

bool Buffer::Write(const int8_t* from, std::size_t size) {
    if (size == 0) {
        return true;
    }

    if (from == nullptr) {
        return false;
    }

    if (WriteSome(from, size).has_value()) {
        return true;
    }

    // 源区间落在自身存储内时扩容会释放旧存储，必须先拷贝出来再扩容写入。
    if (impl_->OverlapsStorage(from, size)) {
        if (!impl_->CoversStorage(from, size)) {
            return false;
        }

        const std::vector<int8_t> source_copy(from, from + size);
        if (!Reserve(size)) {
            return false;
        }

        return WriteSome(source_copy.data(), size).has_value();
    }

    if (!Reserve(size)) {
        return false;
    }

    return WriteSome(from, size).has_value();
}

std::optional<std::size_t> Buffer::ReadSome(int8_t* to, std::size_t size) {
    if (size == 0) {
        return 0;
    }

    if (to == nullptr || size > Size()) {
        return std::nullopt;
    }

    // 只前移读位置，剩余数据保持原位；已读前缀由 Reserve() 统一压缩。
    std::memmove(to, impl_->storage.get() + impl_->read_offset, size);
    impl_->read_offset += size;

    return size;
}

bool Buffer::CommitWrite(std::size_t size) noexcept {
    if (size > CanWriteSize()) {
        return false;
    }

    impl_->write_offset += size;

    return true;
}

bool Buffer::ConsumeRead(std::size_t size) noexcept {
    if (size > Size()) {
        return false;
    }

    // 只前移读位置，避免每次消费都搬移剩余数据；已读前缀由 Reserve() 统一压缩。
    impl_->read_offset += size;

    return true;
}

bool Buffer::Reserve(std::size_t size) {
    if (size <= CanWriteSize()) {
        return true;
    }

    const std::size_t remain = Size();
    const std::size_t max_capacity = impl_->max_capacity;
    const std::size_t free_size = impl_->capacity - remain;

    // 已读前缀可以回收：先压缩，避免扩容。
    if (size <= free_size) {
        impl_->Compact();

        return true;
    }

    if (size > std::numeric_limits<std::size_t>::max() - remain) {
        return false;
    }

    const std::size_t total = remain + size;
    const std::optional<std::size_t> fixed = buffer_detail::FixCapacity(total);
    if (!fixed.has_value()) {
        return false;
    }

    // 倍增扩容，摊还拷贝成本；上限仍由 max_capacity 约束。
    std::size_t grown = max_capacity;
    if (impl_->capacity <= max_capacity - impl_->capacity) {
        grown = impl_->capacity + impl_->capacity;
    }

    std::size_t target = *fixed > grown ? *fixed : grown;
    if (target > max_capacity) {
        target = max_capacity;
    }

    if (target < total) {
        return false;
    }

    return impl_->ResizeTo(target);
}

bool Buffer::Expand(std::size_t expand_size) {
    if (expand_size == 0) {
        return true;
    }

    const std::size_t capacity = Capacity();
    if (expand_size > std::numeric_limits<std::size_t>::max() - capacity) {
        return false;
    }

    const std::size_t total = capacity + expand_size;
    if (total > impl_->max_capacity) {
        return false;
    }

    const std::optional<std::size_t> fixed = buffer_detail::FixCapacity(total);
    if (!fixed.has_value() || *fixed > impl_->max_capacity) {
        return false;
    }

    return impl_->ResizeTo(*fixed);
}

NAMESPACE_EVEREST_END
