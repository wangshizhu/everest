/**
 * @file circular_queue.cpp
 * @brief CircularQueue<T> 的槽位存储与环形索引实现。
 */

// 本文件在统一编译单元中被 include，公共头由 common/include.h 先行包含。
#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

NAMESPACE_EVEREST_BEGIN

namespace detail {

/**
 * @brief CircularQueue<T> 的槽位存储与环形索引实现。
 *
 * 只操作 void*，具体类型的移动构造、移动赋值、析构通过 CircularQueueHooks 完成。
 * 已构造元素恒为逻辑区间 [head_, head_ + size_) 对 slot_count_ 取模，其余槽位
 * 保持未初始化。线程安全：不做同步，调用方负责外部同步。
 */
class CircularQueueCore final : private NonCopyable {
public:
    /**
     * @brief 构造槽位存储；只分配对齐存储，不构造任何槽位。
     * @param capacity 可用元素数；0 归一为 1。
     * @param hooks 槽位的移动构造、移动赋值、析构回调。
     * @throw std::length_error 容量或槽位字节数无法表示时抛出。
     */
    CircularQueueCore(std::size_t capacity, const CircularQueueHooks& hooks);

    /**
     * @brief 析构全部已构造元素并释放对齐存储。
     */
    ~CircularQueueCore();

    /**
     * @brief 把元素移动到队尾；队列已满时覆盖最旧元素。
     * @param item 待入队元素；成功后处于已移动状态。
     */
    void PushBack(void* item);

    /**
     * @brief 尝试把元素移动到队尾；队列已满时拒绝写入。
     * @param item 待入队元素；仅在入队成功时被移动。
     * @return 入队成功返回 true；队列已满返回 false。
     */
    bool TryPushBack(void* item);

    /**
     * @brief 用构造参数在队尾原地构造元素；队列已满时覆盖最旧元素。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     */
    void EmplaceBack(void* params, CircularQueueConstructFn construct);

    /**
     * @brief 尝试用构造参数在队尾原地构造元素；队列已满时拒绝写入。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     * @return 构造成功返回 true；队列已满返回 false。
     */
    bool TryEmplaceBack(void* params, CircularQueueConstructFn construct);

    /**
     * @brief 返回队首元素地址。
     * @return 队首元素地址；要求队列非空。
     */
    void* Front() noexcept;

    /**
     * @brief 返回队首元素地址。
     * @return 队首元素地址；要求队列非空。
     */
    const void* Front() const noexcept;

    /**
     * @brief 返回逻辑下标对应元素的地址。
     * @param index 逻辑下标，0 表示队首；要求 index 小于 size()。
     * @return 元素地址。
     */
    void* At(std::size_t index) noexcept;

    /**
     * @brief 返回逻辑下标对应元素的地址。
     * @param index 逻辑下标，0 表示队首；要求 index 小于 size()。
     * @return 元素地址。
     */
    const void* At(std::size_t index) const noexcept;

    /**
     * @brief 移除队首元素；队列为空时无操作。
     */
    void PopFront() noexcept;

    /**
     * @brief 判断队列是否为空。
     * @return 没有元素返回 true。
     */
    bool Empty() const noexcept;

    /**
     * @brief 判断队列是否已满。
     * @return 元素数等于容量返回 true。
     */
    bool Full() const noexcept;

    /**
     * @brief 返回当前元素数。
     * @return 元素数，范围 [0, capacity_]。
     */
    std::size_t Size() const noexcept;

    /**
     * @brief 返回可用元素数。
     * @return 可用元素数。
     */
    std::size_t Capacity() const noexcept;

    /**
     * @brief 返回覆盖最旧元素的累计次数。
     * @return 累计覆盖次数。
     */
    std::size_t OverrunCounter() const noexcept;

private:
    /**
     * @brief 在队尾槽位上构造元素；队列已满时先丢弃最旧元素。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     * @throw 构造抛出的任何异常。
     */
    void ConstructAtTail(void* params, CircularQueueConstructFn construct);

    /**
     * @brief 尝试在队尾槽位上构造元素；队列已满时拒绝写入。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     * @return 构造成功返回 true；队列已满返回 false。
     * @throw 构造抛出的任何异常。
     */
    bool TryConstructAtTail(void* params, CircularQueueConstructFn construct);

    /**
     * @brief 计算不小于 capacity 的最小 2 的幂，作为槽位数。
     * @param capacity 可用元素数；调用方保证非 0。
     * @return 槽位数。
     * @throw std::length_error 不存在可表示的 2 的幂时抛出。
     */
    static std::size_t NextSlotCount(std::size_t capacity);

    /**
     * @brief 计算槽位下标对应的地址。
     * @param index 槽位下标；必须小于 slot_count_。
     * @return 槽位地址。
     */
    void* SlotAt(std::size_t index) const noexcept;

    // 槽位存储；已按 hooks_.slot_align 对齐。
    void* storage_ = nullptr;
    // 槽位数，取不小于 capacity_ 的最小 2 的幂，用于掩码取模。
    std::size_t slot_count_ = 0;
    // slot_count_ - 1；用按位与替代取模计算环形下标。
    std::size_t slot_mask_ = 0;
    // 队首槽位下标。
    std::size_t head_ = 0;
    // 下一个写入位置的槽位下标。
    std::size_t tail_ = 0;
    // 当前元素数。
    std::size_t size_ = 0;
    // 可用元素数。
    std::size_t capacity_ = 0;
    // 覆盖最旧元素的累计次数。
    std::size_t overrun_counter_ = 0;
    // 槽位的移动构造、移动赋值、析构回调。
    CircularQueueHooks hooks_{};
};

CircularQueueCore::CircularQueueCore(std::size_t capacity, const CircularQueueHooks& hooks) : capacity_(capacity == 0 ? 1 : capacity), hooks_(hooks) {
    // 槽位数取不小于 capacity_ 的最小 2 的幂，后面用掩码替代取模。
    slot_count_ = NextSlotCount(capacity_);
    slot_mask_ = slot_count_ - 1;

    // 槽位字节数同样必须可表示。
    if ((hooks_.slot_size != 0) && (slot_count_ > (std::numeric_limits<std::size_t>::max() / hooks_.slot_size))) {
        throw std::length_error("circular queue storage size is not representable");
    }

    const std::size_t storage_bytes = slot_count_ * hooks_.slot_size;
    storage_ = ::operator new(storage_bytes, std::align_val_t{hooks_.slot_align});
}

CircularQueueCore::~CircularQueueCore() {
    for (std::size_t index = 0; index < size_; ++index) {
        hooks_.destroy(SlotAt((head_ + index) & slot_mask_));
    }
    ::operator delete(storage_, std::align_val_t{hooks_.slot_align});
}

void CircularQueueCore::PushBack(void* item) {
    ConstructAtTail(item, hooks_.move_construct);
}

bool CircularQueueCore::TryPushBack(void* item) {
    return TryConstructAtTail(item, hooks_.move_construct);
}

void CircularQueueCore::EmplaceBack(void* params, CircularQueueConstructFn construct) {
    ConstructAtTail(params, construct);
}

bool CircularQueueCore::TryEmplaceBack(void* params, CircularQueueConstructFn construct) {
    return TryConstructAtTail(params, construct);
}

void CircularQueueCore::ConstructAtTail(void* params, CircularQueueConstructFn construct) {
    if (Full()) {
        // 满时 tail_ 槽位即将承载新元素：先析构最旧元素并推进 head_，
        // 使 tail_ 成为未初始化槽位，再在其上构造。
        hooks_.destroy(SlotAt(head_));
        head_ = (head_ + 1) & slot_mask_;
        ++overrun_counter_;

        try {
            construct(SlotAt(tail_), params);
        } catch (...) {
            // 最旧元素已丢、新元素未入队，元素数减 1 以保持不变式。
            --size_;
            throw;
        }

        tail_ = (tail_ + 1) & slot_mask_;
        return;
    }

    construct(SlotAt(tail_), params);
    tail_ = (tail_ + 1) & slot_mask_;
    ++size_;
}

bool CircularQueueCore::TryConstructAtTail(void* params, CircularQueueConstructFn construct) {
    if (Full()) {
        ++overrun_counter_;
        return false;
    }

    construct(SlotAt(tail_), params);
    tail_ = (tail_ + 1) & slot_mask_;
    ++size_;
    return true;
}

void* CircularQueueCore::Front() noexcept {
    assert(!Empty());
    return SlotAt(head_);
}

const void* CircularQueueCore::Front() const noexcept {
    assert(!Empty());
    return SlotAt(head_);
}

void* CircularQueueCore::At(std::size_t index) noexcept {
    assert(index < Size());
    return SlotAt((head_ + index) & slot_mask_);
}

const void* CircularQueueCore::At(std::size_t index) const noexcept {
    assert(index < Size());
    return SlotAt((head_ + index) & slot_mask_);
}

void CircularQueueCore::PopFront() noexcept {
    if (Empty()) {
        return;
    }

    hooks_.destroy(SlotAt(head_));
    head_ = (head_ + 1) & slot_mask_;
    --size_;
}

bool CircularQueueCore::Empty() const noexcept {
    return size_ == 0;
}

bool CircularQueueCore::Full() const noexcept {
    return size_ == capacity_;
}

std::size_t CircularQueueCore::Size() const noexcept {
    return size_;
}

std::size_t CircularQueueCore::Capacity() const noexcept {
    return capacity_;
}

std::size_t CircularQueueCore::OverrunCounter() const noexcept {
    return overrun_counter_;
}

void* CircularQueueCore::SlotAt(std::size_t index) const noexcept {
    return static_cast<std::byte*>(storage_) + index * hooks_.slot_size;
}

std::size_t CircularQueueCore::NextSlotCount(std::size_t capacity) {
    // size_t 能表示的最大 2 的幂（最高位为 1）。
    const std::size_t max_power = (std::numeric_limits<std::size_t>::max() >> 1) + 1;
    if (capacity > max_power) {
        throw std::length_error("circular queue capacity is not representable");
    }

    std::size_t slot_count = 1;
    while (slot_count < capacity) {
        slot_count <<= 1;
    }
    return slot_count;
}

std::shared_ptr<CircularQueueCore> CircularQueueBridge::Create(std::size_t capacity, const CircularQueueHooks& hooks) {
    return std::make_shared<CircularQueueCore>(capacity, hooks);
}

void CircularQueueBridge::PushBack(CircularQueueCore& core, void* item) {
    core.PushBack(item);
}

bool CircularQueueBridge::TryPushBack(CircularQueueCore& core, void* item) {
    return core.TryPushBack(item);
}

void CircularQueueBridge::EmplaceBack(CircularQueueCore& core, void* params, CircularQueueConstructFn construct) {
    core.EmplaceBack(params, construct);
}

bool CircularQueueBridge::TryEmplaceBack(CircularQueueCore& core, void* params, CircularQueueConstructFn construct) {
    return core.TryEmplaceBack(params, construct);
}

void* CircularQueueBridge::Front(CircularQueueCore& core) noexcept {
    return core.Front();
}

const void* CircularQueueBridge::Front(const CircularQueueCore& core) noexcept {
    return core.Front();
}

void* CircularQueueBridge::At(CircularQueueCore& core, std::size_t index) noexcept {
    return core.At(index);
}

const void* CircularQueueBridge::At(const CircularQueueCore& core, std::size_t index) noexcept {
    return core.At(index);
}

void CircularQueueBridge::PopFront(CircularQueueCore& core) noexcept {
    core.PopFront();
}

bool CircularQueueBridge::Empty(const CircularQueueCore& core) noexcept {
    return core.Empty();
}

bool CircularQueueBridge::Full(const CircularQueueCore& core) noexcept {
    return core.Full();
}

std::size_t CircularQueueBridge::Size(const CircularQueueCore& core) noexcept {
    return core.Size();
}

std::size_t CircularQueueBridge::Capacity(const CircularQueueCore& core) noexcept {
    return core.Capacity();
}

std::size_t CircularQueueBridge::OverrunCounter(const CircularQueueCore& core) noexcept {
    return core.OverrunCounter();
}

}

NAMESPACE_EVEREST_END
